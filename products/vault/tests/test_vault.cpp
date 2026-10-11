// SPDX-License-Identifier: MPL-2.0
// MyDesk Vault — Regression & Verification Tests
#include "mydesk/vault/vault_document.hpp"

#include <cassert>
#include <cstdio>
#include <string>

using namespace mydesk::vault;

static int g_failed = 0;
#define CHECK(cond) \
    do { if (!(cond)) { \
        std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #cond); \
        ++g_failed; \
    } } while(0)

static void test_vault_add_file_hashing() {
    VaultRepository repo;
    const auto id = repo.addFile("MotorBracket.ipt", "$/Parts/MotorBracket.ipt",
                                 "SOLID_GEOMETRY_BYTES", "user_alice", "Initial part");
    CHECK(id > 0);
    const auto* f = repo.findFile(id);
    CHECK(f != nullptr);
    CHECK(f->filename == "MotorBracket.ipt");
    CHECK(!f->versions.empty());
    CHECK(!f->versions.front().contentHash.empty());
    CHECK(f->versions.front().revision == "A");
}

static void test_vault_checkout_checkin_locking() {
    VaultRepository repo;
    const auto id = repo.addFile("Housing.ipt", "$/Parts/Housing.ipt", "DATA_V1", "alice");

    // Alice checks out
    CHECK(repo.checkOut(id, "alice"));
    const auto* f = repo.findFile(id);
    CHECK(f->isCheckedOut);
    CHECK(f->checkedOutBy == "alice");

    // Bob attempts to check out while Alice holds lock -> should fail
    std::string err;
    CHECK(!repo.checkOut(id, "bob", &err));
    CHECK(!err.empty());

    // Alice checks in updated version
    CHECK(repo.checkIn(id, "alice", "DATA_V2", "Added mounting holes"));
    CHECK(!f->isCheckedOut);
    CHECK(f->versions.size() == 2);
    CHECK(f->versions.back().versionNumber == 2);
}

static void test_vault_undo_checkout() {
    VaultRepository repo;
    const auto id = repo.addFile("Shaft.ipt", "$/Parts/Shaft.ipt", "ORIGINAL_DATA", "alice");
    CHECK(repo.checkOut(id, "alice"));

    CHECK(repo.undoCheckOut(id, "alice"));
    const auto* f = repo.findFile(id);
    CHECK(!f->isCheckedOut);
    CHECK(f->versions.size() == 1);
}

static void test_vault_lifecycle_transition() {
    VaultRepository repo;
    const auto id = repo.addFile("Flange.ipt", "$/Parts/Flange.ipt", "DATA", "alice");

    // Transition to ForReview
    CHECK(repo.transitionState(id, LifecycleState::ForReview, "alice"));
    const auto* f = repo.findFile(id);
    CHECK(f->state == LifecycleState::ForReview);

    // Transition to Released
    CHECK(repo.transitionState(id, LifecycleState::Released, "lead_eng"));
    CHECK(f->state == LifecycleState::Released);

    // Cannot check out Released file directly
    std::string err;
    CHECK(!repo.checkOut(id, "alice", &err));

    // Transition back to WorkInProgress for engineering change order -> revision increments to B
    CHECK(repo.transitionState(id, LifecycleState::WorkInProgress, "lead_eng"));
    CHECK(f->versions.back().revision == "B");
}

static void test_vault_assembly_dependencies_where_used() {
    VaultRepository repo;
    const auto partId = repo.addFile("Bolt_M6.ipt", "$/Standard/Bolt_M6.ipt", "BOLT", "admin");
    const auto asm1 = repo.addFile("SubAssembly.iam", "$/Assemblies/SubAssembly.iam", "ASM1", "eng");
    const auto asm2 = repo.addFile("MainAssembly.iam", "$/Assemblies/MainAssembly.iam", "ASM2", "eng");

    repo.addDependency(asm1, partId);
    repo.addDependency(asm2, partId);

    const auto parents = repo.whereUsed(partId);
    CHECK(parents.size() == 2);
}

static void test_vault_bom_generation() {
    VaultRepository repo;
    const auto asmId = repo.addFile("Engine.iam", "$/Assemblies/Engine.iam", "ROOT", "alice");
    const auto piston = repo.addFile("Piston.ipt", "$/Parts/Piston.ipt", "PISTON", "alice");
    const auto rod = repo.addFile("ConnectingRod.ipt", "$/Parts/ConnectingRod.ipt", "ROD", "alice");

    repo.addDependency(asmId, piston);
    repo.addDependency(asmId, rod);

    const auto bom = repo.generateBom(asmId);
    CHECK(bom.size() == 2);
    CHECK(bom[0].filename == "Piston.ipt");
    CHECK(bom[1].filename == "ConnectingRod.ipt");
}

static void test_vault_native_roundtrip() {
    VaultRepository repo;
    repo.setName("CorporateVault");
    repo.addFile("Drawing1.idw", "$/Drawings/Drawing1.idw", "DWG_DATA", "alice");

    const auto json = repo.serializeNative();
    CHECK(!json.empty());

    std::string err;
    VaultRepository loaded = VaultRepository::deserializeNative(json, &err);
    CHECK(err.empty());
    CHECK(loaded.name() == "CorporateVault");
    CHECK(loaded.files().size() == 1);
    CHECK(loaded.files().front().filename == "Drawing1.idw");
}

int main() {
    // Registered capability test identifiers:
    // "test_vault_add_file_hashing"
    test_vault_add_file_hashing();
    // "test_vault_checkout_checkin_locking"
    test_vault_checkout_checkin_locking();
    // "test_vault_undo_checkout"
    test_vault_undo_checkout();
    // "test_vault_lifecycle_transition"
    test_vault_lifecycle_transition();
    // "test_vault_assembly_dependencies_where_used"
    test_vault_assembly_dependencies_where_used();
    // "test_vault_bom_generation"
    test_vault_bom_generation();
    // "test_vault_native_roundtrip"
    test_vault_native_roundtrip();

    if (g_failed == 0) {
        std::printf("All Vault tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d Vault test(s) FAILED.\n", g_failed);
    return 1;
}
