#include "main_window.hpp"

#include "coordination_viewport.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
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
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <sstream>

namespace {

constexpr int kItemKindRole = Qt::UserRole;
constexpr int kObjectIdRole = Qt::UserRole + 1;
constexpr int kModelItem = 1;
constexpr int kElementItem = 2;

QString statusName(mydesk::coordinate::ClashStatus status) {
    using mydesk::coordinate::ClashStatus;
    switch (status) {
    case ClashStatus::New: return QObject::tr("New");
    case ClashStatus::Active: return QObject::tr("Active");
    case ClashStatus::Reviewed: return QObject::tr("Reviewed");
    case ClashStatus::Approved: return QObject::tr("Approved");
    case ClashStatus::Resolved: return QObject::tr("Resolved");
    }
    return QObject::tr("Unknown");
}

QDoubleSpinBox* coordinateEditor(QWidget* parent) {
    auto* editor = new QDoubleSpinBox(parent);
    editor->setRange(-1.0e9, 1.0e9);
    editor->setDecimals(4);
    editor->setSingleStep(0.1);
    editor->setSuffix(QObject::tr(" m"));
    return editor;
}

} // namespace

CoordinateMainWindow::CoordinateMainWindow(QWidget* parent) : QMainWindow(parent) {
    setObjectName(QStringLiteral("coordinateMainWindow"));
    buildInterface();
    buildMenus();
    statusBar()->showMessage(tr("Ready"));
    const QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("coordinate/windowGeometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("coordinate/windowState")).toByteArray());
    refreshUi();
}

CoordinateMainWindow::~CoordinateMainWindow() = default;

void CoordinateMainWindow::buildInterface() {
    viewport_ = new CoordinationViewport(this);
    setCentralWidget(viewport_);
    connect(viewport_, &CoordinationViewport::elementSelected, this,
            &CoordinateMainWindow::viewportSelectionChanged);
    connect(viewport_, &CoordinationViewport::viewChanged, this,
            [this](const QString& message) { statusBar()->showMessage(message, 3000); });

    auto* projectDock = new QDockWidget(tr("Federated Models"), this);
    projectDock->setObjectName(QStringLiteral("federatedModelsDock"));
    modelTree_ = new QTreeWidget(projectDock);
    modelTree_->setObjectName(QStringLiteral("federatedModelTree"));
    modelTree_->setHeaderLabels({tr("Name"), tr("Discipline / Category")});
    modelTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    modelTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    projectDock->setWidget(modelTree_);
    addDockWidget(Qt::LeftDockWidgetArea, projectDock);
    connect(modelTree_, &QTreeWidget::itemSelectionChanged, this,
            &CoordinateMainWindow::treeSelectionChanged);

    auto* clashDock = new QDockWidget(tr("Clash Detective"), this);
    clashDock->setObjectName(QStringLiteral("clashDetectiveDock"));
    auto* clashTabs = new QTabWidget(clashDock);

    auto* setupPage = new QWidget(clashTabs);
    auto* setupLayout = new QFormLayout(setupPage);
    clashNameEdit_ = new QLineEdit(tr("Coordination Test"), setupPage);
    clashNameEdit_->setObjectName(QStringLiteral("clashNameEdit"));
    setACombo_ = new QComboBox(setupPage);
    setACombo_->setObjectName(QStringLiteral("selectionSetACombo"));
    setBCombo_ = new QComboBox(setupPage);
    setBCombo_->setObjectName(QStringLiteral("selectionSetBCombo"));
    clashTypeCombo_ = new QComboBox(setupPage);
    clashTypeCombo_->addItem(tr("Hard clash"), 0);
    clashTypeCombo_->addItem(tr("Clearance"), 1);
    toleranceSpin_ = new QDoubleSpinBox(setupPage);
    toleranceSpin_->setRange(0.0, 1000.0);
    toleranceSpin_->setDecimals(4);
    toleranceSpin_->setValue(0.001);
    toleranceSpin_->setSuffix(tr(" m"));
    clearanceSpin_ = new QDoubleSpinBox(setupPage);
    clearanceSpin_->setRange(0.0, 1000.0);
    clearanceSpin_->setDecimals(4);
    clearanceSpin_->setValue(0.05);
    clearanceSpin_->setSuffix(tr(" m"));
    auto* createButton = new QPushButton(tr("Create Test"), setupPage);
    createButton->setObjectName(QStringLiteral("createClashTestButton"));
    setupLayout->addRow(tr("Name"), clashNameEdit_);
    setupLayout->addRow(tr("Selection A"), setACombo_);
    setupLayout->addRow(tr("Selection B"), setBCombo_);
    setupLayout->addRow(tr("Type"), clashTypeCombo_);
    setupLayout->addRow(tr("Tolerance"), toleranceSpin_);
    setupLayout->addRow(tr("Clearance"), clearanceSpin_);
    setupLayout->addRow(createButton);
    connect(createButton, &QPushButton::clicked, this, &CoordinateMainWindow::createClashTest);
    clashTabs->addTab(setupPage, tr("Setup"));

    auto* resultsPage = new QWidget(clashTabs);
    auto* resultsLayout = new QVBoxLayout(resultsPage);
    auto* resultSelectorLayout = new QHBoxLayout;
    clashTestCombo_ = new QComboBox(resultsPage);
    clashTestCombo_->setObjectName(QStringLiteral("clashTestCombo"));
    auto* runButton = new QPushButton(tr("Run"), resultsPage);
    runButton->setObjectName(QStringLiteral("runClashTestButton"));
    resultSelectorLayout->addWidget(clashTestCombo_, 1);
    resultSelectorLayout->addWidget(runButton);
    resultsLayout->addLayout(resultSelectorLayout);
    resultsTable_ = new QTableWidget(resultsPage);
    resultsTable_->setObjectName(QStringLiteral("clashResultsTable"));
    resultsTable_->setColumnCount(5);
    resultsTable_->setHorizontalHeaderLabels(
        {tr("ID"), tr("Element A"), tr("Element B"), tr("Value"), tr("Status")});
    resultsTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    resultsTable_->horizontalHeader()->setStretchLastSection(true);
    resultsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    resultsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsLayout->addWidget(resultsTable_, 1);
    reviewStatusCombo_ = new QComboBox(resultsPage);
    for (int value = 0; value <= static_cast<int>(mydesk::coordinate::ClashStatus::Resolved);
         ++value) {
        reviewStatusCombo_->addItem(statusName(static_cast<mydesk::coordinate::ClashStatus>(value)),
                                    value);
    }
    reviewCommentsEdit_ = new QPlainTextEdit(resultsPage);
    reviewCommentsEdit_->setPlaceholderText(tr("Coordination review comment"));
    reviewCommentsEdit_->setMaximumHeight(72);
    reviewApplyButton_ = new QPushButton(tr("Apply Review"), resultsPage);
    reviewApplyButton_->setObjectName(QStringLiteral("applyReviewButton"));
    resultsLayout->addWidget(reviewStatusCombo_);
    resultsLayout->addWidget(reviewCommentsEdit_);
    resultsLayout->addWidget(reviewApplyButton_);
    connect(clashTestCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &CoordinateMainWindow::clashTestChanged);
    connect(runButton, &QPushButton::clicked, this, &CoordinateMainWindow::runSelectedClashTest);
    connect(resultsTable_, &QTableWidget::itemSelectionChanged, this,
            &CoordinateMainWindow::resultSelectionChanged);
    connect(reviewApplyButton_, &QPushButton::clicked, this,
            &CoordinateMainWindow::applyReview);
    clashTabs->addTab(resultsPage, tr("Results"));
    clashDock->setWidget(clashTabs);
    addDockWidget(Qt::BottomDockWidgetArea, clashDock);

    auto* propertiesDock = new QDockWidget(tr("Element Properties"), this);
    propertiesDock->setObjectName(QStringLiteral("elementPropertiesDock"));
    auto* propertiesPage = new QWidget(propertiesDock);
    auto* propertiesLayout = new QFormLayout(propertiesPage);
    elementNameEdit_ = new QLineEdit(propertiesPage);
    elementNameEdit_->setObjectName(QStringLiteral("elementNameEdit"));
    elementCategoryEdit_ = new QLineEdit(propertiesPage);
    elementCategoryEdit_->setObjectName(QStringLiteral("elementCategoryEdit"));
    constexpr std::array<const char*, 6> labels{"Minimum X", "Minimum Y", "Minimum Z",
                                                "Maximum X", "Maximum Y", "Maximum Z"};
    for (std::size_t index = 0; index < boundsEdits_.size(); ++index) {
        boundsEdits_[index] = coordinateEditor(propertiesPage);
        boundsEdits_[index]->setObjectName(
            QStringLiteral("elementBound%1Edit").arg(static_cast<int>(index)));
    }
    propertiesApplyButton_ = new QPushButton(tr("Apply Element Changes"), propertiesPage);
    propertiesApplyButton_->setObjectName(QStringLiteral("applyElementPropertiesButton"));
    propertiesLayout->addRow(tr("Name"), elementNameEdit_);
    propertiesLayout->addRow(tr("Category"), elementCategoryEdit_);
    for (std::size_t index = 0; index < boundsEdits_.size(); ++index) {
        propertiesLayout->addRow(tr(labels[index]), boundsEdits_[index]);
    }
    propertiesLayout->addRow(propertiesApplyButton_);
    connect(propertiesApplyButton_, &QPushButton::clicked, this,
            &CoordinateMainWindow::applyElementProperties);
    propertiesDock->setWidget(propertiesPage);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock);

    resize(1400, 850);
}

