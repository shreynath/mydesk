// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace mydesk::vault {
namespace {

QString displayState(LifecycleState state) {
    static const QStringList names{QStringLiteral("Work in Progress"), QStringLiteral("For Review"),
                                   QStringLiteral("Released"), QStringLiteral("Obsolete")};
    return names.at(static_cast<int>(state));
}

QString revision(const VaultFile& file) {
    return file.latestVersion() ? QString::fromStdString(file.latestVersion()->revision)
                                : QStringLiteral("—");
}

QTableWidgetItem* cell(const QString& text, qulonglong id = 0) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    if (id != 0) item->setData(Qt::UserRole, QVariant::fromValue(id));
    return item;
}

std::string readFile(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = file.errorString();
        return {};
    }
    const auto bytes = file.readAll();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

void configureTable(QTableWidget* table, const QStringList& headers, int stretchColumn) {
    table->setColumnCount(static_cast<int>(headers.size()));
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    if (stretchColumn >= 0) table->horizontalHeader()->setSectionResizeMode(stretchColumn, QHeaderView::Stretch);
}

} // namespace

VaultMainWindow::VaultMainWindow(QWidget* parent) : QMainWindow(parent) {
    currentUser_ = qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME", "engineer"));
    if (currentUser_.trimmed().isEmpty()) currentUser_ = QStringLiteral("engineer");
    setObjectName(QStringLiteral("vaultMainWindow"));
    resize(1480, 900);
    setMinimumSize(1080, 700);
    buildUi();
    buildCommands();

    connect(repositoryTree_, &QTreeWidget::itemSelectionChanged, this, [this] {
        if (refreshing_ || repositoryTree_->selectedItems().isEmpty()) return;
        const auto id = repositoryTree_->selectedItems().front()->data(0, Qt::UserRole).toULongLong();
        if (id && id != selectedFileId_) selectFile(id);
    });
    connect(fileTable_, &QTableWidget::itemSelectionChanged, this, [this] {
        if (refreshing_ || fileTable_->currentRow() < 0) return;
        const auto id = fileTable_->item(fileTable_->currentRow(), 0)->data(Qt::UserRole).toULongLong();
        if (id && id != selectedFileId_) selectFile(id);
    });
    connect(searchEdit_, &QLineEdit::textChanged, this, [this] { refreshBrowser(); });
    connect(userEdit_, &QLineEdit::editingFinished, this, [this] { setCurrentUser(userEdit_->text()); });

    refreshAll();
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("window/state")).toByteArray());
}

