// SPDX-License-Identifier: MPL-2.0
// MyDesk Vault — Engineering PDM engine implementation
//
// SHA-256 implemented per FIPS PUB 180-4 (clean-room, no third-party code).
// Timestamps use std::chrono::system_clock for genuine UTC wall time.

#include "mydesk/vault/vault_document.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace mydesk::vault {

// ─── SHA-256 (FIPS 180-4 clean-room implementation) ──────────────────────────

namespace {

// Round constants K[0..63]
constexpr std::array<std::uint32_t, 64> SHA256_K = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// Initial hash values H[0..7]
constexpr std::array<std::uint32_t, 8> SHA256_H0 = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

inline std::uint32_t rotr32(std::uint32_t x, unsigned n) noexcept {
    return (x >> n) | (x << (32u - n));
}

std::array<std::uint32_t, 8> sha256_process(const std::uint8_t* data, std::size_t len) {
    auto state = SHA256_H0;

    // Total bit length (for padding)
    const std::uint64_t bitlen = static_cast<std::uint64_t>(len) * 8u;

    // Padded message length: append 0x80, then 0x00 bytes, then 8-byte big-endian bit count.
    // Padded size = next multiple of 64 that fits (len + 1 + 8) bytes.
    const std::size_t padded = ((len + 9 + 63) / 64) * 64;
    std::vector<std::uint8_t> msg(padded, 0);
    std::memcpy(msg.data(), data, len);
    msg[len] = 0x80;
    // Store bit length as big-endian uint64 in last 8 bytes
    for (int i = 0; i < 8; ++i) {
        msg[padded - 8 + static_cast<std::size_t>(i)] =
            static_cast<std::uint8_t>(bitlen >> (56u - static_cast<unsigned>(i) * 8u));
    }

    // Process each 64-byte block
    for (std::size_t blk = 0; blk < padded; blk += 64) {
        // Prepare message schedule W[0..63]
        std::array<std::uint32_t, 64> W{};
        for (int i = 0; i < 16; ++i) {
            const std::uint8_t* b = msg.data() + blk + static_cast<std::size_t>(i) * 4;
            W[static_cast<std::size_t>(i)] =
                (static_cast<std::uint32_t>(b[0]) << 24) |
                (static_cast<std::uint32_t>(b[1]) << 16) |
                (static_cast<std::uint32_t>(b[2]) << 8)  |
                (static_cast<std::uint32_t>(b[3]));
        }
        for (std::size_t i = 16; i < 64; ++i) {
            std::uint32_t s0 = rotr32(W[i-15], 7) ^ rotr32(W[i-15], 18) ^ (W[i-15] >> 3);
            std::uint32_t s1 = rotr32(W[i-2], 17) ^ rotr32(W[i-2], 19)  ^ (W[i-2] >> 10);
            W[i] = W[i-16] + s0 + W[i-7] + s1;
        }

        auto [a,b,c,d,e,f,g,h] = state;
        for (std::size_t i = 0; i < 64; ++i) {
            std::uint32_t S1    = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
            std::uint32_t ch    = (e & f) ^ (~e & g);
            std::uint32_t temp1 = h + S1 + ch + SHA256_K[i] + W[i];
            std::uint32_t S0    = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
            std::uint32_t maj   = (a & b) ^ (a & c) ^ (b & c);
            std::uint32_t temp2 = S0 + maj;
            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }
    return state;
}

using Json = nlohmann::json;

constexpr int kNativeSchemaVersion = 2;
constexpr std::string_view kBase64Alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string encodeBase64(std::string_view bytes) {
    std::string encoded;
    encoded.reserve(((bytes.size() + 2U) / 3U) * 4U);
    std::uint32_t accumulator = 0;
    int bits = -6;
    for (const char character : bytes) {
        const auto byte = static_cast<unsigned char>(character);
        accumulator = (accumulator << 8U) | byte;
        bits += 8;
        while (bits >= 0) {
            encoded.push_back(kBase64Alphabet[(accumulator >> static_cast<unsigned>(bits)) & 0x3fU]);
            bits -= 6;
        }
    }
    if (bits > -6) {
        encoded.push_back(kBase64Alphabet[(accumulator << static_cast<unsigned>(-bits)) & 0x3fU]);
    }
    while ((encoded.size() % 4U) != 0U) encoded.push_back('=');
    return encoded;
}

std::string decodeBase64(std::string_view encoded) {
    if ((encoded.size() % 4U) != 0U) throw std::runtime_error("base64 length is not a multiple of four");
    const auto firstPadding = encoded.find('=');
    if (firstPadding != std::string_view::npos) {
        const auto paddingCount = encoded.size() - firstPadding;
        if (paddingCount > 2U || firstPadding < encoded.size() - 2U ||
            encoded.find_first_not_of('=', firstPadding) != std::string_view::npos) {
            throw std::runtime_error("invalid base64 padding");
        }
    }
    std::array<int, 256> values{};
    values.fill(-1);
    for (std::size_t i = 0; i < kBase64Alphabet.size(); ++i) {
        values[static_cast<unsigned char>(kBase64Alphabet[i])] = static_cast<int>(i);
    }
    std::string decoded;
    decoded.reserve((encoded.size() / 4U) * 3U);
    std::uint32_t accumulator = 0;
    int bits = -8;
    bool padding = false;
    for (const char character : encoded) {
        const auto byte = static_cast<unsigned char>(character);
        if (character == '=') {
            padding = true;
            continue;
        }
        if (padding || values[byte] < 0) throw std::runtime_error("invalid base64 payload");
        accumulator = (accumulator << 6U) | static_cast<std::uint32_t>(values[byte]);
        bits += 6;
        if (bits >= 0) {
            decoded.push_back(static_cast<char>((accumulator >> static_cast<unsigned>(bits)) & 0xffU));
            bits -= 8;
        }
    }
    return decoded;
}

LifecycleState parseLifecycleState(const Json& value) {
    if (value.is_number_integer()) {
        const auto raw = value.get<int>();
        if (raw < static_cast<int>(LifecycleState::WorkInProgress) ||
            raw > static_cast<int>(LifecycleState::Obsolete)) {
            throw std::runtime_error("lifecycle state is outside the supported range");
        }
        return static_cast<LifecycleState>(raw);
    }
    if (!value.is_string()) throw std::runtime_error("lifecycle state must be a string");
    const auto state = value.get<std::string>();
    if (state == "WorkInProgress") return LifecycleState::WorkInProgress;
    if (state == "ForReview") return LifecycleState::ForReview;
    if (state == "Released") return LifecycleState::Released;
    if (state == "Obsolete") return LifecycleState::Obsolete;
    throw std::runtime_error("unknown lifecycle state '" + state + "'");
}

bool isSha256(std::string_view hash) {
    return hash.size() == 64U && std::all_of(hash.begin(), hash.end(), [](const char character) {
        return std::isxdigit(static_cast<unsigned char>(character)) != 0;
    });
}

std::string nextRevisionString(std::string_view cur) {
    if (cur.empty()) return "A";
    char last = cur.back();
    if (last >= 'A' && last < 'Z') {
        std::string next(cur);
        next.back() = static_cast<char>(last + 1);
        return next;
    }
    return std::string(cur) + "A";
}

} // anonymous namespace

// ─── Public SHA-256 API ───────────────────────────────────────────────────────

std::string VaultRepository::computeSha256(std::string_view content) {
    auto state = sha256_process(
        reinterpret_cast<const std::uint8_t*>(content.data()), content.size());
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (auto w : state) ss << std::setw(8) << w;
    return ss.str();
}

// ─── UTC timestamp ────────────────────────────────────────────────────────────

std::string VaultRepository::utcTimestampNow() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto tt  = system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &tt);
#else
    gmtime_r(&tt, &utc);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return std::string(buf);
}

