// SPDX-License-Identifier: MPL-2.0
// MyDesk Vault — Regression & Verification Tests
#include "mydesk/vault/vault_document.hpp"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace mydesk::vault;

static int g_failed = 0;
#define CHECK(cond) \
    do { if (!(cond)) { \
        std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #cond); \
        ++g_failed; \
    } } while(0)

// ─── SHA-256 correctness (FIPS 180-4 test vectors) ───────────────────────────

static void test_sha256_known_vectors() {
    // SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    const std::string empty_hash = VaultRepository::computeSha256("");
    CHECK(empty_hash == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // SHA-256("abc") from FIPS 180-4.
    const std::string abc_hash = VaultRepository::computeSha256("abc");
    CHECK(abc_hash == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    // SHA-256 of "The quick brown fox jumps over the lazy dog"
    const std::string fox_hash = VaultRepository::computeSha256(
        "The quick brown fox jumps over the lazy dog");
    CHECK(fox_hash == "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
}

// ─── Hash is stored in version, not fake ─────────────────────────────────────

static void test_vault_add_file_hashing() {
    VaultRepository repo;
    const auto id = repo.addFile("MotorBracket.ipt", "$/Parts/MotorBracket.ipt",
                                 "SOLID_GEOMETRY_BYTES", "user_alice", "Initial part");
    CHECK(id > 0);
    const auto* f = repo.findFile(id);
    CHECK(f != nullptr);
    CHECK(f->filename == "MotorBracket.ipt");
    CHECK(!f->versions.empty());
    // Hash must be 64 hex chars (SHA-256)
    CHECK(f->versions.front().contentHash.size() == 64);
    CHECK(f->versions.front().revision == "A");
    // Verify hash is correct
    CHECK(f->versions.front().contentHash ==
          VaultRepository::computeSha256("SOLID_GEOMETRY_BYTES"));
    // Content is preserved
    CHECK(f->versions.front().content == "SOLID_GEOMETRY_BYTES");
    // Timestamp is non-empty and contains 'T' (ISO 8601)
    CHECK(f->versions.front().timestamp.find('T') != std::string::npos);
}

// ─── Check-out / check-in with content storage ───────────────────────────────

static void test_vault_checkout_checkin_locking() {
    VaultRepository repo;
    const auto id = repo.addFile("Housing.ipt", "$/Parts/Housing.ipt", "DATA_V1", "alice");

    // Alice checks out
    CHECK(repo.checkOut(id, "alice"));
    const auto* f = repo.findFile(id);
    CHECK(f->isCheckedOut);
    CHECK(f->checkedOutBy == "alice");

    // Bob cannot check out while Alice holds lock
    std::string err;
    CHECK(!repo.checkOut(id, "bob", &err));
    CHECK(!err.empty());

    // Alice checks in updated version
    CHECK(repo.checkIn(id, "alice", "DATA_V2", "Added mounting holes"));
    CHECK(!f->isCheckedOut);
    CHECK(f->versions.size() == 2);
    CHECK(f->versions.back().versionNumber == 2);
    CHECK(f->versions.back().content == "DATA_V2");
    CHECK(f->versions.back().contentHash == VaultRepository::computeSha256("DATA_V2"));
    // Both versions stored (history preserved)
    CHECK(f->versions.front().content == "DATA_V1");
}

// ─── Undo check-out (discard working copy) ───────────────────────────────────

static void test_vault_undo_checkout() {
    VaultRepository repo;
    const auto id = repo.addFile("Shaft.ipt", "$/Parts/Shaft.ipt", "ORIGINAL_DATA", "alice");
    CHECK(repo.checkOut(id, "alice"));
    CHECK(repo.undoCheckOut(id, "alice"));
    const auto* f = repo.findFile(id);
    CHECK(!f->isCheckedOut);
    CHECK(f->versions.size() == 1);
}

// ─── Historical retrieval ────────────────────────────────────────────────────

static void test_vault_historical_retrieval() {
    VaultRepository repo;
    const auto id = repo.addFile("Part.ipt", "$/Part.ipt", "v1_data", "alice");
    CHECK(repo.checkOut(id, "alice"));
    CHECK(repo.checkIn(id, "alice", "v2_data", "second version"));

    auto v1 = repo.retrieveContent(id, 1);
    CHECK(v1.has_value());
    CHECK(*v1 == "v1_data");

    auto v2 = repo.retrieveContent(id, 2);
    CHECK(v2.has_value());
    CHECK(*v2 == "v2_data");

    auto v3 = repo.retrieveContent(id, 99);
    CHECK(!v3.has_value());
}

// ─── Rollback ────────────────────────────────────────────────────────────────

static void test_vault_rollback() {
    VaultRepository repo;
    const auto id = repo.addFile("Design.ipt", "$/Design.ipt", "rev1_data", "alice");
    CHECK(repo.checkOut(id, "alice"));
    CHECK(repo.checkIn(id, "alice", "rev2_data", "version 2"));

    // Rollback to v1 → appends a new version with v1 content
    CHECK(repo.rollback(id, 1, "alice"));
    const auto* f = repo.findFile(id);
    CHECK(f->versions.size() == 3);
    CHECK(f->versions.back().content == "rev1_data");
    CHECK(f->versions.back().comment.find("Rollback") != std::string::npos);
}

// ─── Lifecycle state machine ──────────────────────────────────────────────────

static void test_vault_lifecycle_transition() {
    VaultRepository repo;
    const auto id = repo.addFile("Flange.ipt", "$/Parts/Flange.ipt", "DATA", "alice");

    CHECK(repo.transitionState(id, LifecycleState::ForReview, "alice"));
    const auto* f = repo.findFile(id);
    CHECK(f->state == LifecycleState::ForReview);

    CHECK(repo.transitionState(id, LifecycleState::Released, "lead_eng"));
    CHECK(f->state == LifecycleState::Released);

    // Cannot check out Released file
    std::string err;
    CHECK(!repo.checkOut(id, "alice", &err));

    // Release → WIP increments revision
    CHECK(repo.transitionState(id, LifecycleState::WorkInProgress, "lead_eng"));
    CHECK(f->versions.back().revision == "B");
}

// ─── Dependencies and BOM ────────────────────────────────────────────────────

static void test_vault_assembly_dependencies_where_used() {
    VaultRepository repo;
    const auto partId = repo.addFile("Bolt_M6.ipt", "$/Standard/Bolt_M6.ipt", "BOLT", "admin");
    const auto asm1   = repo.addFile("SubAssembly.iam", "$/Assemblies/Sub.iam", "ASM1", "eng");
    const auto asm2   = repo.addFile("MainAssembly.iam", "$/Assemblies/Main.iam", "ASM2", "eng");

    repo.addDependency(asm1, partId);
    repo.addDependency(asm2, partId);

    const auto parents = repo.whereUsed(partId);
    CHECK(parents.size() == 2);
}

static void test_vault_bom_generation() {
    VaultRepository repo;
    const auto asmId  = repo.addFile("Engine.iam", "$/Assemblies/Engine.iam", "ROOT", "alice");
    const auto piston = repo.addFile("Piston.ipt", "$/Parts/Piston.ipt", "PISTON", "alice");
    const auto rod    = repo.addFile("ConnRod.ipt", "$/Parts/ConnRod.ipt", "ROD", "alice");

    repo.addDependency(asmId, piston);
    repo.addDependency(asmId, rod);

    const auto bom = repo.generateBom(asmId);
    CHECK(bom.size() == 2);
    CHECK(bom[0].filename == "Piston.ipt");
    CHECK(bom[1].filename == "ConnRod.ipt");
}

// ─── Search ──────────────────────────────────────────────────────────────────

static void test_vault_search() {
    VaultRepository repo;
    const auto id1 = repo.addFile("GearShaft.ipt", "$/Parts/GearShaft.ipt", "data", "alice");
    const auto id2 = repo.addFile("GearWheel.ipt", "$/Parts/GearWheel.ipt", "data", "bob");
    (void)repo.addFile("Bracket.ipt", "$/Parts/Bracket.ipt", "data", "charlie");

    auto gears = repo.searchByFilename("Gear");
    CHECK(gears.size() == 2);

    auto byAlice = repo.searchByAuthor("alice");
    CHECK(byAlice.size() == 1);
    CHECK(byAlice[0] == id1);
    (void)id2;
}

// ─── Statistics ──────────────────────────────────────────────────────────────

static void test_vault_statistics() {
    VaultRepository repo;
    const auto id = repo.addFile("Part.ipt", "$/Part.ipt", "content_bytes", "alice");
    CHECK(repo.checkOut(id, "alice"));
    CHECK(repo.checkIn(id, "alice", "more_content_bytes", "v2"));

    CHECK(repo.totalVersionCount() == 2);
    CHECK(repo.totalStoredBytes() == std::string("content_bytes").size() +
                                     std::string("more_content_bytes").size());
}

// ─── Native round-trip ───────────────────────────────────────────────────────

static void test_vault_native_roundtrip() {
    VaultRepository repo;
    repo.setName("Corporate \"Vault\"");
    const std::string binaryContent("DWG\0DATA\n\x01\xff", 11);
    const auto drawingId = repo.addFile("Drawing \"1\".idw", "$/Drawings/Drawing1.idw",
                                        binaryContent, "alice", "Initial\nimport");
    CHECK(repo.checkOut(drawingId, "alice"));
    CHECK(repo.checkIn(drawingId, "alice", "DWG_DATA_V2", "Second version"));
    const auto assemblyId = repo.addFile("Assembly.iam", "$/Assemblies/Assembly.iam",
                                         "ASSEMBLY", "bob");
    CHECK(repo.addDependency(assemblyId, drawingId));
    CHECK(repo.checkOut(assemblyId, "bob"));

    const auto json = repo.serializeNative();
    CHECK(!json.empty());
    CHECK(json.find("schema_version") != std::string::npos);
    CHECK(json.find("sha256") != std::string::npos);
    CHECK(json.find("content_base64") != std::string::npos);

    std::string err;
    VaultRepository loaded = VaultRepository::deserializeNative(json, &err);
    CHECK(err.empty());
    CHECK(loaded.name() == "Corporate \"Vault\"");
    CHECK(loaded.files().size() == 2);
    const auto* loadedDrawing = loaded.findFile(drawingId);
    CHECK(loadedDrawing != nullptr);
    if (loadedDrawing != nullptr) {
        CHECK(loadedDrawing->filename == "Drawing \"1\".idw");
        CHECK(loadedDrawing->versions.size() == 2);
        CHECK(loadedDrawing->versions.front().content == binaryContent);
        CHECK(loadedDrawing->versions.front().comment == "Initial\nimport");
        CHECK(loadedDrawing->versions.back().content == "DWG_DATA_V2");
    }
    const auto* loadedAssembly = loaded.findFile(assemblyId);
    CHECK(loadedAssembly != nullptr);
    if (loadedAssembly != nullptr) {
        CHECK(loadedAssembly->isCheckedOut);
        CHECK(loadedAssembly->checkedOutBy == "bob");
        CHECK(loadedAssembly->childrenDependencies.size() == 1);
        CHECK(loadedAssembly->childrenDependencies.front() == drawingId);
    }
    CHECK(loaded.auditLog().size() == repo.auditLog().size());
    CHECK(loaded.totalStoredBytes() == repo.totalStoredBytes());
}

static void test_vault_native_malformed_input() {
    std::string err;
    auto loaded = VaultRepository::deserializeNative("{broken", &err);
    CHECK(!err.empty());
    CHECK(loaded.files().empty());

    const std::string futureSchema = R"({
      "format":"mydesk-vault", "schema_version":99,
      "name":"future", "files":[]
    })";
    loaded = VaultRepository::deserializeNative(futureSchema, &err);
    CHECK(err.find("unsupported schema version") != std::string::npos);

    VaultRepository repo;
    repo.addFile("Part.ipt", "$/Part.ipt", "trusted bytes", "alice");
    auto tampered = repo.serializeNative();
    const auto hashPosition = tampered.find(VaultRepository::computeSha256("trusted bytes"));
    CHECK(hashPosition != std::string::npos);
    if (hashPosition != std::string::npos) tampered[hashPosition] = '0';
    loaded = VaultRepository::deserializeNative(tampered, &err);
    CHECK(err.find("hash validation failed") != std::string::npos);
    CHECK(loaded.files().empty());

    const std::string danglingDependency = R"({
      "format":"mydesk-vault", "schema_version":1, "name":"legacy",
      "files":[{"id":1,"name":"Part.ipt","path":"$/Part.ipt",
                "state":0,"locked":false,"deps":[2]}]
    })";
    loaded = VaultRepository::deserializeNative(danglingDependency, &err);
    CHECK(err.find("missing file ID") != std::string::npos);
}