void VaultMainWindow::buildUi() {
    setStyleSheet(QStringLiteral(
        "QMainWindow{background:#171b21;color:#e7ebf1} QWidget{font-size:12px}"
        "QToolBar{background:#202630;border:0;border-bottom:1px solid #3a4350;padding:4px;spacing:4px}"
        "QDockWidget::title{background:#242b35;padding:7px;font-weight:600}"
        "QTreeWidget,QTableWidget,QLineEdit,QComboBox{background:#1d232b;color:#e7ebf1;border:1px solid #3a4350;selection-background-color:#176b9b}"
        "QHeaderView::section{background:#2a323d;color:#dce3eb;padding:6px;border:0;border-right:1px solid #3a4350}"
        "QPushButton{background:#2879aa;color:white;border:0;border-radius:3px;padding:7px 12px}"
        "QTabWidget::pane{border:1px solid #3a4350} QTabBar::tab{background:#242b35;padding:7px 12px} QTabBar::tab:selected{background:#176b9b}"));

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    auto* summary = new QHBoxLayout;
    auto* heading = new QLabel(QStringLiteral("ENGINEERING FILES"), central);
    heading->setStyleSheet(QStringLiteral("font-weight:700;letter-spacing:1px;color:#8fcdf2"));
    statsLabel_ = new QLabel(central);
    statsLabel_->setObjectName(QStringLiteral("repositoryStats"));
    summary->addWidget(heading); summary->addStretch(); summary->addWidget(statsLabel_);
    layout->addLayout(summary);
    fileTable_ = new QTableWidget(central);
    fileTable_->setObjectName(QStringLiteral("fileTable"));
    configureTable(fileTable_, {QStringLiteral("Name"), QStringLiteral("Vault path"), QStringLiteral("Rev"),
                                QStringLiteral("Version"), QStringLiteral("Lifecycle"),
                                QStringLiteral("Lock owner"), QStringLiteral("Bytes")}, 1);
    fileTable_->setSortingEnabled(true);
    layout->addWidget(fileTable_);
    setCentralWidget(central);

    auto* browserDock = new QDockWidget(QStringLiteral("VAULT BROWSER"), this);
    browserDock->setObjectName(QStringLiteral("browserDock"));
    auto* browser = new QWidget(browserDock);
    auto* browserLayout = new QVBoxLayout(browser);
    browserLayout->addWidget(new QLabel(QStringLiteral("Signed in as"), browser));
    userEdit_ = new QLineEdit(currentUser_, browser);
    userEdit_->setObjectName(QStringLiteral("currentUserEdit"));
    browserLayout->addWidget(userEdit_);
    searchEdit_ = new QLineEdit(browser);
    searchEdit_->setObjectName(QStringLiteral("vaultSearchEdit"));
    searchEdit_->setPlaceholderText(QStringLiteral("Filter name or vault path…"));
    searchEdit_->setClearButtonEnabled(true);
    browserLayout->addWidget(searchEdit_);
    repositoryTree_ = new QTreeWidget(browser);
    repositoryTree_->setObjectName(QStringLiteral("repositoryTree"));
    repositoryTree_->setHeaderLabels({QStringLiteral("Project files"), QStringLiteral("Rev")});
    repositoryTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    browserLayout->addWidget(repositoryTree_);
    browserDock->setWidget(browser);
    addDockWidget(Qt::LeftDockWidgetArea, browserDock);

    auto* detailsDock = new QDockWidget(QStringLiteral("FILE DATA"), this);
    detailsDock->setObjectName(QStringLiteral("detailsDock"));
    auto* tabs = new QTabWidget(detailsDock);
    tabs->setObjectName(QStringLiteral("fileDataTabs"));
    auto* properties = new QWidget(tabs);
    auto* form = new QFormLayout(properties);
    idLabel_ = new QLabel(QStringLiteral("—"), properties);
    filenameEdit_ = new QLineEdit(properties); filenameEdit_->setObjectName(QStringLiteral("filenameEdit"));
    vaultPathEdit_ = new QLineEdit(properties); vaultPathEdit_->setObjectName(QStringLiteral("vaultPathEdit"));
    lifecycleCombo_ = new QComboBox(properties); lifecycleCombo_->setObjectName(QStringLiteral("lifecycleCombo"));
    lifecycleCombo_->addItems({QStringLiteral("Work in Progress"), QStringLiteral("For Review"),
                               QStringLiteral("Released"), QStringLiteral("Obsolete")});
    lockLabel_ = new QLabel(QStringLiteral("—"), properties);
    hashLabel_ = new QLabel(QStringLiteral("—"), properties); hashLabel_->setWordWrap(true);
    auto* applyProperties = new QPushButton(QStringLiteral("Apply file properties"), properties);
    applyProperties->setObjectName(QStringLiteral("applyPropertiesButton"));
    auto* applyLifecycle = new QPushButton(QStringLiteral("Apply lifecycle state"), properties);
    applyLifecycle->setObjectName(QStringLiteral("applyLifecycleButton"));
    form->addRow(QStringLiteral("File ID"), idLabel_); form->addRow(QStringLiteral("Name"), filenameEdit_);
    form->addRow(QStringLiteral("Vault path"), vaultPathEdit_); form->addRow(QStringLiteral("Lifecycle"), lifecycleCombo_);
    form->addRow(QStringLiteral("Lock"), lockLabel_); form->addRow(QStringLiteral("SHA-256"), hashLabel_);
    form->addRow(applyProperties); form->addRow(applyLifecycle);
    tabs->addTab(properties, QStringLiteral("Properties"));
    historyTable_ = new QTableWidget(tabs); historyTable_->setObjectName(QStringLiteral("historyTable"));
    configureTable(historyTable_, {QStringLiteral("Version"), QStringLiteral("Rev"), QStringLiteral("Author"),
                                   QStringLiteral("Date"), QStringLiteral("Comment")}, 4);
    tabs->addTab(historyTable_, QStringLiteral("History"));
    bomTable_ = new QTableWidget(tabs); bomTable_->setObjectName(QStringLiteral("bomTable"));
    configureTable(bomTable_, {QStringLiteral("Component"), QStringLiteral("Rev"),
                               QStringLiteral("Qty"), QStringLiteral("File ID")}, 0);
    tabs->addTab(bomTable_, QStringLiteral("BOM"));
    whereUsedTable_ = new QTableWidget(tabs); whereUsedTable_->setObjectName(QStringLiteral("whereUsedTable"));
    configureTable(whereUsedTable_, {QStringLiteral("Parent"), QStringLiteral("Vault path"), QStringLiteral("File ID")}, 1);
    tabs->addTab(whereUsedTable_, QStringLiteral("Where Used"));
    detailsDock->setWidget(tabs);
    addDockWidget(Qt::RightDockWidgetArea, detailsDock);

    auto* auditDock = new QDockWidget(QStringLiteral("AUDIT TRAIL"), this);
    auditDock->setObjectName(QStringLiteral("auditDock"));
    auditTable_ = new QTableWidget(auditDock); auditTable_->setObjectName(QStringLiteral("auditTable"));
    configureTable(auditTable_, {QStringLiteral("Timestamp"), QStringLiteral("User"),
                                 QStringLiteral("Action"), QStringLiteral("Details")}, 3);
    auditDock->setWidget(auditTable_);
    addDockWidget(Qt::BottomDockWidgetArea, auditDock);
    resizeDocks({browserDock, detailsDock}, {290, 400}, Qt::Horizontal);
    resizeDocks({auditDock}, {190}, Qt::Vertical);

    connect(applyProperties, &QPushButton::clicked, this, [this] {
        std::string error;
        if (!updateSelectedProperties(filenameEdit_->text().trimmed().toStdString(),
                                      vaultPathEdit_->text().trimmed().toStdString(), &error))
            showError(QStringLiteral("Update properties"), error);
    });
    connect(applyLifecycle, &QPushButton::clicked, this, [this] {
        std::string error;
        if (!transitionSelected(static_cast<LifecycleState>(lifecycleCombo_->currentIndex()), &error))
            showError(QStringLiteral("Lifecycle transition"), error);
    });
    statusBar()->showMessage(QStringLiteral("Ready"));
}