// ─── Lifecycle name ───────────────────────────────────────────────────────────

const char* lifecycleStateName(LifecycleState s) {
    switch (s) {
        case LifecycleState::WorkInProgress: return "WorkInProgress";
        case LifecycleState::ForReview:      return "ForReview";
        case LifecycleState::Released:       return "Released";
        case LifecycleState::Obsolete:       return "Obsolete";
    }
    return "Unknown";
}

// ─── VaultRepository ─────────────────────────────────────────────────────────

VaultRepository::VaultRepository() = default;

void VaultRepository::logAudit(std::string username, std::string action, std::string details) {
    AuditLogEntry entry;
    entry.timestamp = utcTimestampNow();
    entry.username  = std::move(username);
    entry.action    = std::move(action);
    entry.details   = std::move(details);
    auditLog_.push_back(std::move(entry));
}

FileId VaultRepository::addFile(std::string filename, std::string vaultPath,
                                std::string initialContent, std::string author,
                                std::string comment) {
    VaultFile file;
    file.id        = nextFileId_++;
    file.filename  = std::move(filename);
    file.vaultPath = std::move(vaultPath);
    file.state     = LifecycleState::WorkInProgress;
    file.isCheckedOut = false;

    FileVersion v;
    v.versionNumber = 1;
    v.revision      = "A";
    v.contentHash   = computeSha256(initialContent);
    v.content       = std::move(initialContent);
    v.author        = author;
    v.comment       = std::move(comment);
    v.timestamp     = utcTimestampNow();
    file.versions.push_back(std::move(v));

    logAudit(author, "ADD_FILE", file.filename + " (sha256:" + file.versions.front().contentHash.substr(0,8) + "...)");
    files_.push_back(std::move(file));
    return files_.back().id;
}

