// SPDX-License-Identifier: MPL-2.0
// MyDesk Vault v0.1.0 — Engineering PDM application
// Interactive command-line interface for repository management.
#include "mydesk/vault/vault_document.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mydesk::vault;
namespace fs = std::filesystem;

// ─── Helper utilities ─────────────────────────────────────────────────────────

static void printBanner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════╗
║          MyDesk Vault  v0.1.0 — PDM System           ║
║    Open-source Autodesk Vault class alternative      ║
╚══════════════════════════════════════════════════════╝
)" << '\n';
}

static void printHelp() {
    std::cout <<
        "Commands:\n"
        "  open <path>                   Open or create repository file\n"
        "  save [path]                   Save repository to file\n"
        "  add <filename> <vault-path>   Add a new file from disk\n"
        "  ls                            List all vault files\n"
        "  info <id>                     Show file details\n"
        "  co <id> <user>                Check out a file\n"
        "  ci <id> <user> <disk-path>    Check in a file from disk\n"
        "  undo <id> <user>              Undo check-out\n"
        "  get <id> <ver> [output]       Retrieve a specific version\n"
        "  rollback <id> <ver> <user>    Roll back to earlier version\n"
        "  state <id> <state> <user>     Transition lifecycle state\n"
        "                                (wip|review|released|obsolete)\n"
        "  dep <parent-id> <child-id>    Add assembly dependency\n"
        "  bom <id>                      Generate BOM for assembly\n"
        "  used <id>                     Where-used query\n"
        "  search <query>                Search files by name\n"
        "  log                           Print audit log\n"
        "  stats                         Repository statistics\n"
        "  sha256 <text>                 Compute SHA-256 of a string\n"
        "  help                          Show this help\n"
        "  exit                          Exit\n\n";
}

static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) tokens.push_back(tok);
    return tokens;
}

static std::string readFile(const std::string& path, bool& ok) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { ok = false; return {}; }
    ok = true;
    return std::string(std::istreambuf_iterator<char>(f), {});
}

static bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    return f.good();
}

static void printFile(const VaultFile& f) {
    const char* state = lifecycleStateName(f.state);
    std::cout << "  ID=" << f.id << "  " << f.filename << "\n"
              << "    Vault path : " << f.vaultPath << "\n"
              << "    State      : " << state << "\n"
              << "    Checked out: " << (f.isCheckedOut ? ("YES by " + f.checkedOutBy) : "No") << "\n"
              << "    Versions   : " << f.versions.size() << "\n";
    if (!f.versions.empty()) {
        const auto& v = f.versions.back();
        std::cout << "    Latest     : v" << v.versionNumber
                  << " rev=" << v.revision
                  << " by=" << v.author
                  << " at=" << v.timestamp << "\n"
                  << "    SHA-256    : " << v.contentHash << "\n";
    }
}

