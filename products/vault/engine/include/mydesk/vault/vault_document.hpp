// SPDX-License-Identifier: MPL-2.0
#pragma once

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

struct FileVersion {
    std::size_t versionNumber{1};
    std::string revision{"A"};
    std::string contentHash;
    std::string comment;
    std::string author;
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
    std::vector<FileId> childrenDependencies; // e.g. parts in an assembly
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
    std::string action; // "CHECK_IN", "CHECK_OUT", "STATE_CHANGE"
    std::string details;
};

class VaultRepository {
public:
    VaultRepository();

    [[nodiscard]] const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    // File operations
    FileId addFile(std::string filename, std::string vaultPath, std::string initialContent,
                   std::string author, std::string comment = "Initial commit");
    [[nodiscard]] const VaultFile* findFile(FileId id) const;
    [[nodiscard]] const VaultFile* findFileByPath(std::string_view path) const;
    [[nodiscard]] const std::vector<VaultFile>& files() const { return files_; }

    // Locking & Check-in / Check-out
    bool checkOut(FileId id, std::string_view username, std::string* error = nullptr);
    bool checkIn(FileId id, std::string_view username, std::string newContent,
                 std::string comment, std::string* error = nullptr);
    bool undoCheckOut(FileId id, std::string_view username, std::string* error = nullptr);

    // Lifecycle transitions
    bool transitionState(FileId id, LifecycleState newState, std::string_view username, std::string* error = nullptr);

    // Dependencies & Where-Used
    bool addDependency(FileId parentAssemblyId, FileId childComponentId);
    [[nodiscard]] std::vector<FileId> whereUsed(FileId childComponentId) const;
    [[nodiscard]] std::vector<BomItem> generateBom(FileId assemblyId) const;

    // Audit logs
    [[nodiscard]] const std::vector<AuditLogEntry>& auditLog() const { return auditLog_; }

    // Content hashing (SHA-256 simulation / FNV-1a 64-bit hex)
    static std::string computeHash(std::string_view content);

    // Native persistence
    [[nodiscard]] std::string serializeNative() const;
    static VaultRepository deserializeNative(std::string_view json, std::string* error = nullptr);

private:
    std::string name_{"EngineeringVault"};
    std::vector<VaultFile> files_;
    std::vector<AuditLogEntry> auditLog_;
    FileId nextFileId_{1};

    void logAudit(std::string username, std::string action, std::string details);
};

} // namespace mydesk::vault