const VaultFile* VaultRepository::findFile(FileId id) const {
    for (const auto& f : files_)
        if (f.id == id) return &f;
    return nullptr;
}

VaultFile* VaultRepository::findFileMut(FileId id) {
    for (auto& f : files_)
        if (f.id == id) return &f;
    return nullptr;
}

const VaultFile* VaultRepository::findFileByPath(std::string_view path) const {
    for (const auto& f : files_)
        if (f.vaultPath == path) return &f;
    return nullptr;
}

bool VaultRepository::updateFileProperties(FileId id, std::string filename,
                                           std::string vaultPath,
                                           std::string_view username,
                                           std::string* error) {
    if (error) error->clear();
    auto* file = findFileMut(id);
    if (!file) { if (error) *error = "file not found"; return false; }
    if (filename.empty() || vaultPath.empty()) {
        if (error) *error = "file name and vault path must not be empty";
        return false;
    }
    if (const auto* existing = findFileByPath(vaultPath); existing && existing->id != id) {
        if (error) *error = "another file already uses that vault path";
        return false;
    }
    const auto oldPath = file->vaultPath;
    file->filename = std::move(filename);
    file->vaultPath = std::move(vaultPath);
    logAudit(std::string(username), "UPDATE_PROPERTIES",
             oldPath + " → " + file->vaultPath);
    return true;
}

bool VaultRepository::checkOut(FileId id, std::string_view username, std::string* error) {
    auto* f = findFileMut(id);
    if (!f) { if (error) *error = "file not found"; return false; }
    if (f->isCheckedOut) {
        if (error) *error = "file is already checked out by " + f->checkedOutBy;
        return false;
    }
    if (f->state == LifecycleState::Released) {
        if (error) *error = "file is Released — must transition to WIP before editing";
        return false;
    }
    f->isCheckedOut  = true;
    f->checkedOutBy  = std::string(username);
    logAudit(std::string(username), "CHECK_OUT", f->filename);
    return true;
}

bool VaultRepository::checkIn(FileId id, std::string_view username, std::string newContent,
                              std::string comment, std::string* error) {
    auto* f = findFileMut(id);
    if (!f) { if (error) *error = "file not found"; return false; }
    if (!f->isCheckedOut) { if (error) *error = "file is not checked out"; return false; }
    if (f->checkedOutBy != username) {
        if (error) *error = "file was checked out by " + f->checkedOutBy + ", not " + std::string(username);
        return false;
    }

    FileVersion v;
    v.versionNumber = f->versions.size() + 1;
    v.revision      = f->versions.empty() ? "A" : f->versions.back().revision;
    v.contentHash   = computeSha256(newContent);
    v.content       = std::move(newContent);
    v.author        = std::string(username);
    v.comment       = std::move(comment);
    v.timestamp     = utcTimestampNow();

    f->versions.push_back(std::move(v));
    f->isCheckedOut = false;
    f->checkedOutBy.clear();

    logAudit(std::string(username), "CHECK_IN",
             f->filename + " v" + std::to_string(f->versions.size()) +
             " (sha256:" + f->versions.back().contentHash.substr(0,8) + "...)");
    return true;
}