// ─── Main interactive loop ────────────────────────────────────────────────────

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    printBanner();
    printHelp();

    VaultRepository repo;
    repo.setName("EngineeringVault");
    std::string repoPath;

    std::cout << "vault> " << std::flush;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) { std::cout << "vault> " << std::flush; continue; }
        auto tok = tokenize(line);
        if (tok.empty()) { std::cout << "vault> " << std::flush; continue; }

        const std::string& cmd = tok[0];

        if (cmd == "exit" || cmd == "quit") {
            std::cout << "Exiting MyDesk Vault.\n";
            break;

        } else if (cmd == "help") {
            printHelp();

        } else if (cmd == "open" && tok.size() >= 2) {
            repoPath = tok[1];
            if (!fs::exists(repoPath)) {
                // Create new empty repository file
                std::cout << "Creating new repository at " << repoPath << "\n";
                repo = VaultRepository{};
                repo.setName(fs::path(repoPath).stem().string());
                std::string err;
                if (repo.saveNativeFile(repoPath, &err)) {
                    std::cout << "Repository created.\n";
                } else {
                    std::cout << "ERROR: " << err << "\n";
                }
            } else {
                std::string err;
                auto loaded = VaultRepository::loadNativeFile(repoPath, &err);
                if (!loaded.has_value()) {
                    std::cout << "ERROR: " << err << "\n";
                } else {
                    repo = std::move(*loaded);
                    std::cout << "Opened repository '" << repo.name()
                              << "' with " << repo.files().size() << " file(s).\n";
                }
            }

        } else if (cmd == "save") {
            std::string path = (tok.size() >= 2) ? tok[1] : repoPath;
            if (path.empty()) { std::cout << "No path specified.\n"; }
            else {
                std::string err;
                if (repo.saveNativeFile(path, &err)) {
                    repoPath = path;
                    std::cout << "Saved to " << path << "\n";
                } else {
                    std::cout << "ERROR: " << err << "\n";
                }
            }

        } else if (cmd == "add" && tok.size() >= 3) {
            const std::string& diskPath = tok[1];
            const std::string& vaultPath = tok[2];
            std::string user = (tok.size() >= 4) ? tok[3] : "user";
            bool ok = false;
            auto content = readFile(diskPath, ok);
            if (!ok) {
                std::cout << "ERROR: cannot read " << diskPath << "\n";
            } else {
                auto id = repo.addFile(fs::path(diskPath).filename().string(),
                                       vaultPath, content, user, "Initial import");
                std::cout << "Added file ID=" << id << "\n";
                std::cout << "  SHA-256: " << VaultRepository::computeSha256(content) << "\n";
            }

        } else if (cmd == "ls") {
            if (repo.files().empty()) {
                std::cout << "  (empty repository)\n";
            }
            for (const auto& f : repo.files()) {
                std::cout << "  [" << f.id << "] " << f.filename
                          << "  v" << f.versions.size()
                          << "  " << lifecycleStateName(f.state)
                          << (f.isCheckedOut ? "  [CHECKED OUT by " + f.checkedOutBy + "]" : "")
                          << "\n";
            }

        } else if (cmd == "info" && tok.size() >= 2) {
            FileId id = std::stoull(tok[1]);
            const auto* f = repo.findFile(id);
            if (!f) { std::cout << "File " << id << " not found.\n"; }
            else {
                printFile(*f);
                if (f->versions.size() > 1) {
                    std::cout << "    Version history:\n";
                    for (const auto& v : f->versions) {
                        std::cout << "      v" << v.versionNumber
                                  << " rev=" << v.revision
                                  << " by=" << v.author
                                  << " " << v.timestamp
                                  << " \"" << v.comment << "\"\n"
                                  << "        sha256=" << v.contentHash << "\n";
                    }
                }
            }

        } else if (cmd == "co" && tok.size() >= 3) {
            FileId id = std::stoull(tok[1]);
            std::string user = tok[2];
            std::string err;
            if (repo.checkOut(id, user, &err)) {
                std::cout << "Checked out " << id << " by " << user << "\n";
            } else {
                std::cout << "ERROR: " << err << "\n";
            }

        } else if (cmd == "ci" && tok.size() >= 4) {
            FileId id = std::stoull(tok[1]);
            std::string user = tok[2];
            std::string diskPath = tok[3];
            std::string comment = (tok.size() >= 5) ? tok[4] : "Update";
            bool ok = false;
            auto content = readFile(diskPath, ok);
            if (!ok) {
                std::cout << "ERROR: cannot read " << diskPath << "\n";
            } else {
                std::string err;
                if (repo.checkIn(id, user, content, comment, &err)) {
                    const auto* f = repo.findFile(id);
                    std::cout << "Checked in v" << f->versions.size()
                              << " sha256=" << f->versions.back().contentHash << "\n";
                } else {
                    std::cout << "ERROR: " << err << "\n";
                }
            }

        } else if (cmd == "undo" && tok.size() >= 3) {
            FileId id = std::stoull(tok[1]);
            std::string user = tok[2];
            std::string err;
            if (repo.undoCheckOut(id, user, &err)) {
                std::cout << "Undo check-out for file " << id << "\n";
            } else {
                std::cout << "ERROR: " << err << "\n";
            }

        } else if (cmd == "get" && tok.size() >= 3) {
            FileId id = std::stoull(tok[1]);
            std::size_t ver = std::stoull(tok[2]);
            auto content = repo.retrieveContent(id, ver);
            if (!content) {
                std::cout << "ERROR: version not found\n";
            } else {
                if (tok.size() >= 4) {
                    if (writeFile(tok[3], *content)) {
                        std::cout << "Written to " << tok[3] << " (" << content->size() << " bytes)\n";
                    } else {
                        std::cout << "ERROR: cannot write output file\n";
                    }
                } else {
                    // Print to stdout if small enough
                    if (content->size() <= 4096) {
                        std::cout << *content << "\n";
                    } else {
                        std::cout << "(content " << content->size() << " bytes — specify output path)\n";
                    }
                }
            }

        } else if (cmd == "rollback" && tok.size() >= 4) {
            FileId id = std::stoull(tok[1]);
            std::size_t ver = std::stoull(tok[2]);
            std::string user = tok[3];
            std::string err;
            if (repo.rollback(id, ver, user, &err)) {
                std::cout << "Rolled back file " << id << " to v" << ver << "\n";
            } else {
                std::cout << "ERROR: " << err << "\n";
            }

        } else if (cmd == "state" && tok.size() >= 4) {
            FileId id = std::stoull(tok[1]);
            std::string stateStr = tok[2];
            std::string user = tok[3];
            LifecycleState state = LifecycleState::WorkInProgress;
            if      (stateStr == "review"  || stateStr == "ForReview")      state = LifecycleState::ForReview;
            else if (stateStr == "released"|| stateStr == "Released")       state = LifecycleState::Released;
            else if (stateStr == "obsolete"|| stateStr == "Obsolete")       state = LifecycleState::Obsolete;
            std::string err;
            if (repo.transitionState(id, state, user, &err)) {
                std::cout << "File " << id << " transitioned to " << lifecycleStateName(state) << "\n";
            } else {
                std::cout << "ERROR: " << err << "\n";
            }

        } else if (cmd == "dep" && tok.size() >= 3) {
            FileId parent = std::stoull(tok[1]);
            FileId child  = std::stoull(tok[2]);
            if (repo.addDependency(parent, child)) {
                std::cout << "Dependency added: " << parent << " → " << child << "\n";
            } else {
                std::cout << "Could not add dependency (already exists or parent not found)\n";
            }

        } else if (cmd == "bom" && tok.size() >= 2) {
            FileId id = std::stoull(tok[1]);
            auto bom = repo.generateBom(id);
            if (bom.empty()) {
                std::cout << "  (no dependencies)\n";
            }
            for (const auto& item : bom) {
                std::cout << "  [" << item.fileId << "] " << item.filename
                          << "  rev=" << item.revision
                          << "  qty=" << item.quantity << "\n";
            }

        } else if (cmd == "used" && tok.size() >= 2) {
            FileId id = std::stoull(tok[1]);
            auto parents = repo.whereUsed(id);
            if (parents.empty()) {
                std::cout << "  Not used in any parent assembly.\n";
            }
            for (auto pid : parents) {
                const auto* f = repo.findFile(pid);
                std::cout << "  Used in [" << pid << "] "
                          << (f ? f->filename : "(unknown)") << "\n";
            }

        } else if (cmd == "search" && tok.size() >= 2) {
            auto results = repo.searchByFilename(tok[1]);
            if (results.empty()) {
                std::cout << "  No matches found.\n";
            }
            for (auto id : results) {
                const auto* f = repo.findFile(id);
                if (f) std::cout << "  [" << id << "] " << f->filename << "\n";
            }

        } else if (cmd == "log") {
            const auto& log = repo.auditLog();
            if (log.empty()) { std::cout << "  (no audit entries)\n"; }
            for (const auto& e : log) {
                std::cout << "  " << e.timestamp << "  " << e.username
                          << "  " << e.action << "  " << e.details << "\n";
            }

        } else if (cmd == "stats") {
            std::cout << "  Repository  : " << repo.name() << "\n"
                      << "  Files       : " << repo.files().size() << "\n"
                      << "  Versions    : " << repo.totalVersionCount() << "\n"
                      << "  Stored bytes: " << repo.totalStoredBytes() << "\n"
                      << "  Audit entries: " << repo.auditLog().size() << "\n";

        } else if (cmd == "sha256" && tok.size() >= 2) {
            // Join remaining tokens
            std::string text;
            for (std::size_t i = 1; i < tok.size(); ++i) {
                if (i > 1) text.push_back(' ');
                text += tok[i];
            }
            std::cout << VaultRepository::computeSha256(text) << "\n";

        } else {
            std::cout << "Unknown command: " << cmd << "  (type 'help' for usage)\n";
        }

        std::cout << "vault> " << std::flush;
    }

    return 0;
}