void VaultMainWindow::buildCommands() {
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    auto* newAction = fileMenu->addAction(QStringLiteral("&New Repository")); newAction->setShortcut(QKeySequence::New);
    auto* openAction = fileMenu->addAction(QStringLiteral("&Open…")); openAction->setShortcut(QKeySequence::Open);
    saveAction_ = fileMenu->addAction(QStringLiteral("&Save")); saveAction_->setShortcut(QKeySequence::Save);
    auto* saveAsAction = fileMenu->addAction(QStringLiteral("Save &As…"));
    fileMenu->addSeparator();
    auto* getAction = fileMenu->addAction(QStringLiteral("Get Selected Version…"));
    auto* bomExportAction = fileMenu->addAction(QStringLiteral("Export BOM as CSV…"));
    fileMenu->addSeparator();
    auto* quitAction = fileMenu->addAction(QStringLiteral("E&xit")); quitAction->setShortcut(QKeySequence::Quit);

    auto* editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
    undoAction_ = editMenu->addAction(QStringLiteral("&Undo Repository Change")); undoAction_->setShortcut(QKeySequence::Undo);
    undoAction_->setObjectName(QStringLiteral("undoAction"));
    redoAction_ = editMenu->addAction(QStringLiteral("&Redo Repository Change")); redoAction_->setShortcut(QKeySequence::Redo);
    redoAction_->setObjectName(QStringLiteral("redoAction"));

    auto* vaultMenu = menuBar()->addMenu(QStringLiteral("&Vault"));
    auto* addAction = vaultMenu->addAction(QStringLiteral("Add File…")); addAction->setObjectName(QStringLiteral("addFileAction"));
    checkOutAction_ = vaultMenu->addAction(QStringLiteral("Check Out")); checkOutAction_->setObjectName(QStringLiteral("checkOutAction"));
    checkInAction_ = vaultMenu->addAction(QStringLiteral("Check In…")); checkInAction_->setObjectName(QStringLiteral("checkInAction"));
    undoCheckoutAction_ = vaultMenu->addAction(QStringLiteral("Undo Check Out"));
    vaultMenu->addSeparator();
    addDependencyAction_ = vaultMenu->addAction(QStringLiteral("Add Component Relationship…"));
    removeDependencyAction_ = vaultMenu->addAction(QStringLiteral("Remove Selected BOM Relationship"));
    auto* rollbackAction = vaultMenu->addAction(QStringLiteral("Rollback to Selected Version"));

    auto* toolbar = addToolBar(QStringLiteral("Vault operations")); toolbar->setObjectName(QStringLiteral("vaultToolbar"));
    toolbar->setMovable(false); toolbar->addAction(addAction); toolbar->addSeparator();
    toolbar->addAction(checkOutAction_); toolbar->addAction(checkInAction_); toolbar->addAction(undoCheckoutAction_);
    toolbar->addSeparator(); toolbar->addAction(addDependencyAction_); toolbar->addAction(bomExportAction);

    connect(newAction, &QAction::triggered, this, [this] { newRepository(); });
    connect(openAction, &QAction::triggered, this, [this] { openDialog(); });
    connect(saveAction_, &QAction::triggered, this, [this] { save(); });
    connect(saveAsAction, &QAction::triggered, this, [this] { saveAs(); });
    connect(getAction, &QAction::triggered, this, [this] { exportVersionDialog(); });
    connect(bomExportAction, &QAction::triggered, this, [this] { exportBomDialog(); });
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
    connect(undoAction_, &QAction::triggered, this, [this] { undo(); });
    connect(redoAction_, &QAction::triggered, this, [this] { redo(); });
    connect(addAction, &QAction::triggered, this, [this] { addFileDialog(); });
    connect(checkOutAction_, &QAction::triggered, this, [this] {
        std::string error; if (!checkOutSelected(&error)) showError(QStringLiteral("Check out"), error);
    });
    connect(checkInAction_, &QAction::triggered, this, [this] { checkInDialog(); });
    connect(undoCheckoutAction_, &QAction::triggered, this, [this] {
        std::string error; if (!undoCheckOutSelected(&error)) showError(QStringLiteral("Undo check out"), error);
    });
    connect(addDependencyAction_, &QAction::triggered, this, [this] { dependencyDialog(); });
    connect(removeDependencyAction_, &QAction::triggered, this, [this] {
        if (bomTable_->currentRow() < 0) return;
        const auto child = bomTable_->item(bomTable_->currentRow(), 0)->data(Qt::UserRole).toULongLong();
        std::string error;
        if (!removeDependency(selectedFileId_, child, &error)) showError(QStringLiteral("Remove relationship"), error);
    });
    connect(rollbackAction, &QAction::triggered, this, [this] {
        if (historyTable_->currentRow() < 0) return;
        std::string error;
        if (!rollbackSelected(static_cast<std::size_t>(historyTable_->currentRow() + 1), &error))
            showError(QStringLiteral("Rollback"), error);
    });
}