void CoordinateMainWindow::buildMenus() {
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&New Session"), QKeySequence::New, this,
                        &CoordinateMainWindow::newSession);
    fileMenu->addAction(tr("&Open…"), QKeySequence::Open, this,
                        &CoordinateMainWindow::openSession);
    saveAction_ = fileMenu->addAction(tr("&Save"), QKeySequence::Save, this,
                                      &CoordinateMainWindow::saveSession);
    fileMenu->addAction(tr("Save &As…"), QKeySequence::SaveAs, this,
                        &CoordinateMainWindow::saveSessionAs);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Export &BCF XML…"), this, &CoordinateMainWindow::exportBcf);
    fileMenu->addAction(tr("Export Clash &Report…"), this,
                        &CoordinateMainWindow::exportCsvReport);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu(tr("&Edit"));
    undoAction_ = editMenu->addAction(tr("&Undo"), QKeySequence::Undo, this,
                                      &CoordinateMainWindow::undo);
    redoAction_ = editMenu->addAction(tr("&Redo"), QKeySequence::Redo, this,
                                      &CoordinateMainWindow::redo);

    auto* modelMenu = menuBar()->addMenu(tr("&Model"));
    modelMenu->addAction(tr("Append Federated Model…"), this,
                         &CoordinateMainWindow::addFederatedModel);
    modelMenu->addAction(tr("Create Coordination Box…"), this,
                         &CoordinateMainWindow::addBoxElement);

    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(tr("Fit Federated Model"), QKeySequence(Qt::Key_F), viewport_,
                        &CoordinationViewport::fitAll);
}

