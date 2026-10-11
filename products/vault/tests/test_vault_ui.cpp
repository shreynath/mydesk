// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QApplication>
#include <QLineEdit>
#include <QTableWidget>
#include <QTemporaryDir>

#include <cstdio>

using mydesk::vault::LifecycleState;
using mydesk::vault::VaultMainWindow;

static int failed = 0;
#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #condition); ++failed; \
} } while (false)

int main(int argc, char** argv) {
    // "test_vault_desktop_workflow"
    QApplication application(argc, argv);
    QTemporaryDir settingsHome;
    qputenv("XDG_CONFIG_HOME", settingsHome.path().toUtf8());

    VaultMainWindow window;
    window.setCurrentUser(QStringLiteral("alice"));
    std::string error;
    CHECK(window.addFileFromData("Assembly.iam", "$/Assemblies/Assembly.iam", "asm-v1",
                                 "alice", "Initial assembly", &error));
    const auto assembly = window.selectedFileId();
    CHECK(window.addFileFromData("Bracket.ipt", "$/Parts/Bracket.ipt", "part-v1",
                                 "alice", "Initial part", &error));
    const auto part = window.selectedFileId();
    CHECK(window.addDependency(assembly, part, &error));
    CHECK(window.selectFile(part));
    CHECK(window.checkOutSelected(&error));
    CHECK(window.checkInSelectedData("part-v2", "Added mounting holes", &error));
    CHECK(window.repository().findFile(part)->versions.size() == 2);
    CHECK(window.updateSelectedProperties("Bracket-M6.ipt", "$/Standard/Bracket-M6.ipt", &error));
    CHECK(window.transitionSelected(LifecycleState::ForReview, &error));

    auto* history = window.findChild<QTableWidget*>(QStringLiteral("historyTable"));
    auto* whereUsed = window.findChild<QTableWidget*>(QStringLiteral("whereUsedTable"));
    CHECK(history != nullptr && history->rowCount() == 2);
    CHECK(whereUsed != nullptr && whereUsed->rowCount() == 1);

    CHECK(window.undo());
    CHECK(window.repository().findFile(part)->state == LifecycleState::WorkInProgress);
    CHECK(window.redo());
    CHECK(window.repository().findFile(part)->state == LifecycleState::ForReview);

    const auto repositoryPath = settingsHome.filePath(QStringLiteral("workflow.myvault"));
    const auto bomPath = settingsHome.filePath(QStringLiteral("bom.csv"));
    const auto versionPath = settingsHome.filePath(QStringLiteral("Bracket.ipt"));
    CHECK(window.saveRepository(repositoryPath, &error));
    CHECK(window.exportBomCsv(bomPath, assembly, &error));
    CHECK(window.exportSelectedVersion(versionPath, 2, &error));

    VaultMainWindow reopened;
    CHECK(reopened.openRepository(repositoryPath, &error));
    CHECK(reopened.repository().files().size() == 2);
    CHECK(reopened.repository().findFile(part)->versions.size() == 2);
    CHECK(!reopened.isDirty());

    if (failed == 0) {
        std::puts("All Vault Qt workflow tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d Vault Qt workflow test(s) failed.\n", failed);
    return 1;
}