void VaultMainWindow::setCurrentUser(QString user) {
    currentUser_ = user.trimmed().isEmpty() ? QStringLiteral("engineer") : user.trimmed();
    if (userEdit_->text() != currentUser_) userEdit_->setText(currentUser_);
    refreshActions();
}

bool VaultMainWindow::mutate(const std::function<bool(std::string*)>& operation, std::string* error) {
    std::string localError;
    const auto before = repository_.serializeNative();
    if (!operation(&localError)) {
        if (error) *error = localError.empty() ? "operation was rejected" : localError;
        return false;
    }
    undoSnapshots_.push_back(before); redoSnapshots_.clear(); setDirty(true); refreshAll();
    if (error) error->clear();
    return true;
}

bool VaultMainWindow::addFileFromData(const std::string& filename, const std::string& path,
                                      const std::string& content, const std::string& author,
                                      const std::string& comment, std::string* error) {
    if (filename.empty() || path.empty()) { if (error) *error = "file name and vault path must not be empty"; return false; }
    if (repository_.findFileByPath(path)) { if (error) *error = "a file already exists at that vault path"; return false; }
    const auto before = repository_.serializeNative();
    selectedFileId_ = repository_.addFile(filename, path, content,
        author.empty() ? currentUser_.toStdString() : author, comment);
    undoSnapshots_.push_back(before); redoSnapshots_.clear(); setDirty(true); refreshAll();
    if (error) error->clear();
    return true;
}

bool VaultMainWindow::selectFile(FileId id) {
    if (!repository_.findFile(id)) return false;
    selectedFileId_ = id; refreshBrowser(); refreshDetails(); refreshActions(); return true;
}

bool VaultMainWindow::checkOutSelected(std::string* error) {
    if (!selectedFileId_) { if (error) *error = "select a file first"; return false; }
    return mutate([this](std::string* e) { return repository_.checkOut(selectedFileId_, currentUser_.toStdString(), e); }, error);
}

bool VaultMainWindow::checkInSelectedData(const std::string& content, const std::string& comment, std::string* error) {
    if (!selectedFileId_) { if (error) *error = "select a file first"; return false; }
    return mutate([this, &content, &comment](std::string* e) {
        return repository_.checkIn(selectedFileId_, currentUser_.toStdString(), content, comment, e);
    }, error);
}

bool VaultMainWindow::undoCheckOutSelected(std::string* error) {
    if (!selectedFileId_) { if (error) *error = "select a file first"; return false; }
    return mutate([this](std::string* e) { return repository_.undoCheckOut(selectedFileId_, currentUser_.toStdString(), e); }, error);
}

bool VaultMainWindow::transitionSelected(LifecycleState state, std::string* error) {
    if (!selectedFileId_) { if (error) *error = "select a file first"; return false; }
    return mutate([this, state](std::string* e) {
        return repository_.transitionState(selectedFileId_, state, currentUser_.toStdString(), e);
    }, error);
}