void CoordinateMainWindow::replaceDocument(mydesk::coordinate::CoordinateDocument document) {
    document_ = std::move(document);
    selectedElement_ = 0;
    currentFilePath_.clear();
    setDirty(false);
    refreshUi();
}

void CoordinateMainWindow::refreshUi() {
    refreshModelTree();
    refreshModelSelectors();
    refreshClashTests();
    viewport_->setDocument(&document_);
    showElementProperties(selectedElement_);
    updateActions();
    setWindowTitle(displayTitle());
}

void CoordinateMainWindow::refreshModelTree() {
    modelTree_->clear();
    for (const auto& model : document_.models()) {
        auto* modelItem = new QTreeWidgetItem(modelTree_,
                                               {QString::fromStdString(model.name),
                                                QString::fromStdString(model.discipline)});
        modelItem->setData(0, kItemKindRole, kModelItem);
        modelItem->setData(0, kObjectIdRole, QVariant::fromValue<qulonglong>(model.id));
        modelItem->setToolTip(0, QString::fromStdString(model.sourcePath));
        for (const auto& element : model.elements) {
            auto* elementItem = new QTreeWidgetItem(
                modelItem, {QString::fromStdString(element.name),
                            QString::fromStdString(element.category)});
            elementItem->setData(0, kItemKindRole, kElementItem);
            elementItem->setData(0, kObjectIdRole,
                                 QVariant::fromValue<qulonglong>(element.id));
        }
        modelItem->setExpanded(true);
    }
}

