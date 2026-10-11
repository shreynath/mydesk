// SPDX-License-Identifier: MPL-2.0
// MyDesk Vault — Engineering PDM document model
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mydesk::vault {

using FileId = std::uint64_t;

enum class LifecycleState {
    WorkInProgress,
    ForReview,
    Released,
    Obsolete
};

/// Returns the string name of a lifecycle state for display/serialization.
const char* lifecycleStateName(LifecycleState s);

struct FileVersion {
    std::size_t versionNumber{1};
    std::string revision{"A"};
    /// SHA-256 hex digest (64 hex chars) of the stored content.
    std::string contentHash;
    /// The actual file bytes stored in this version.
    std::string content;
    std::string comment;
    std::string author;
    /// ISO-8601 UTC timestamp (e.g. "2026-10-10T19:50:00Z").
    std::string timestamp;
};

struct VaultFile {
    FileId id{0};
    std::string filename;
    std::string vaultPath;
    LifecycleState state{LifecycleState::WorkInProgress};
    bool isCheckedOut{false};
    std::string checkedOutBy;
    std::vector<FileVersion> versions;
    std::vector<FileId> childrenDependencies; // assembly → part references

    /// Retrieve the latest version, or nullptr if none.
    [[nodiscard]] const FileVersion* latestVersion() const {
        return versions.empty() ? nullptr : &versions.back();
    }
    /// Retrieve a specific version by 1-based number.
    [[nodiscard]] const FileVersion* versionByNumber(std::size_t num) const {
        for (const auto& v : versions)
            if (v.versionNumber == num) return &v;
        return nullptr;
    }
};

struct BomItem {
    FileId fileId{0};
    std::string filename;
    std::string revision;
    int quantity{1};
};

struct AuditLogEntry {
    std::string timestamp;
    std::string username;
    std::string action; // "CHECK_IN", "CHECK_OUT", "STATE_CHANGE", "ADD_FILE", etc.
    std::string details;
};

class VaultRepository {
public:
    VaultRepository();

    [[nodiscard]] const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    // ── File registration ────────────────────────────────────────────────────
    FileId addFile(std::string filename, std::string vaultPath, std::string initialContent,
                   std::string author, std::string comment = "Initial commit");
    [[nodiscard]] const VaultFile* findFile(FileId id) const;
    [[nodiscard]] VaultFile*       findFileMut(FileId id);
    [[nodiscard]] const VaultFile* findFileByPath(std::string_view path) const;
    [[nodiscard]] const std::vector<VaultFile>& files() const { return files_; }

    // ── Check-in / check-out ─────────────────────────────────────────────────
    bool checkOut(FileId id, std::string_view username, std::string* error = nullptr);
    bool checkIn(FileId id, std::string_view username, std::string newContent,
                 std::string comment, std::string* error = nullptr);
    bool undoCheckOut(FileId id, std::string_view username, std::string* error = nullptr);

    // ── Lifecycle ────────────────────────────────────────────────────────────
    bool transitionState(FileId id, LifecycleState newState, std::string_view username,
                         std::string* error = nullptr);

    // ── Historical retrieval ─────────────────────────────────────────────────
    /// Returns the content bytes stored for the given version number (1-based).
    [[nodiscard]] std::optional<std::string> retrieveContent(FileId id,
                                                              std::size_t versionNumber) const;
    /// Rolls the head back to the given version (leaves prior versions intact).
    bool rollback(FileId id, std::size_t targetVersionNumber,
                  std::string_view username, std::string* error = nullptr);

    // ── Dependencies & BOM ───────────────────────────────────────────────────
    bool addDependency(FileId parentAssemblyId, FileId childComponentId);
    [[nodiscard]] std::vector<FileId> whereUsed(FileId childComponentId) const;
    [[nodiscard]] std::vector<BomItem> generateBom(FileId assemblyId) const;

    // ── Search ───────────────────────────────────────────────────────────────
    [[nodiscard]] std::vector<FileId> searchByFilename(std::string_view query) const;
    [[nodiscard]] std::vector<FileId> searchByAuthor(std::string_view author) const;

    // ── Audit ────────────────────────────────────────────────────────────────
    [[nodiscard]] const std::vector<AuditLogEntry>& auditLog() const { return auditLog_; }

    // ── Hashing ─────────────────────────────────────────────────────────────
    /// Pure C++ SHA-256 — returns 64-char lowercase hex digest.
    static std::string computeSha256(std::string_view content);

    // ── Native persistence ───────────────────────────────────────────────────
    [[nodiscard]] std::string serializeNative() const;
    static VaultRepository deserializeNative(std::string_view json, std::string* error = nullptr);
    /// Save through a validated same-directory temporary file. When replacing an
    /// existing repository, the previous valid file is retained at `<path>.bak`.
    [[nodiscard]] bool saveNativeFile(std::string_view path, std::string* error = nullptr) const;
    /// Load and structurally validate a repository file, including every content hash.
    static std::optional<VaultRepository> loadNativeFile(std::string_view path,
                                                          std::string* error = nullptr);

    // ── Statistics ───────────────────────────────────────────────────────────
    [[nodiscard]] std::size_t totalVersionCount() const;
    [[nodiscard]] std::size_t totalStoredBytes() const;

private:
    std::string name_{"EngineeringVault"};
    std::vector<VaultFile> files_;
    std::vector<AuditLogEntry> auditLog_;
    FileId nextFileId_{1};

    static std::string utcTimestampNow();
    void logAudit(std::string username, std::string action, std::string details);
};

} // namespace mydesk::vault