bool VaultMainWindow::updateSelectedProperties(const std::string& filename, const std::string& path, std::string* error) {
    if (!selectedFileId_) { if (error) *error = "select a file first"; return false; }
    return mutate([this, &filename, &path](std::string* e) {
        return repository_.updateFileProperties(selectedFileId_, filename, path, currentUser_.toStdString(), e);
    }, error);
}

bool VaultMainWindow::addDependency(FileId parent, FileId child, std::string* error) {
    return mutate([this, parent, child](std::string* e) {
        if (repository_.addDependency(parent, child)) return true;
        *e = "relationship is duplicate, missing, self-referential, or cyclic"; return false;
    }, error);
}

bool VaultMainWindow::removeDependency(FileId parent, FileId child, std::string* error) {
    return mutate([this, parent, child](std::string* e) {
        if (repository_.removeDependency(parent, child)) return true;
        *e = "the selected direct relationship does not exist"; return false;
    }, error);
}

bool VaultMainWindow::rollbackSelected(std::size_t version, std::string* error) {
    if (!selectedFileId_) { if (error) *error = "select a file first"; return false; }
    return mutate([this, version](std::string* e) {
        return repository_.rollback(selectedFileId_, version, currentUser_.toStdString(), e);
    }, error);
}

bool VaultMainWindow::saveRepository(const QString& path, std::string* error) {
    std::string localError;
    if (!repository_.saveNativeFile(path.toStdString(), &localError)) { if (error) *error = localError; return false; }
    repositoryPath_ = path; QSettings().setValue(QStringLiteral("repository/lastPath"), path); setDirty(false);
    statusBar()->showMessage(QStringLiteral("Repository saved: %1").arg(path), 5000);
    if (error) error->clear(); return true;
}

bool VaultMainWindow::openRepository(const QString& path, std::string* error) {
    std::string localError;
    auto loaded = VaultRepository::loadNativeFile(path.toStdString(), &localError);
    if (!loaded) { if (error) *error = localError; return false; }
    repository_ = std::move(*loaded); repositoryPath_ = path;
    selectedFileId_ = repository_.files().empty() ? 0 : repository_.files().front().id;
    undoSnapshots_.clear(); redoSnapshots_.clear(); QSettings().setValue(QStringLiteral("repository/lastPath"), path);
    setDirty(false); refreshAll(); if (error) error->clear(); return true;
}

bool VaultMainWindow::exportBomCsv(const QString& path, FileId assembly, std::string* error) const {
    if (!repository_.findFile(assembly)) { if (error) *error = "assembly file not found"; return false; }
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) { if (error) *error = output.errorString().toStdString(); return false; }
    output.write("file_id,filename,revision,quantity\n");
    for (const auto& item : repository_.generateBom(assembly)) {
        auto quote = [](QString value) { value.replace('"', QStringLiteral("\"\"")); return QStringLiteral("\"") + value + '"'; };
        output.write(QStringLiteral("%1,%2,%3,%4\n").arg(item.fileId)
            .arg(quote(QString::fromStdString(item.filename))).arg(quote(QString::fromStdString(item.revision)))
            .arg(item.quantity).toUtf8());
    }
    if (!output.commit()) { if (error) *error = output.errorString().toStdString(); return false; }
    if (error) error->clear(); return true;
}

bool VaultMainWindow::exportSelectedVersion(const QString& path, std::size_t version, std::string* error) const {
    const auto content = repository_.retrieveContent(selectedFileId_, version);
    if (!content) { if (error) *error = "selected version was not found"; return false; }
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) ||
        output.write(content->data(), static_cast<qint64>(content->size())) != static_cast<qint64>(content->size()) ||
        !output.commit()) {
        if (error) *error = output.errorString().toStdString(); return false;
    }
    if (error) error->clear(); return true;
}

void VaultMainWindow::restoreSnapshot(const std::string& snapshot) {
    std::string error; auto restored = VaultRepository::deserializeNative(snapshot, &error);
    if (!error.empty()) return;
    repository_ = std::move(restored);
    if (!repository_.findFile(selectedFileId_)) selectedFileId_ = repository_.files().empty() ? 0 : repository_.files().front().id;
    setDirty(true); refreshAll();
}

bool VaultMainWindow::undo() {
    if (undoSnapshots_.empty()) return false;
    redoSnapshots_.push_back(repository_.serializeNative()); auto snapshot = std::move(undoSnapshots_.back());
    undoSnapshots_.pop_back(); restoreSnapshot(snapshot); return true;
}

bool VaultMainWindow::redo() {
    if (redoSnapshots_.empty()) return false;
    undoSnapshots_.push_back(repository_.serializeNative()); auto snapshot = std::move(redoSnapshots_.back());
    redoSnapshots_.pop_back(); restoreSnapshot(snapshot); return true;
}