void CoordinateMainWindow::refreshModelSelectors() {
    const auto oldA = setACombo_->currentData().toULongLong();
    const auto oldB = setBCombo_->currentData().toULongLong();
    setACombo_->clear();
    setBCombo_->clear();
    for (const auto& model : document_.models()) {
        const QString label = QStringLiteral("%1 — %2")
                                  .arg(QString::fromStdString(model.name),
                                       QString::fromStdString(model.discipline));
        const auto value = QVariant::fromValue<qulonglong>(model.id);
        setACombo_->addItem(label, value);
        setBCombo_->addItem(label, value);
    }
    const int aIndex = setACombo_->findData(QVariant::fromValue<qulonglong>(oldA));
    const int bIndex = setBCombo_->findData(QVariant::fromValue<qulonglong>(oldB));
    setACombo_->setCurrentIndex(aIndex >= 0 ? aIndex : 0);
    setBCombo_->setCurrentIndex(bIndex >= 0 ? bIndex : std::min(1, setBCombo_->count() - 1));
}

void CoordinateMainWindow::refreshClashTests() {
    const auto oldId = currentTestId();
    clashTestCombo_->blockSignals(true);
    clashTestCombo_->clear();
    for (const auto& test : document_.clashTests()) {
        clashTestCombo_->addItem(QString::fromStdString(test.name),
                                 QVariant::fromValue<qulonglong>(test.id));
    }
    const int index = clashTestCombo_->findData(QVariant::fromValue<qulonglong>(oldId));
    clashTestCombo_->setCurrentIndex(index >= 0 ? index : 0);
    clashTestCombo_->blockSignals(false);
    refreshResults();
}

void CoordinateMainWindow::refreshResults() {
    resultsTable_->setRowCount(0);
    const auto* test = document_.findClashTest(currentTestId());
    if (!test) {
        reviewApplyButton_->setEnabled(false);
        return;
    }
    resultsTable_->setRowCount(static_cast<int>(test->results.size()));
    for (std::size_t index = 0; index < test->results.size(); ++index) {
        const auto& result = test->results[index];
        const int row = static_cast<int>(index);
        const auto* elementA = document_.findElement(result.elementA);
        const auto* elementB = document_.findElement(result.elementB);
        auto* idItem = new QTableWidgetItem(QString::number(result.id));
        idItem->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(result.id));
        resultsTable_->setItem(row, 0, idItem);
        resultsTable_->setItem(row, 1, new QTableWidgetItem(
                                           elementA ? QString::fromStdString(elementA->name)
                                                    : QString::number(result.elementA)));
        resultsTable_->setItem(row, 2, new QTableWidgetItem(
                                           elementB ? QString::fromStdString(elementB->name)
                                                    : QString::number(result.elementB)));
        resultsTable_->setItem(row, 3,
                               new QTableWidgetItem(QStringLiteral("%1 m")
                                                        .arg(result.penetrationOrDistance, 0, 'f', 4)));
        resultsTable_->setItem(row, 4, new QTableWidgetItem(statusName(result.status)));
    }
    reviewApplyButton_->setEnabled(false);
}