bool VaultRepository::undoCheckOut(FileId id, std::string_view username, std::string* error) {
    auto* f = findFileMut(id);
    if (!f) { if (error) *error = "file not found"; return false; }
    if (!f->isCheckedOut) { if (error) *error = "file is not checked out"; return false; }
    if (f->checkedOutBy != username) {
        if (error) *error = "file was checked out by " + f->checkedOutBy;
        return false;
    }
    f->isCheckedOut = false;
    f->checkedOutBy.clear();
    logAudit(std::string(username), "UNDO_CHECK_OUT", f->filename);
    return true;
}

bool VaultRepository::transitionState(FileId id, LifecycleState newState,
                                      std::string_view username, std::string* error) {
    auto* f = findFileMut(id);
    if (!f) { if (error) *error = "file not found"; return false; }
    if (f->isCheckedOut) {
        if (error) *error = "cannot transition lifecycle while checked out";
        return false;
    }
    const auto oldState = f->state;
    f->state = newState;

    // Transitioning from Released back to WIP increments the revision letter
    if (oldState == LifecycleState::Released &&
        newState == LifecycleState::WorkInProgress && !f->versions.empty()) {
        f->versions.back().revision = nextRevisionString(f->versions.back().revision);
    }

    logAudit(std::string(username), "STATE_CHANGE",
             f->filename + ": " + lifecycleStateName(oldState) +
             " → " + lifecycleStateName(newState));
    return true;
}

std::optional<std::string> VaultRepository::retrieveContent(FileId id,
                                                              std::size_t versionNumber) const {
    const auto* f = findFile(id);
    if (!f) return std::nullopt;
    const auto* v = f->versionByNumber(versionNumber);
    if (!v) return std::nullopt;
    return v->content;
}

bool VaultRepository::rollback(FileId id, std::size_t targetVersionNumber,
                               std::string_view username, std::string* error) {
    auto* f = findFileMut(id);
    if (!f) { if (error) *error = "file not found"; return false; }
    if (f->isCheckedOut) { if (error) *error = "file is checked out"; return false; }
    const auto* target = f->versionByNumber(targetVersionNumber);
    if (!target) { if (error) *error = "version not found"; return false; }

    // Append a new version that is a copy of the target (preserves history)
    FileVersion v;
    v.versionNumber = f->versions.size() + 1;
    v.revision      = f->versions.back().revision;
    v.contentHash   = target->contentHash;
    v.content       = target->content;
    v.author        = std::string(username);
    v.comment       = "Rollback to version " + std::to_string(targetVersionNumber);
    v.timestamp     = utcTimestampNow();
    f->versions.push_back(std::move(v));

    logAudit(std::string(username), "ROLLBACK",
             f->filename + " → v" + std::to_string(targetVersionNumber));
    return true;
}

bool VaultRepository::addDependency(FileId parentId, FileId childId) {
    auto* f = findFileMut(parentId);
    if (!f || !findFile(childId) || parentId == childId) return false;
    if (std::find(f->childrenDependencies.begin(), f->childrenDependencies.end(), childId)
        != f->childrenDependencies.end()) return false;

    // Reject dependency cycles. A cyclic assembly graph cannot produce a
    // deterministic BOM and is invalid in the native repository schema.
    std::vector<FileId> pending{childId};
    std::unordered_set<FileId> visited;
    while (!pending.empty()) {
        const auto current = pending.back();
        pending.pop_back();
        if (current == parentId) return false;
        if (!visited.insert(current).second) continue;
        if (const auto* candidate = findFile(current)) {
            pending.insert(pending.end(), candidate->childrenDependencies.begin(),
                           candidate->childrenDependencies.end());
        }
    }
    f->childrenDependencies.push_back(childId);
    logAudit("system", "ADD_DEPENDENCY",
             std::to_string(parentId) + " → " + std::to_string(childId));
    return true;
}