void VaultMainWindow::refreshAll() {
    refreshBrowser(); refreshDetails(); refreshAudit(); refreshActions();
    statsLabel_->setText(QStringLiteral("%1 files  ·  %2 versions  ·  %3 bytes stored")
        .arg(repository_.files().size()).arg(repository_.totalVersionCount()).arg(repository_.totalStoredBytes()));
    const auto document = repositoryPath_.isEmpty() ? QString::fromStdString(repository_.name()) : QFileInfo(repositoryPath_).fileName();
    setWindowTitle(QStringLiteral("%1%2 — MyDesk Vault 0.2.0").arg(dirty_ ? QStringLiteral("• ") : QString{}, document));
}

void VaultMainWindow::refreshBrowser() {
    refreshing_ = true;
    const auto filter = searchEdit_->text().trimmed();
    repositoryTree_->clear(); fileTable_->setSortingEnabled(false); fileTable_->setRowCount(0);
    auto* root = new QTreeWidgetItem(repositoryTree_, {QString::fromStdString(repository_.name()), {}}); root->setExpanded(true);
    QHash<QString, QTreeWidgetItem*> folders; folders.insert(QStringLiteral("$"), root);
    for (const auto& file : repository_.files()) {
        const auto name = QString::fromStdString(file.filename); const auto path = QString::fromStdString(file.vaultPath);
        if (!filter.isEmpty() && !name.contains(filter, Qt::CaseInsensitive) && !path.contains(filter, Qt::CaseInsensitive)) continue;
        auto parts = path.split('/', Qt::SkipEmptyParts); if (!parts.isEmpty() && parts.front() == QStringLiteral("$")) parts.removeFirst();
        if (!parts.isEmpty()) parts.removeLast();
        auto* parent = root; QString key = QStringLiteral("$");
        for (const auto& part : parts) {
            key += '/' + part;
            if (!folders.contains(key)) { folders.insert(key, new QTreeWidgetItem(parent, {part, {}})); folders[key]->setExpanded(true); }
            parent = folders[key];
        }
        auto* treeItem = new QTreeWidgetItem(parent, {(file.isCheckedOut ? QStringLiteral("🔒 ") : QString{}) + name, revision(file)});
        treeItem->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(file.id)); treeItem->setToolTip(0, path);
        if (file.id == selectedFileId_) repositoryTree_->setCurrentItem(treeItem);
        const int row = fileTable_->rowCount(); fileTable_->insertRow(row);
        fileTable_->setItem(row, 0, cell(name, file.id)); fileTable_->setItem(row, 1, cell(path));
        fileTable_->setItem(row, 2, cell(revision(file))); fileTable_->setItem(row, 3, cell(QString::number(file.versions.size())));
        fileTable_->setItem(row, 4, cell(displayState(file.state)));
        fileTable_->setItem(row, 5, cell(file.isCheckedOut ? QString::fromStdString(file.checkedOutBy) : QStringLiteral("—")));
        fileTable_->setItem(row, 6, cell(QString::number(file.latestVersion() ? file.latestVersion()->content.size() : 0)));
        if (file.id == selectedFileId_) fileTable_->selectRow(row);
    }
    repositoryTree_->expandAll(); fileTable_->setSortingEnabled(true); refreshing_ = false;
}

