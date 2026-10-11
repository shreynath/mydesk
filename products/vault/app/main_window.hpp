// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "mydesk/vault/vault_document.hpp"
#include <QMainWindow>
#include <functional>
#include <string>
#include <vector>

class QAction;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class QTabWidget;
class QTreeWidget;

namespace mydesk::vault {

class VaultMainWindow final : public QMainWindow {
public:
    explicit VaultMainWindow(QWidget* parent = nullptr);
    [[nodiscard]] const VaultRepository& repository() const { return repository_; }
    [[nodiscard]] FileId selectedFileId() const { return selectedFileId_; }
    [[nodiscard]] bool isDirty() const { return dirty_; }
    [[nodiscard]] QString repositoryPath() const { return repositoryPath_; }

    void setCurrentUser(QString user);
    bool addFileFromData(const std::string& filename, const std::string& vaultPath,
                         const std::string& content, const std::string& author,
                         const std::string& comment, std::string* error = nullptr);
    bool selectFile(FileId id);
    bool checkOutSelected(std::string* error = nullptr);
    bool checkInSelectedData(const std::string& content, const std::string& comment,
                             std::string* error = nullptr);
    bool undoCheckOutSelected(std::string* error = nullptr);
    bool transitionSelected(LifecycleState state, std::string* error = nullptr);
    bool updateSelectedProperties(const std::string& filename, const std::string& vaultPath,
                                  std::string* error = nullptr);
    bool addDependency(FileId parent, FileId child, std::string* error = nullptr);
    bool removeDependency(FileId parent, FileId child, std::string* error = nullptr);
    bool rollbackSelected(std::size_t version, std::string* error = nullptr);
    bool saveRepository(const QString& path, std::string* error = nullptr);
    bool openRepository(const QString& path, std::string* error = nullptr);
    bool exportBomCsv(const QString& path, FileId assembly, std::string* error = nullptr) const;
    bool exportSelectedVersion(const QString& path, std::size_t version,
                               std::string* error = nullptr) const;
    bool undo();
    bool redo();
    void restoreLastRepository();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void buildCommands();
    void refreshAll();
    void refreshBrowser();
    void refreshDetails();
    void refreshAudit();
    void refreshActions();
    void setDirty(bool dirty);
    bool mutate(const std::function<bool(std::string*)>& operation, std::string* error);
    void restoreSnapshot(const std::string& snapshot);
    bool confirmDiscardChanges();
    void showError(const QString& title, const std::string& error);
    void newRepository();
    void openDialog();
    void save();
    void saveAs();
    void addFileDialog();
    void checkInDialog();
    void dependencyDialog();
    void exportBomDialog();
    void exportVersionDialog();

    VaultRepository repository_;
    QString repositoryPath_;
    QString currentUser_;
    FileId selectedFileId_{0};
    bool dirty_{false};
    bool refreshing_{false};
    std::vector<std::string> undoSnapshots_;
    std::vector<std::string> redoSnapshots_;

    QLineEdit* searchEdit_{nullptr};
    QLineEdit* userEdit_{nullptr};
    QTreeWidget* repositoryTree_{nullptr};
    QTableWidget* fileTable_{nullptr};
    QLineEdit* filenameEdit_{nullptr};
    QLineEdit* vaultPathEdit_{nullptr};
    QLabel* idLabel_{nullptr};
    QLabel* lockLabel_{nullptr};
    QLabel* hashLabel_{nullptr};
    QLabel* statsLabel_{nullptr};
    QComboBox* lifecycleCombo_{nullptr};
    QTableWidget* historyTable_{nullptr};
    QTableWidget* bomTable_{nullptr};
    QTableWidget* whereUsedTable_{nullptr};
    QTableWidget* auditTable_{nullptr};
    QAction* saveAction_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
    QAction* checkOutAction_{nullptr};
    QAction* checkInAction_{nullptr};
    QAction* undoCheckoutAction_{nullptr};
    QAction* addDependencyAction_{nullptr};
    QAction* removeDependencyAction_{nullptr};
};

} // namespace mydesk::vault