bool VaultRepository::removeDependency(FileId parentId, FileId childId) {
    auto* file = findFileMut(parentId);
    if (!file) return false;
    const auto found = std::find(file->childrenDependencies.begin(),
                                 file->childrenDependencies.end(), childId);
    if (found == file->childrenDependencies.end()) return false;
    file->childrenDependencies.erase(found);
    logAudit("system", "REMOVE_DEPENDENCY",
             std::to_string(parentId) + " → " + std::to_string(childId));
    return true;
}

std::vector<FileId> VaultRepository::whereUsed(FileId childId) const {
    std::vector<FileId> parents;
    std::vector<FileId> pending{childId};
    std::unordered_set<FileId> visited{childId};
    while (!pending.empty()) {
        const auto dependency = pending.back();
        pending.pop_back();
        for (const auto& file : files_) {
            if (std::find(file.childrenDependencies.begin(), file.childrenDependencies.end(), dependency)
                    == file.childrenDependencies.end() || !visited.insert(file.id).second) continue;
            parents.push_back(file.id);
            pending.push_back(file.id);
        }
    }
    return parents;
}

std::vector<BomItem> VaultRepository::generateBom(FileId assemblyId) const {
    std::vector<BomItem> bom;
    const auto* parent = findFile(assemblyId);
    if (!parent) return bom;
    std::vector<FileId> pending(parent->childrenDependencies.rbegin(),
                                parent->childrenDependencies.rend());
    while (!pending.empty()) {
        const auto childId = pending.back();
        pending.pop_back();
        const auto* child = findFile(childId);
        if (!child) continue;
        const auto existing = std::find_if(bom.begin(), bom.end(), [childId](const BomItem& item) {
            return item.fileId == childId;
        });
        if (existing == bom.end()) {
            BomItem item;
            item.fileId = child->id;
            item.filename = child->filename;
            item.revision = child->versions.empty() ? "A" : child->versions.back().revision;
            item.quantity = 1;
            bom.push_back(std::move(item));
        } else {
            ++existing->quantity;
        }
        pending.insert(pending.end(), child->childrenDependencies.rbegin(),
                       child->childrenDependencies.rend());
    }
    return bom;
}

std::vector<FileId> VaultRepository::searchByFilename(std::string_view query) const {
    std::vector<FileId> results;
    for (const auto& f : files_)
        if (f.filename.find(query) != std::string::npos)
            results.push_back(f.id);
    return results;
}

std::vector<FileId> VaultRepository::searchByAuthor(std::string_view author) const {
    std::vector<FileId> results;
    for (const auto& f : files_)
        for (const auto& v : f.versions)
            if (v.author == author) { results.push_back(f.id); break; }
    return results;
}

std::size_t VaultRepository::totalVersionCount() const {
    std::size_t n = 0;
    for (const auto& f : files_) n += f.versions.size();
    return n;
}

std::size_t VaultRepository::totalStoredBytes() const {
    std::size_t n = 0;
    for (const auto& f : files_)
        for (const auto& v : f.versions) n += v.content.size();
    return n;
}

// ─── Native persistence ───────────────────────────────────────────────────────

std::string VaultRepository::serializeNative() const {
    Json root = Json::object();
    root["format"] = "mydesk-vault";
    root["schema_version"] = kNativeSchemaVersion;
    root["name"] = name_;
    root["files"] = Json::array();
    for (const auto& file : files_) {
        Json serializedFile = {
            {"id", file.id},
            {"filename", file.filename},
            {"path", file.vaultPath},
            {"state", lifecycleStateName(file.state)},
            {"locked", file.isCheckedOut},
            {"locked_by", file.checkedOutBy},
            {"deps", file.childrenDependencies},
            {"versions", Json::array()}
        };
        for (const auto& version : file.versions) {
            serializedFile["versions"].push_back({
                {"version", version.versionNumber},
                {"revision", version.revision},
                {"sha256", version.contentHash},
                {"author", version.author},
                {"comment", version.comment},
                {"timestamp", version.timestamp},
                {"content_encoding", "base64"},
                {"content_base64", encodeBase64(version.content)}
            });
        }
        root["files"].push_back(std::move(serializedFile));
    }

    root["audit_log"] = Json::array();
    for (const auto& entry : auditLog_) {
        root["audit_log"].push_back({
            {"timestamp", entry.timestamp},
            {"username", entry.username},
            {"action", entry.action},
            {"details", entry.details}
        });
    }
    return root.dump(2) + '\n';
}