void CoordinateMainWindow::showElementProperties(mydesk::coordinate::ElementId id) {
    const auto* element = document_.findElement(id);
    const bool enabled = element != nullptr;
    elementNameEdit_->setEnabled(enabled);
    elementCategoryEdit_->setEnabled(enabled);
    propertiesApplyButton_->setEnabled(enabled);
    for (auto* editor : boundsEdits_) {
        editor->setEnabled(enabled);
    }
    if (!element) {
        elementNameEdit_->clear();
        elementCategoryEdit_->clear();
        return;
    }
    elementNameEdit_->setText(QString::fromStdString(element->name));
    elementCategoryEdit_->setText(QString::fromStdString(element->category));
    const std::array<double, 6> values{element->bounds.minX, element->bounds.minY,
                                       element->bounds.minZ, element->bounds.maxX,
                                       element->bounds.maxY, element->bounds.maxZ};
    for (std::size_t index = 0; index < values.size(); ++index) {
        boundsEdits_[index]->setValue(values[index]);
    }
}

void CoordinateMainWindow::setDirty(bool dirty) {
    dirty_ = dirty;
    setWindowModified(dirty);
    setWindowTitle(displayTitle());
    updateActions();
}

QString CoordinateMainWindow::displayTitle() const {
    const QString documentName = currentFilePath_.isEmpty()
                                     ? QString::fromStdString(document_.name())
                                     : QFileInfo(currentFilePath_).fileName();
    return tr("%1[*] — MyDesk Coordinate").arg(documentName);
}

bool CoordinateMainWindow::confirmDiscardChanges() {
    if (!dirty_) {
        return true;
    }
    const auto answer = QMessageBox::warning(
        this, tr("Unsaved coordination changes"),
        tr("Save changes to the current coordination session?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) {
        return false;
    }
    return answer == QMessageBox::Discard || saveSession();
}

void CoordinateMainWindow::newSession() {
    if (!confirmDiscardChanges()) {
        return;
    }
    document_ = {};
    currentFilePath_.clear();
    selectedElement_ = 0;
    setDirty(false);
    refreshUi();
}

void CoordinateMainWindow::openSession() {
    if (!confirmDiscardChanges()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Coordinate Session"), {}, tr("MyDesk Coordinate (*.mydesk-coord);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Open failed"), file.errorString());
        return;
    }
    const QByteArray contents = file.readAll();
    std::string error;
    auto loaded = mydesk::coordinate::CoordinateDocument::deserializeNative(
        std::string_view(contents.constData(), static_cast<std::size_t>(contents.size())), &error);
    if (!error.empty()) {
        QMessageBox::critical(this, tr("Invalid coordination session"),
                              QString::fromStdString(error));
        return;
    }
    document_ = std::move(loaded);
    currentFilePath_ = path;
    selectedElement_ = 0;
    setDirty(false);
    refreshUi();
    statusBar()->showMessage(tr("Opened %1").arg(path), 5000);
}

bool CoordinateMainWindow::writeSession(const QString& path) {
    const auto native = document_.serializeNative();
    return writeTextExport(path, QByteArray(native.data(), static_cast<qsizetype>(native.size())),
                           tr("coordination session"));
}

bool CoordinateMainWindow::writeTextExport(const QString& path, const QByteArray& bytes,
                                           const QString& description) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        QMessageBox::critical(this, tr("Write failed"),
                              tr("Could not write %1 to %2: %3")
                                  .arg(description, path, file.errorString()));
        return false;
    }
    return true;
}

bool CoordinateMainWindow::saveSession() {
    if (currentFilePath_.isEmpty()) {
        return saveSessionAs();
    }
    if (!writeSession(currentFilePath_)) {
        return false;
    }
    setDirty(false);
    statusBar()->showMessage(tr("Saved %1").arg(currentFilePath_), 5000);
    return true;
}

bool CoordinateMainWindow::saveSessionAs() {
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Coordinate Session"), currentFilePath_,
        tr("MyDesk Coordinate (*.mydesk-coord)"));
    if (path.isEmpty()) {
        return false;
    }
    if (!path.endsWith(QStringLiteral(".mydesk-coord"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".mydesk-coord");
    }
    if (!writeSession(path)) {
        return false;
    }
    currentFilePath_ = path;
    setDirty(false);
    statusBar()->showMessage(tr("Saved %1").arg(path), 5000);
    return true;
}