static void test_vault_native_v1_migration() {
    const std::string legacy = R"({
      "format":"mydesk-vault",
      "name":"LegacyVault",
      "files":[
        {"id":7,"name":"Legacy.ipt","path":"$/Legacy.ipt",
         "state":2,"locked":false,"deps":[]}
      ]
    })";
    std::string err;
    auto loaded = VaultRepository::deserializeNative(legacy, &err);
    CHECK(err.empty());
    CHECK(loaded.name() == "LegacyVault");
    CHECK(loaded.files().size() == 1);
    CHECK(loaded.files().front().id == 7);
    CHECK(loaded.files().front().filename == "Legacy.ipt");
    CHECK(loaded.files().front().state == LifecycleState::Released);
    CHECK(loaded.files().front().versions.empty());

    const auto newId = loaded.addFile("New.ipt", "$/New.ipt", "new", "alice");
    CHECK(newId == 8);
}

static void test_vault_atomic_file_save_and_backup() {
    namespace fs = std::filesystem;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = fs::temp_directory_path() /
        ("mydesk-vault-atomic-" + std::to_string(stamp) + ".myvault");
    auto backup = path;
    backup += ".bak";
    auto temporary = path;
    temporary += ".tmp";

    VaultRepository repo;
    repo.setName("AtomicVault");
    repo.addFile("First.ipt", "$/First.ipt", "first", "alice");
    std::string err;
    CHECK(repo.saveNativeFile(path.string(), &err));
    CHECK(err.empty());
    CHECK(fs::exists(path));
    CHECK(!fs::exists(temporary));
    CHECK(!fs::exists(backup));

    repo.addFile("Second.ipt", "$/Second.ipt", "second", "bob");
    CHECK(repo.saveNativeFile(path.string(), &err));
    CHECK(fs::exists(backup));
    auto loaded = VaultRepository::loadNativeFile(path.string(), &err);
    CHECK(loaded.has_value());
    if (loaded.has_value()) CHECK(loaded->files().size() == 2);
    auto previous = VaultRepository::loadNativeFile(backup.string(), &err);
    CHECK(previous.has_value());
    if (previous.has_value()) CHECK(previous->files().size() == 1);

    {
        std::ofstream corrupt(path, std::ios::binary | std::ios::trunc);
        corrupt << "not json";
    }
    loaded = VaultRepository::loadNativeFile(path.string(), &err);
    CHECK(!loaded.has_value());
    CHECK(!err.empty());
    previous = VaultRepository::loadNativeFile(backup.string(), &err);
    CHECK(previous.has_value());

    std::error_code cleanupError;
    fs::remove(path, cleanupError);
    fs::remove(backup, cleanupError);
    fs::remove(temporary, cleanupError);
}