VaultRepository VaultRepository::deserializeNative(std::string_view json, std::string* error) {
    if (error) error->clear();
    VaultRepository repo;
    try {
        const Json root = Json::parse(json.begin(), json.end());
        if (!root.is_object()) throw std::runtime_error("repository root must be a JSON object");
        if (root.at("format").get<std::string>() != "mydesk-vault") {
            throw std::runtime_error("not a mydesk-vault document");
        }
        const int schemaVersion = root.value("schema_version", 1);
        if (schemaVersion < 1 || schemaVersion > kNativeSchemaVersion) {
            throw std::runtime_error("unsupported schema version " + std::to_string(schemaVersion));
        }
        repo.name_ = root.at("name").get<std::string>();
        if (repo.name_.empty()) throw std::runtime_error("repository name must not be empty");

        const auto& serializedFiles = root.at("files");
        if (!serializedFiles.is_array()) throw std::runtime_error("files must be an array");
        std::unordered_set<FileId> fileIds;
        for (const auto& serializedFile : serializedFiles) {
            if (!serializedFile.is_object()) throw std::runtime_error("each file must be an object");
            VaultFile file;
            file.id = serializedFile.at("id").get<FileId>();
            if (file.id == 0 || !fileIds.insert(file.id).second) {
                throw std::runtime_error("file IDs must be unique positive integers");
            }
            const char* filenameKey = serializedFile.contains("filename") ? "filename" : "name";
            file.filename = serializedFile.at(filenameKey).get<std::string>();
            file.vaultPath = serializedFile.at("path").get<std::string>();
            if (file.filename.empty() || file.vaultPath.empty()) {
                throw std::runtime_error("file name and vault path must not be empty");
            }
            file.state = parseLifecycleState(serializedFile.at("state"));
            file.isCheckedOut = serializedFile.value("locked", false);
            file.checkedOutBy = serializedFile.value("locked_by", std::string{});
            if (file.isCheckedOut != !file.checkedOutBy.empty()) {
                throw std::runtime_error("checked-out state and lock owner are inconsistent");
            }

            if (serializedFile.contains("deps")) {
                file.childrenDependencies = serializedFile.at("deps").get<std::vector<FileId>>();
                std::unordered_set<FileId> dependencies;
                for (const auto dependency : file.childrenDependencies) {
                    if (dependency == 0 || !dependencies.insert(dependency).second) {
                        throw std::runtime_error("dependencies must contain unique positive file IDs");
                    }
                }
            }

            if (serializedFile.contains("versions")) {
                const auto& serializedVersions = serializedFile.at("versions");
                if (!serializedVersions.is_array()) throw std::runtime_error("versions must be an array");
                std::size_t expectedVersion = 1;
                for (const auto& serializedVersion : serializedVersions) {
                    FileVersion version;
                    version.versionNumber = serializedVersion.at("version").get<std::size_t>();
                    if (version.versionNumber != expectedVersion++) {
                        throw std::runtime_error("version numbers must be contiguous and start at one");
                    }
                    version.revision = serializedVersion.at("revision").get<std::string>();
                    version.contentHash = serializedVersion.at("sha256").get<std::string>();
                    version.author = serializedVersion.at("author").get<std::string>();
                    version.comment = serializedVersion.at("comment").get<std::string>();
                    version.timestamp = serializedVersion.at("timestamp").get<std::string>();
                    if (serializedVersion.value("content_encoding", std::string{}) != "base64") {
                        throw std::runtime_error("unsupported version content encoding");
                    }
                    version.content = decodeBase64(serializedVersion.at("content_base64").get<std::string>());
                    if (version.revision.empty() || version.author.empty() || version.timestamp.empty()) {
                        throw std::runtime_error("version revision, author, and timestamp must not be empty");
                    }
                    if (!isSha256(version.contentHash) || computeSha256(version.content) != version.contentHash) {
                        throw std::runtime_error("version content hash validation failed");
                    }
                    file.versions.push_back(std::move(version));
                }
            }
            if (schemaVersion >= 2 && file.versions.empty()) {
                throw std::runtime_error("schema version 2 files must contain at least one version");
            }
            repo.nextFileId_ = std::max(repo.nextFileId_, file.id + 1U);
            repo.files_.push_back(std::move(file));
        }

        for (const auto& file : repo.files_) {
            for (const auto dependency : file.childrenDependencies) {
                if (!fileIds.contains(dependency)) {
                    throw std::runtime_error("dependency references a missing file ID");
                }
                if (dependency == file.id) throw std::runtime_error("a file cannot depend on itself");
            }
        }

        if (root.contains("audit_log")) {
            const auto& serializedAudit = root.at("audit_log");
            if (!serializedAudit.is_array()) throw std::runtime_error("audit_log must be an array");
            for (const auto& serializedEntry : serializedAudit) {
                AuditLogEntry entry;
                entry.timestamp = serializedEntry.at("timestamp").get<std::string>();
                entry.username = serializedEntry.at("username").get<std::string>();
                entry.action = serializedEntry.at("action").get<std::string>();
                entry.details = serializedEntry.at("details").get<std::string>();
                if (entry.timestamp.empty() || entry.username.empty() || entry.action.empty()) {
                    throw std::runtime_error("audit entries require timestamp, username, and action");
                }
                repo.auditLog_.push_back(std::move(entry));
            }
        }
        return repo;
    } catch (const std::exception& exception) {
        if (error) *error = std::string("invalid mydesk-vault document: ") + exception.what();
        return VaultRepository{};
    }
}