void CoordinateMainWindow::exportBcf() {
    QString path = QFileDialog::getSaveFileName(this, tr("Export BCF Visualization XML"), {},
                                                tr("BCF XML (*.bcf.xml *.xml)"));
    if (path.isEmpty()) {
        return;
    }
    const auto bcf = document_.exportBcfXml();
    if (writeTextExport(path, QByteArray(bcf.data(), static_cast<qsizetype>(bcf.size())),
                        tr("BCF export"))) {
        statusBar()->showMessage(tr("Exported BCF XML to %1").arg(path), 5000);
    }
}

void CoordinateMainWindow::exportCsvReport() {
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Clash Report"), {},
                                                      tr("CSV report (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    std::ostringstream report;
    report << "test,result,type,element_a,element_b,value_m,status,comment\n";
    auto csv = [](const std::string& value) {
        std::string escaped = value;
        std::size_t position = 0;
        while ((position = escaped.find('"', position)) != std::string::npos) {
            escaped.insert(position, 1, '"');
            position += 2;
        }
        return '"' + escaped + '"';
    };
    for (const auto& test : document_.clashTests()) {
        for (const auto& result : test.results) {
            report << csv(test.name) << ',' << result.id << ','
                   << (result.type == mydesk::coordinate::ClashType::Hard ? "hard" : "clearance")
                   << ',' << result.elementA << ',' << result.elementB << ','
                   << result.penetrationOrDistance << ','
                   << csv(statusName(result.status).toStdString()) << ',' << csv(result.comments)
                   << '\n';
        }
    }
    const auto text = report.str();
    if (writeTextExport(path, QByteArray(text.data(), static_cast<qsizetype>(text.size())),
                        tr("clash report"))) {
        statusBar()->showMessage(tr("Exported clash report to %1").arg(path), 5000);
    }
}

void CoordinateMainWindow::addFederatedModel() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Append Federated Model"));
    QFormLayout layout(&dialog);
    QLineEdit name(&dialog);
    QLineEdit discipline(&dialog);
    QLineEdit source(&dialog);
    auto* browse = new QPushButton(tr("Browse…"), &dialog);
    auto* sourceRow = new QWidget(&dialog);
    auto* sourceLayout = new QHBoxLayout(sourceRow);
    sourceLayout->setContentsMargins(0, 0, 0, 0);
    sourceLayout->addWidget(&source, 1);
    sourceLayout->addWidget(browse);
    connect(browse, &QPushButton::clicked, &dialog, [&] {
        const QString path = QFileDialog::getOpenFileName(
            &dialog, tr("Choose source model"), {}, tr("Coordination sources (*.ifc *.nwc *.nwd *.obj *.step *.stp);;All files (*)"));
        if (!path.isEmpty()) {
            source.setText(path);
            if (name.text().isEmpty()) {
                name.setText(QFileInfo(path).completeBaseName());
            }
        }
    });
    layout.addRow(tr("Display name"), &name);
    layout.addRow(tr("Discipline"), &discipline);
    layout.addRow(tr("Source file"), sourceRow);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (name.text().trimmed().isEmpty() || discipline.text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Incomplete model"),
                             tr("A display name and discipline are required."));
        return;
    }
    document_.addModel(name.text().trimmed().toStdString(), source.text().toStdString(),
                       discipline.text().trimmed().toStdString());
    setDirty(true);
    refreshUi();
}

mydesk::coordinate::ModelId CoordinateMainWindow::selectedModelId() const {
    const auto items = modelTree_->selectedItems();
    if (items.isEmpty()) {
        return 0;
    }
    QTreeWidgetItem* item = items.front();
    if (item->data(0, kItemKindRole).toInt() == kElementItem) {
        item = item->parent();
    }
    return static_cast<mydesk::coordinate::ModelId>(item->data(0, kObjectIdRole).toULongLong());
}

