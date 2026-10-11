// SPDX-License-Identifier: MPL-2.0
#include "mydesk/vault/vault_document.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace mydesk::vault {

namespace {

std::string escapeJson(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

bool extractString(std::string_view json, std::string_view key, std::string& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) return false;
    const auto q1 = json.find('"', json.find(':', pos));
    if (q1 == std::string_view::npos) return false;
    const auto q2 = json.find('"', q1 + 1);
    if (q2 == std::string_view::npos) return false;
    out = std::string(json.substr(q1 + 1, q2 - q1 - 1));
    return true;
}

bool extractNumber(std::string_view json, std::string_view key, double& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) return false;
    const auto colon = json.find(':', pos);
    if (colon == std::string_view::npos) return false;
    try {
        out = std::stod(std::string(json.substr(colon + 1)));
        return std::isfinite(out);
    } catch (...) {
        return false;
    }
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

} // namespace

std::string VaultRepository::computeHash(std::string_view content) {
    // FNV-1a 64-bit hash
    std::uint64_t hash = 14695981039346656037ULL;
    for (char c : content) {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    std::ostringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}

VaultRepository::VaultRepository() = default;

void VaultRepository::logAudit(std::string username, std::string action, std::string details) {
    AuditLogEntry entry;
    entry.timestamp = "2026-10-10T12:00:00Z";
    entry.username = std::move(username);
    entry.action = std::move(action);
    entry.details = std::move(details);
    auditLog_.push_back(std::move(entry));
}

FileId VaultRepository::addFile(std::string filename, std::string vaultPath, std::string initialContent,
                               std::string author, std::string comment) {
    VaultFile file;
    file.id = nextFileId_++;
    file.filename = std::move(filename);
    file.vaultPath = std::move(vaultPath);
    file.state = LifecycleState::WorkInProgress;
    file.isCheckedOut = false;

    FileVersion v;
    v.versionNumber = 1;
    v.revision = "A";
    v.contentHash = computeHash(initialContent);
    v.author = author;
    v.comment = std::move(comment);
    v.timestamp = "2026-10-10T12:00:00Z";
    file.versions.push_back(std::move(v));

    logAudit(author, "ADD_FILE", file.filename + " (hash: " + file.versions.front().contentHash + ")");
    files_.push_back(std::move(file));
    return files_.back().id;
}

const VaultFile* VaultRepository::findFile(FileId id) const {
    for (const auto& f : files_) {
        if (f.id == id) return &f;
    }
    return nullptr;
}

const VaultFile* VaultRepository::findFileByPath(std::string_view path) const {
    for (const auto& f : files_) {
        if (f.vaultPath == path) return &f;
    }
    return nullptr;
}

bool VaultRepository::checkOut(FileId id, std::string_view username, std::string* error) {
    for (auto& f : files_) {
        if (f.id == id) {
            if (f.isCheckedOut) {
                if (error) *error = "file is already checked out by " + f.checkedOutBy;
                return false;
            }
            if (f.state == LifecycleState::Released) {
                if (error) *error = "file is in Released state and locked from direct edit";
                return false;
            }
            f.isCheckedOut = true;
            f.checkedOutBy = std::string(username);
            logAudit(std::string(username), "CHECK_OUT", f.filename);
            return true;
        }
    }
    if (error) *error = "file not found";
    return false;
}

bool VaultRepository::checkIn(FileId id, std::string_view username, std::string newContent,
                             std::string comment, std::string* error) {
    for (auto& f : files_) {
        if (f.id == id) {
            if (!f.isCheckedOut) {
                if (error) *error = "file is not checked out";
                return false;
            }
            if (f.checkedOutBy != username) {
                if (error) *error = "file was checked out by " + f.checkedOutBy + ", not " + std::string(username);
                return false;
            }

            FileVersion v;
            v.versionNumber = f.versions.size() + 1;
            v.revision = f.versions.empty() ? "A" : f.versions.back().revision;
            v.contentHash = computeHash(newContent);
            v.author = std::string(username);
            v.comment = std::move(comment);
            v.timestamp = "2026-10-10T12:00:00Z";

            f.versions.push_back(std::move(v));
            f.isCheckedOut = false;
            f.checkedOutBy.clear();

            logAudit(std::string(username), "CHECK_IN", f.filename + " (v" + std::to_string(f.versions.size()) + ")");
            return true;
        }
    }
    if (error) *error = "file not found";
    return false;
}

bool VaultRepository::undoCheckOut(FileId id, std::string_view username, std::string* error) {
    for (auto& f : files_) {
        if (f.id == id) {
            if (!f.isCheckedOut) {
                if (error) *error = "file is not checked out";
                return false;
            }
            if (f.checkedOutBy != username) {
                if (error) *error = "file was checked out by " + f.checkedOutBy;
                return false;
            }
            f.isCheckedOut = false;
            f.checkedOutBy.clear();
            logAudit(std::string(username), "UNDO_CHECK_OUT", f.filename);
            return true;
        }
    }
    if (error) *error = "file not found";
    return false;
}

bool VaultRepository::transitionState(FileId id, LifecycleState newState, std::string_view username, std::string* error) {
    for (auto& f : files_) {
        if (f.id == id) {
            if (f.isCheckedOut) {
                if (error) *error = "cannot transition lifecycle while checked out";
                return false;
            }
            const auto oldState = f.state;
            f.state = newState;

            // If transitioning to WorkInProgress from Released, increment revision letter
            if (oldState == LifecycleState::Released && newState == LifecycleState::WorkInProgress && !f.versions.empty()) {
                f.versions.back().revision = nextRevisionString(f.versions.back().revision);
            }

            logAudit(std::string(username), "STATE_CHANGE", f.filename + " to state " + std::to_string(static_cast<int>(newState)));
            return true;
        }
    }
    if (error) *error = "file not found";
    return false;
}

bool VaultRepository::addDependency(FileId parentAssemblyId, FileId childComponentId) {
    for (auto& f : files_) {
        if (f.id == parentAssemblyId) {
            if (std::find(f.childrenDependencies.begin(), f.childrenDependencies.end(), childComponentId) == f.childrenDependencies.end()) {
                f.childrenDependencies.push_back(childComponentId);
                return true;
            }
            return false;
        }
    }
    return false;
}

std::vector<FileId> VaultRepository::whereUsed(FileId childComponentId) const {
    std::vector<FileId> parents;
    for (const auto& f : files_) {
        if (std::find(f.childrenDependencies.begin(), f.childrenDependencies.end(), childComponentId) != f.childrenDependencies.end()) {
            parents.push_back(f.id);
        }
    }
    return parents;
}

std::vector<BomItem> VaultRepository::generateBom(FileId assemblyId) const {
    std::vector<BomItem> bom;
    const auto* parent = findFile(assemblyId);
    if (!parent) return bom;

    for (const auto childId : parent->childrenDependencies) {
        const auto* child = findFile(childId);
        if (child) {
            BomItem item;
            item.fileId = child->id;
            item.filename = child->filename;
            item.revision = child->versions.empty() ? "A" : child->versions.back().revision;
            item.quantity = 1;
            bom.push_back(item);
        }
    }
    return bom;
}

std::string VaultRepository::serializeNative() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"format\": \"mydesk-vault\",\n";
    ss << "  \"name\": \"" << escapeJson(name_) << "\",\n";
    ss << "  \"files\": [\n";
    for (std::size_t i = 0; i < files_.size(); ++i) {
        const auto& f = files_[i];
        ss << "    {\"id\": " << f.id << ", \"name\": \"" << escapeJson(f.filename)
           << "\", \"path\": \"" << escapeJson(f.vaultPath) << "\", \"state\": "
           << static_cast<int>(f.state) << ", \"locked\": " << (f.isCheckedOut ? "true" : "false")
           << ", \"deps\": [";
        for (std::size_t d = 0; d < f.childrenDependencies.size(); ++d) {
            ss << f.childrenDependencies[d];
            if (d + 1 < f.childrenDependencies.size()) ss << ", ";
        }
        ss << "]}";
        if (i + 1 < files_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n}\n";
    return ss.str();
}

VaultRepository VaultRepository::deserializeNative(std::string_view json, std::string* error) {
    VaultRepository repo;
    std::string format;
    if (!extractString(json, "format", format) || format != "mydesk-vault") {
        if (error) *error = "not a mydesk-vault document";
        return repo;
    }
    extractString(json, "name", repo.name_);

    std::size_t search = json.find("\"files\"");
    while (search != std::string_view::npos) {
        const auto idPos = json.find("\"id\":", search);
        if (idPos == std::string_view::npos) break;
        VaultFile f;
        double id = 0, state = 0;
        extractNumber(json.substr(idPos, 30), "id", id);
        f.id = static_cast<FileId>(id);
        extractString(json.substr(idPos, 120), "name", f.filename);
        extractString(json.substr(idPos, 120), "path", f.vaultPath);
        extractNumber(json.substr(idPos, 120), "state", state);
        f.state = static_cast<LifecycleState>(state);

        repo.files_.push_back(std::move(f));
        repo.nextFileId_ = std::max(repo.nextFileId_, f.id + 1);
        search = idPos + 10;
    }

    return repo;
}

} // namespace mydesk::vault