bool VaultRepository::saveNativeFile(std::string_view path, std::string* error) const {
    if (error) error->clear();
    namespace fs = std::filesystem;
    const fs::path destination{std::string(path)};
    if (destination.empty()) {
        if (error) *error = "repository path must not be empty";
        return false;
    }
    fs::path temporary = destination;
    temporary += ".tmp";
    fs::path backup = destination;
    backup += ".bak";

    std::error_code filesystemError;
    fs::remove(temporary, filesystemError);
    filesystemError.clear();
    const auto serialized = serializeNative();
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            if (error) *error = "cannot create temporary repository file: " + temporary.string();
            return false;
        }
        output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
        output.flush();
        if (!output.good()) {
            output.close();
            fs::remove(temporary, filesystemError);
            if (error) *error = "failed while writing temporary repository file";
            return false;
        }
    }

    std::string validationError;
    const auto validated = loadNativeFile(temporary.string(), &validationError);
    if (!validated.has_value()) {
        fs::remove(temporary, filesystemError);
        if (error) *error = "temporary repository validation failed: " + validationError;
        return false;
    }

    const bool replacing = fs::exists(destination, filesystemError);
    if (filesystemError) {
        fs::remove(temporary, filesystemError);
        if (error) *error = "cannot inspect destination repository file";
        return false;
    }
    if (replacing) {
        fs::remove(backup, filesystemError);
        filesystemError.clear();
        fs::rename(destination, backup, filesystemError);
        if (filesystemError) {
            fs::remove(temporary, filesystemError);
            if (error) *error = "cannot preserve previous repository as backup";
            return false;
        }
    }

    fs::rename(temporary, destination, filesystemError);
    if (filesystemError) {
        const auto commitError = filesystemError.message();
        filesystemError.clear();
        if (replacing) fs::rename(backup, destination, filesystemError);
        filesystemError.clear();
        fs::remove(temporary, filesystemError);
        if (error) *error = "cannot commit repository file: " + commitError;
        return false;
    }
    return true;
}

std::optional<VaultRepository> VaultRepository::loadNativeFile(std::string_view path,
                                                                std::string* error) {
    if (error) error->clear();
    const std::filesystem::path source{std::string(path)};
    std::ifstream input(source, std::ios::binary);
    if (!input) {
        if (error) *error = "cannot open repository file: " + source.string();
        return std::nullopt;
    }
    const std::string serialized{std::istreambuf_iterator<char>(input),
                                 std::istreambuf_iterator<char>()};
    if (!input.good() && !input.eof()) {
        if (error) *error = "failed while reading repository file: " + source.string();
        return std::nullopt;
    }
    std::string parseError;
    auto repository = deserializeNative(serialized, &parseError);
    if (!parseError.empty()) {
        if (error) *error = std::move(parseError);
        return std::nullopt;
    }
    return repository;
}

} // namespace mydesk::vault