void VaultMainWindow::refreshDetails() {
    const auto* file = repository_.findFile(selectedFileId_); refreshing_ = true;
    filenameEdit_->setEnabled(file); vaultPathEdit_->setEnabled(file); lifecycleCombo_->setEnabled(file);
    idLabel_->setText(file ? QString::number(file->id) : QStringLiteral("—"));
    filenameEdit_->setText(file ? QString::fromStdString(file->filename) : QString{});
    vaultPathEdit_->setText(file ? QString::fromStdString(file->vaultPath) : QString{});
    lifecycleCombo_->setCurrentIndex(file ? static_cast<int>(file->state) : 0);
    lockLabel_->setText(file ? (file->isCheckedOut ? QStringLiteral("Checked out by %1").arg(QString::fromStdString(file->checkedOutBy))
                                                   : QStringLiteral("Available")) : QStringLiteral("—"));
    hashLabel_->setText(file && file->latestVersion() ? QString::fromStdString(file->latestVersion()->contentHash) : QStringLiteral("—"));
    historyTable_->setRowCount(file ? static_cast<int>(file->versions.size()) : 0);
    if (file) for (int row = 0; row < static_cast<int>(file->versions.size()); ++row) {
        const auto& version = file->versions[static_cast<std::size_t>(row)];
        historyTable_->setItem(row, 0, cell(QString::number(version.versionNumber), version.versionNumber));
        historyTable_->setItem(row, 1, cell(QString::fromStdString(version.revision)));
        historyTable_->setItem(row, 2, cell(QString::fromStdString(version.author)));
        historyTable_->setItem(row, 3, cell(QString::fromStdString(version.timestamp)));
        historyTable_->setItem(row, 4, cell(QString::fromStdString(version.comment)));
    }
    if (file && !file->versions.empty()) historyTable_->selectRow(historyTable_->rowCount() - 1);
    const auto bom = file ? repository_.generateBom(file->id) : std::vector<BomItem>{}; bomTable_->setRowCount(static_cast<int>(bom.size()));
    for (int row = 0; row < static_cast<int>(bom.size()); ++row) {
        const auto& item = bom[static_cast<std::size_t>(row)];
        bomTable_->setItem(row, 0, cell(QString::fromStdString(item.filename), item.fileId));
        bomTable_->setItem(row, 1, cell(QString::fromStdString(item.revision))); bomTable_->setItem(row, 2, cell(QString::number(item.quantity)));
        bomTable_->setItem(row, 3, cell(QString::number(item.fileId)));
    }
    const auto parents = file ? repository_.whereUsed(file->id) : std::vector<FileId>{}; whereUsedTable_->setRowCount(static_cast<int>(parents.size()));
    for (int row = 0; row < static_cast<int>(parents.size()); ++row) if (const auto* parent = repository_.findFile(parents[static_cast<std::size_t>(row)])) {
        whereUsedTable_->setItem(row, 0, cell(QString::fromStdString(parent->filename)));
        whereUsedTable_->setItem(row, 1, cell(QString::fromStdString(parent->vaultPath)));
        whereUsedTable_->setItem(row, 2, cell(QString::number(parent->id)));
    }
    refreshing_ = false;
}

void VaultMainWindow::refreshAudit() {
    const auto& entries = repository_.auditLog(); auditTable_->setRowCount(static_cast<int>(entries.size()));
    for (int row = 0; row < static_cast<int>(entries.size()); ++row) {
        const auto& entry = entries[static_cast<std::size_t>(row)];
        auditTable_->setItem(row, 0, cell(QString::fromStdString(entry.timestamp))); auditTable_->setItem(row, 1, cell(QString::fromStdString(entry.username)));
        auditTable_->setItem(row, 2, cell(QString::fromStdString(entry.action))); auditTable_->setItem(row, 3, cell(QString::fromStdString(entry.details)));
    }
    if (!entries.empty()) auditTable_->scrollToBottom();
}

void VaultMainWindow::refreshActions() {
    const auto* file = repository_.findFile(selectedFileId_); saveAction_->setEnabled(dirty_);
    undoAction_->setEnabled(!undoSnapshots_.empty()); redoAction_->setEnabled(!redoSnapshots_.empty());
    checkOutAction_->setEnabled(file && !file->isCheckedOut && file->state != LifecycleState::Released);
    const bool owns = file && file->isCheckedOut && file->checkedOutBy == currentUser_.toStdString();
    checkInAction_->setEnabled(owns); undoCheckoutAction_->setEnabled(owns);
    addDependencyAction_->setEnabled(file && repository_.files().size() > 1);
    removeDependencyAction_->setEnabled(file && !file->childrenDependencies.empty());
}

void VaultMainWindow::setDirty(bool dirty) { dirty_ = dirty; if (saveAction_) refreshActions(); }

void VaultMainWindow::newRepository() {
    if (!confirmDiscardChanges()) return;
    const auto name = QInputDialog::getText(this, QStringLiteral("New repository"), QStringLiteral("Repository name:"));
    if (name.trimmed().isEmpty()) return;
    repository_ = {}; repository_.setName(name.trimmed().toStdString()); repositoryPath_.clear(); selectedFileId_ = 0;
    undoSnapshots_.clear(); redoSnapshots_.clear(); setDirty(false); refreshAll();
}

void VaultMainWindow::openDialog() {
    if (!confirmDiscardChanges()) return;
    const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Vault repository"), {}, QStringLiteral("MyDesk Vault (*.myvault)"));
    if (path.isEmpty()) return; std::string error; if (!openRepository(path, &error)) showError(QStringLiteral("Open repository"), error);
}

void VaultMainWindow::save() { if (repositoryPath_.isEmpty()) saveAs(); else { std::string error; if (!saveRepository(repositoryPath_, &error)) showError(QStringLiteral("Save repository"), error); } }
void VaultMainWindow::saveAs() {
    auto path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Vault repository"), repositoryPath_, QStringLiteral("MyDesk Vault (*.myvault)"));
    if (path.isEmpty()) return; if (!path.endsWith(QStringLiteral(".myvault"), Qt::CaseInsensitive)) path += QStringLiteral(".myvault");
    std::string error; if (!saveRepository(path, &error)) showError(QStringLiteral("Save repository"), error);
}