// ─── Audit log ───────────────────────────────────────────────────────────────

static void test_vault_audit_log() {
    VaultRepository repo;
    const auto id = repo.addFile("Part.ipt", "$/Part.ipt", "data", "alice");
    CHECK(repo.checkOut(id, "alice"));
    CHECK(repo.checkIn(id, "alice", "data_v2", "update"));

    const auto& log = repo.auditLog();
    CHECK(log.size() >= 3); // ADD_FILE, CHECK_OUT, CHECK_IN
    // All entries must have non-empty ISO timestamps
    for (const auto& e : log) {
        CHECK(!e.timestamp.empty());
        CHECK(e.timestamp.find('T') != std::string::npos);
    }
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int main() {
    // Registered capability test identifiers:
    // "test_vault_add_file_hashing"
    // "test_vault_checkout_checkin_locking"
    // "test_vault_undo_checkout"
    // "test_vault_lifecycle_transition"
    // "test_vault_assembly_dependencies_where_used"
    // "test_vault_bom_generation"
    // "test_vault_native_roundtrip"
    // "test_vault_native_malformed_input"
    // "test_vault_native_v1_migration"
    // "test_vault_atomic_file_save_and_backup"
    test_sha256_known_vectors();
    test_vault_add_file_hashing();
    test_vault_checkout_checkin_locking();
    test_vault_undo_checkout();
    test_vault_historical_retrieval();
    test_vault_rollback();
    test_vault_lifecycle_transition();
    test_vault_assembly_dependencies_where_used();
    test_vault_bom_generation();
    test_vault_search();
    test_vault_statistics();
    test_vault_native_roundtrip();
    test_vault_native_malformed_input();
    test_vault_native_v1_migration();
    test_vault_atomic_file_save_and_backup();
    test_vault_audit_log();

    if (g_failed == 0) {
        std::printf("All Vault tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d Vault test(s) FAILED.\n", g_failed);
    return 1;
}