void CoordinateMainWindow::addBoxElement() {
    const auto modelId = selectedModelId();
    if (modelId == 0) {
        QMessageBox::information(this, tr("Select a model"),
                                 tr("Select a federated model before creating a coordination box."));
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Create Coordination Box"));
    QFormLayout layout(&dialog);
    QLineEdit name(tr("Coordination Box"), &dialog);
    QLineEdit category(tr("GenericModel"), &dialog);
    std::array<QDoubleSpinBox*, 6> values{};
    constexpr std::array<const char*, 6> labels{"Minimum X", "Minimum Y", "Minimum Z",
                                                "Maximum X", "Maximum Y", "Maximum Z"};
    for (std::size_t index = 0; index < values.size(); ++index) {
        values[index] = coordinateEditor(&dialog);
        values[index]->setValue(index >= 3 ? 1.0 : 0.0);
    }
    layout.addRow(tr("Name"), &name);
    layout.addRow(tr("Category"), &category);
    for (std::size_t index = 0; index < values.size(); ++index) {
        layout.addRow(tr(labels[index]), values[index]);
    }
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const mydesk::coordinate::BoundingBox3D bounds{
        values[0]->value(), values[1]->value(), values[2]->value(),
        values[3]->value(), values[4]->value(), values[5]->value()};
    const auto elementId = document_.addElementToModel(
        modelId, name.text().trimmed().toStdString(), category.text().trimmed().toStdString(), bounds);
    if (elementId == 0) {
        QMessageBox::warning(this, tr("Invalid element"),
                             tr("Names are required and minimum coordinates must not exceed maximum coordinates."));
        return;
    }
    selectedElement_ = elementId;
    setDirty(true);
    refreshUi();
    viewport_->setSelectedElement(elementId);
}

void CoordinateMainWindow::applyElementProperties() {
    const mydesk::coordinate::BoundingBox3D bounds{
        boundsEdits_[0]->value(), boundsEdits_[1]->value(), boundsEdits_[2]->value(),
        boundsEdits_[3]->value(), boundsEdits_[4]->value(), boundsEdits_[5]->value()};
    if (!document_.updateElement(selectedElement_, elementNameEdit_->text().trimmed().toStdString(),
                                 elementCategoryEdit_->text().trimmed().toStdString(), bounds)) {
        QMessageBox::warning(this, tr("Invalid element properties"),
                             tr("Names are required and each minimum coordinate must be no greater than its maximum."));
        return;
    }
    setDirty(true);
    refreshUi();
    viewport_->setSelectedElement(selectedElement_);
}

void CoordinateMainWindow::createClashTest() {
    if (setACombo_->currentIndex() < 0 || setBCombo_->currentIndex() < 0 ||
        clashNameEdit_->text().trimmed().isEmpty()) {
        QMessageBox::information(this, tr("Incomplete clash test"),
                                 tr("Choose two model selections and enter a test name."));
        return;
    }
    const auto type = clashTypeCombo_->currentData().toInt() == 0
                          ? mydesk::coordinate::ClashType::Hard
                          : mydesk::coordinate::ClashType::Clearance;
    const auto testId = document_.createClashTest(clashNameEdit_->text().trimmed().toStdString(),
                                                  type, toleranceSpin_->value(),
                                                  clearanceSpin_->value());
    document_.setClashTestSets(
        testId,
        {static_cast<mydesk::coordinate::ModelId>(setACombo_->currentData().toULongLong())},
        {static_cast<mydesk::coordinate::ModelId>(setBCombo_->currentData().toULongLong())});
    setDirty(true);
    refreshClashTests();
    clashTestCombo_->setCurrentIndex(
        clashTestCombo_->findData(QVariant::fromValue<qulonglong>(testId)));
    statusBar()->showMessage(tr("Created clash test"), 3000);
}

void CoordinateMainWindow::runSelectedClashTest() {
    const auto testId = currentTestId();
    if (testId == 0) {
        return;
    }
    const auto count = document_.runClashTest(testId);
    setDirty(true);
    refreshResults();
    statusBar()->showMessage(tr("Clash analysis completed: %1 result(s)").arg(count), 5000);
}

void CoordinateMainWindow::applyReview() {
    const auto resultId = currentResultId();
    const auto testId = currentTestId();
    if (resultId == 0 || testId == 0) {
        return;
    }
    const auto status = static_cast<mydesk::coordinate::ClashStatus>(
        reviewStatusCombo_->currentData().toInt());
    if (document_.setClashResultReview(testId, resultId, status,
                                       reviewCommentsEdit_->toPlainText().toStdString())) {
        setDirty(true);
        refreshResults();
        updateActions();
    }
}

void CoordinateMainWindow::undo() {
    if (document_.undo()) {
        setDirty(true);
        refreshUi();
    }
}

void CoordinateMainWindow::redo() {
    if (document_.redo()) {
        setDirty(true);
        refreshUi();
    }
}

void CoordinateMainWindow::treeSelectionChanged() {
    const auto items = modelTree_->selectedItems();
    if (items.isEmpty() || items.front()->data(0, kItemKindRole).toInt() != kElementItem) {
        selectedElement_ = 0;
    } else {
        selectedElement_ = static_cast<mydesk::coordinate::ElementId>(
            items.front()->data(0, kObjectIdRole).toULongLong());
    }
    viewport_->setSelectedElement(selectedElement_);
    showElementProperties(selectedElement_);
}

void CoordinateMainWindow::viewportSelectionChanged(qulonglong id) {
    selectedElement_ = static_cast<mydesk::coordinate::ElementId>(id);
    showElementProperties(selectedElement_);
    if (id == 0) {
        modelTree_->clearSelection();
        return;
    }
    const auto matches = modelTree_->findItems(QStringLiteral("*"),
                                                Qt::MatchWildcard | Qt::MatchRecursive, 0);
    for (auto* item : matches) {
        if (item->data(0, kItemKindRole).toInt() == kElementItem &&
            item->data(0, kObjectIdRole).toULongLong() == id) {
            modelTree_->setCurrentItem(item);
            break;
        }
    }
}

void CoordinateMainWindow::clashTestChanged(int) { refreshResults(); }

void CoordinateMainWindow::resultSelectionChanged() {
    const auto resultId = currentResultId();
    const auto* test = document_.findClashTest(currentTestId());
    if (!test || resultId == 0) {
        reviewApplyButton_->setEnabled(false);
        return;
    }
    const auto found = std::find_if(test->results.begin(), test->results.end(),
                                    [resultId](const auto& result) {
                                        return result.id == resultId;
                                    });
    if (found == test->results.end()) {
        reviewApplyButton_->setEnabled(false);
        return;
    }
    reviewStatusCombo_->setCurrentIndex(
        reviewStatusCombo_->findData(static_cast<int>(found->status)));
    reviewCommentsEdit_->setPlainText(QString::fromStdString(found->comments));
    reviewApplyButton_->setEnabled(true);
    viewport_->focusPoint({found->clashX, found->clashY, found->clashZ});
}

mydesk::coordinate::ClashTestId CoordinateMainWindow::currentTestId() const {
    return static_cast<mydesk::coordinate::ClashTestId>(
        clashTestCombo_ ? clashTestCombo_->currentData().toULongLong() : 0);
}

mydesk::coordinate::ClashResultId CoordinateMainWindow::currentResultId() const {
    if (!resultsTable_ || resultsTable_->currentRow() < 0) {
        return 0;
    }
    const auto* item = resultsTable_->item(resultsTable_->currentRow(), 0);
    return item ? static_cast<mydesk::coordinate::ClashResultId>(
                      item->data(Qt::UserRole).toULongLong())
                : 0;
}

void CoordinateMainWindow::updateActions() {
    saveAction_->setEnabled(dirty_ || currentFilePath_.isEmpty());
    undoAction_->setEnabled(document_.canUndo());
    redoAction_->setEnabled(document_.canRedo());
}

void CoordinateMainWindow::closeEvent(QCloseEvent* event) {
    if (!confirmDiscardChanges()) {
        event->ignore();
        return;
    }
    QSettings settings;
    settings.setValue(QStringLiteral("coordinate/windowGeometry"), saveGeometry());
    settings.setValue(QStringLiteral("coordinate/windowState"), saveState());
    event->accept();
}