void VaultMainWindow::addFileDialog() {
    const auto source = QFileDialog::getOpenFileName(this, QStringLiteral("Add engineering file")); if (source.isEmpty()) return;
    const auto filename = QFileInfo(source).fileName();
    const auto path = QInputDialog::getText(this, QStringLiteral("Vault destination"), QStringLiteral("Vault path:"),
                                            QLineEdit::Normal, QStringLiteral("$/Designs/") + filename);
    if (path.trimmed().isEmpty()) return; QString ioError; const auto content = readFile(source, &ioError);
    if (!ioError.isEmpty()) { QMessageBox::critical(this, QStringLiteral("Add file"), ioError); return; }
    std::string error; if (!addFileFromData(filename.toStdString(), path.trimmed().toStdString(), content,
        currentUser_.toStdString(), "Added from " + source.toStdString(), &error)) showError(QStringLiteral("Add file"), error);
}

void VaultMainWindow::checkInDialog() {
    const auto source = QFileDialog::getOpenFileName(this, QStringLiteral("Select updated file content")); if (source.isEmpty()) return;
    bool accepted = false; const auto comment = QInputDialog::getMultiLineText(this, QStringLiteral("Check in"),
        QStringLiteral("Version comment:"), {}, &accepted); if (!accepted) return;
    QString ioError; const auto content = readFile(source, &ioError); if (!ioError.isEmpty()) { QMessageBox::critical(this, QStringLiteral("Check in"), ioError); return; }
    std::string error; if (!checkInSelectedData(content, comment.toStdString(), &error)) showError(QStringLiteral("Check in"), error);
}

void VaultMainWindow::dependencyDialog() {
    QStringList choices; std::vector<FileId> ids;
    for (const auto& file : repository_.files()) if (file.id != selectedFileId_) {
        choices << QStringLiteral("%1  [%2]").arg(QString::fromStdString(file.filename), QString::fromStdString(file.vaultPath)); ids.push_back(file.id);
    }
    bool accepted = false; const auto choice = QInputDialog::getItem(this, QStringLiteral("Add component relationship"),
        QStringLiteral("Referenced component:"), choices, 0, false, &accepted); if (!accepted) return;
    const auto index = choices.indexOf(choice); if (index < 0) return; std::string error;
    if (!addDependency(selectedFileId_, ids[static_cast<std::size_t>(index)], &error)) showError(QStringLiteral("Add relationship"), error);
}

void VaultMainWindow::exportBomDialog() {
    if (!selectedFileId_) return; auto path = QFileDialog::getSaveFileName(this, QStringLiteral("Export bill of materials"), {}, QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) return; if (!path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) path += QStringLiteral(".csv");
    std::string error; if (!exportBomCsv(path, selectedFileId_, &error)) showError(QStringLiteral("Export BOM"), error);
}

void VaultMainWindow::exportVersionDialog() {
    const auto* file = repository_.findFile(selectedFileId_); if (!file || file->versions.empty()) return;
    const auto version = historyTable_->currentRow() >= 0 ? static_cast<std::size_t>(historyTable_->currentRow() + 1) : file->versions.size();
    const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("Get file version"), QString::fromStdString(file->filename));
    if (path.isEmpty()) return; std::string error; if (!exportSelectedVersion(path, version, &error)) showError(QStringLiteral("Get version"), error);
}

void VaultMainWindow::showError(const QString& title, const std::string& error) { QMessageBox::warning(this, title, QString::fromStdString(error)); }

bool VaultMainWindow::confirmDiscardChanges() {
    if (!dirty_) return true;
    const auto answer = QMessageBox::warning(this, QStringLiteral("Unsaved repository"), QStringLiteral("Save changes to the current repository?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false; if (answer == QMessageBox::Save) { save(); return !dirty_; } return true;
}

void VaultMainWindow::restoreLastRepository() {
    const auto path = QSettings().value(QStringLiteral("repository/lastPath")).toString();
    if (path.isEmpty() || !QFileInfo::exists(path)) return; std::string error;
    if (!openRepository(path, &error)) statusBar()->showMessage(QStringLiteral("Could not reopen last repository: %1").arg(QString::fromStdString(error)), 10000);
}

void VaultMainWindow::closeEvent(QCloseEvent* event) {
    if (!confirmDiscardChanges()) { event->ignore(); return; }
    QSettings settings; settings.setValue(QStringLiteral("window/geometry"), saveGeometry()); settings.setValue(QStringLiteral("window/state"), saveState());
    event->accept();
}

} // namespace mydesk::vault
