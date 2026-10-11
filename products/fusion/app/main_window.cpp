// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <array>

using namespace mydesk::fusion;

namespace {
QString kindName(TimelineKind kind) {
    switch (kind) {
    case TimelineKind::Sketch: return "Sketch";
    case TimelineKind::Extrude: return "Extrude";
    case TimelineKind::Revolve: return "Revolve";
    case TimelineKind::Chamfer: return "Chamfer";
    case TimelineKind::DirectPushPull: return "Push/Pull";
    case TimelineKind::CAMOperation: return "2D Pocket";
    case TimelineKind::ECADBoard: return "PCB";
    }
    return "Feature";
}

QTreeWidgetItem* category(QTreeWidget* tree, const QString& label) {
    auto* item = new QTreeWidgetItem(tree, {label});
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    return item;
}
}

FusionMainWindow::FusionMainWindow(QWidget* parent) : QMainWindow(parent) {
    setObjectName("FusionMainWindow");
    setWindowTitle("MyDesk Fusion — Untitled");
    resize(1320, 820);
    buildUi();
    viewport_->setDocument(&document_);
    QSettings settings("MyDesk", "Fusion");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    rebuildAll();
}

void FusionMainWindow::buildUi() {
    viewport_ = new FusionViewport(this);
    setCentralWidget(viewport_);
    viewport_->featurePicked = [this](FeatureId id) { selectFeature(id); };

    auto* fileMenu = menuBar()->addMenu("&File");
    auto* newAction = fileMenu->addAction("&New", QKeySequence::New);
    auto* openAction = fileMenu->addAction("&Open…", QKeySequence::Open);
    auto* saveAction = fileMenu->addAction("&Save", QKeySequence::Save);
    auto* saveAsAction = fileMenu->addAction("Save &As…", QKeySequence::SaveAs);
    fileMenu->addSeparator();
    auto* exportAction = fileMenu->addAction("Export CAM &G-code…");
    fileMenu->addSeparator();
    auto* quitAction = fileMenu->addAction("&Quit", QKeySequence::Quit);

    auto* editMenu = menuBar()->addMenu("&Edit");
    undoAction_ = editMenu->addAction("&Undo", QKeySequence::Undo);
    redoAction_ = editMenu->addAction("&Redo", QKeySequence::Redo);

    auto* solidMenu = menuBar()->addMenu("&Solid");
    auto* sketchAction = solidMenu->addAction("Create Rectangle Sketch");
    auto* extrudeAction = solidMenu->addAction("Extrude Sketch");
    auto* pushPullAction = solidMenu->addAction("Push/Pull End Face");
    auto* starterAction = solidMenu->addAction("Create Starter Part");
    auto* manufactureMenu = menuBar()->addMenu("&Manufacture");
    auto* pocketAction = manufactureMenu->addAction("Generate 2D Pocket");
    auto* electronicsMenu = menuBar()->addMenu("&Electronics");
    auto* boardAction = electronicsMenu->addAction("Add PCB Outline");
    auto* viewMenu = menuBar()->addMenu("&View");
    auto* fitAction = viewMenu->addAction("Fit All", QKeySequence(Qt::Key_F));

    auto* toolbar = addToolBar("Design and Manufacture");
    toolbar->setObjectName("FusionPrimaryToolbar");
    toolbar->addAction(sketchAction);
    toolbar->addAction(extrudeAction);
    toolbar->addAction(pushPullAction);
    toolbar->addSeparator();
    toolbar->addAction(pocketAction);
    toolbar->addAction(boardAction);
    toolbar->addSeparator();
    toolbar->addAction(fitAction);

    auto* browserDock = new QDockWidget("Browser", this);
    browserDock->setObjectName("FusionBrowserDock");
    browser_ = new QTreeWidget(browserDock);
    browser_->setHeaderLabels({"Component / Feature", "State"});
    browser_->setAlternatingRowColors(true);
    browserDock->setWidget(browser_);
    addDockWidget(Qt::LeftDockWidgetArea, browserDock);

    auto* propertiesDock = new QDockWidget("Properties", this);
    propertiesDock->setObjectName("FusionPropertiesDock");
    auto* properties = new QWidget(propertiesDock);
    auto* form = new QFormLayout(properties);
    selectionSummary_ = new QLabel("No feature selected", properties);
    selectionSummary_->setWordWrap(true);
    featureName_ = new QLineEdit(properties);
    suppressed_ = new QCheckBox("Exclude from regeneration", properties);
    parameterA_ = new QDoubleSpinBox(properties);
    parameterB_ = new QDoubleSpinBox(properties);
    parameterC_ = new QDoubleSpinBox(properties);
    parameterD_ = new QDoubleSpinBox(properties);
    for (QDoubleSpinBox* box : {parameterA_, parameterB_, parameterC_, parameterD_}) {
        box->setRange(-1000000.0, 1000000.0);
        box->setDecimals(3);
        box->setSuffix(" mm");
    }
    parameterALabel_ = new QLabel("Parameter A", properties);
    parameterBLabel_ = new QLabel("Parameter B", properties);
    parameterCLabel_ = new QLabel("Parameter C", properties);
    parameterDLabel_ = new QLabel("Parameter D", properties);
    auto* apply = new QPushButton("Apply Feature Changes", properties);
    form->addRow(selectionSummary_);
    form->addRow("Name", featureName_);
    form->addRow(suppressed_);
    form->addRow(parameterALabel_, parameterA_);
    form->addRow(parameterBLabel_, parameterB_);
    form->addRow(parameterCLabel_, parameterC_);
    form->addRow(parameterDLabel_, parameterD_);
    form->addRow(apply);
    propertiesDock->setWidget(properties);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock);

    auto* timelineDock = new QDockWidget("Parametric Timeline", this);
    timelineDock->setObjectName("FusionTimelineDock");
    timeline_ = new QListWidget(timelineDock);
    timeline_->setFlow(QListView::LeftToRight);
    timeline_->setWrapping(false);
    timeline_->setViewMode(QListView::ListMode);
    timeline_->setMaximumHeight(115);
    timelineDock->setWidget(timeline_);
    addDockWidget(Qt::BottomDockWidgetArea, timelineDock);

    statusSummary_ = new QLabel(this);
    statusBar()->addPermanentWidget(statusSummary_);
    statusBar()->showMessage("Ready — author a rectangle sketch or starter part");

    connect(newAction, &QAction::triggered, this, [this] { newDocument(); });
    connect(openAction, &QAction::triggered, this, [this] { openDialog(); });
    connect(saveAction, &QAction::triggered, this, [this] { save(); });
    connect(saveAsAction, &QAction::triggered, this, [this] { saveAs(); });
    connect(exportAction, &QAction::triggered, this, [this] { exportDialog(); });
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
    connect(undoAction_, &QAction::triggered, this, [this] { undo(); });
    connect(redoAction_, &QAction::triggered, this, [this] { redo(); });
    connect(sketchAction, &QAction::triggered, this, [this] { addSketch(); });
    connect(extrudeAction, &QAction::triggered, this, [this] { addExtrude(); });
    connect(pushPullAction, &QAction::triggered, this, [this] { addPushPull(); });
    connect(starterAction, &QAction::triggered, this, [this] { createStarterPart(); });
    connect(pocketAction, &QAction::triggered, this, [this] { addPocket(); });
    connect(boardAction, &QAction::triggered, this, [this] { addBoard(); });
    connect(fitAction, &QAction::triggered, viewport_, &FusionViewport::fitAll);
    connect(apply, &QPushButton::clicked, this, [this] { applyProperties(); });
    connect(browser_, &QTreeWidget::itemSelectionChanged, this, [this] {
        const auto items = browser_->selectedItems();
        if (!items.empty()) selectFeature(items.front()->data(0, Qt::UserRole).toULongLong());
    });
    connect(timeline_, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* item) { if (item) selectFeature(item->data(Qt::UserRole).toULongLong()); });
}

const TimelineEntry* FusionMainWindow::selectedEntry() const {
    const auto found = std::find_if(document_.timeline().begin(), document_.timeline().end(),
        [this](const TimelineEntry& entry) { return entry.id == selectedId_; });
    return found == document_.timeline().end() ? nullptr : &*found;
}

void FusionMainWindow::rebuildBrowser() {
    browser_->blockSignals(true);
    browser_->clear();
    auto* component = category(browser_, QString::fromStdString(document_.name()));
    auto* design = new QTreeWidgetItem(component, {"Design", QString("%1 bodies").arg(document_.bodies().size())});
    auto* manufacture = new QTreeWidgetItem(component, {"Manufacture", QString("%1 setups").arg(document_.camSetups().size())});
    auto* electronics = new QTreeWidgetItem(component, {"Electronics", document_.ecadBoard() ? "1 board" : "Empty"});
    for (const auto& entry : document_.timeline()) {
        QTreeWidgetItem* parent = design;
        if (entry.kind == TimelineKind::CAMOperation) parent = manufacture;
        if (entry.kind == TimelineKind::ECADBoard) parent = electronics;
        auto* item = new QTreeWidgetItem(parent, {QString::fromStdString(entry.name),
            entry.suppressed ? "Suppressed" : kindName(entry.kind)});
        item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(entry.id));
        if (entry.id == selectedId_) item->setSelected(true);
    }
    browser_->expandAll();
    browser_->blockSignals(false);
}

void FusionMainWindow::rebuildTimeline() {
    timeline_->blockSignals(true);
    timeline_->clear();
    for (std::size_t i = 0; i < document_.timeline().size(); ++i) {
        const auto& entry = document_.timeline()[i];
        auto* item = new QListWidgetItem(QString("%1  %2").arg(i + 1).arg(QString::fromStdString(entry.name)), timeline_);
        item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(entry.id));
        item->setToolTip(kindName(entry.kind));
        if (i >= document_.timelineCursor()) item->setForeground(QColor(130, 135, 140));
        if (entry.suppressed) item->setText("⊘ " + item->text());
        if (entry.id == selectedId_) timeline_->setCurrentItem(item);
    }
    timeline_->blockSignals(false);
}

void FusionMainWindow::refreshProperties() {
    const TimelineEntry* entry = selectedEntry();
    const bool enabled = entry != nullptr;
    featureName_->setEnabled(enabled);
    suppressed_->setEnabled(enabled);
    if (!entry) {
        selectionSummary_->setText("No feature selected");
        featureName_->clear();
    } else {
        selectionSummary_->setText(QString("Feature %1 · %2").arg(entry->id).arg(kindName(entry->kind)));
        featureName_->setText(QString::fromStdString(entry->name));
        suppressed_->setChecked(entry->suppressed);
    }

    std::array<QLabel*, 4> labels{parameterALabel_, parameterBLabel_, parameterCLabel_, parameterDLabel_};
    std::array<QDoubleSpinBox*, 4> boxes{parameterA_, parameterB_, parameterC_, parameterD_};
    std::array<QString, 4> keys;
    if (entry) {
        switch (entry->kind) {
        case TimelineKind::Sketch: keys = {"width", "height", {}, {}}; break;
        case TimelineKind::Extrude: keys = {"depth", {}, {}, {}}; break;
        case TimelineKind::DirectPushPull: keys = {"offset", {}, {}, {}}; break;
        case TimelineKind::CAMOperation: keys = {"toolDiameter", "depth", "stepover", "stepdown"}; break;
        case TimelineKind::ECADBoard: keys = {"width", "height", "thickness", {}}; break;
        default: break;
        }
    }
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        const bool visible = entry && !keys[i].isEmpty();
        labels[i]->setVisible(visible);
        boxes[i]->setVisible(visible);
        if (visible) {
            labels[i]->setText(keys[i]);
            boxes[i]->setProperty("parameterKey", keys[i]);
            const auto found = entry->parameters.find(keys[i].toStdString());
            boxes[i]->setValue(found == entry->parameters.end() ? 0.0 : found->second);
        }
    }
}

void FusionMainWindow::rebuildAll() {
    if (!selectedEntry()) selectedId_ = 0;
    rebuildBrowser();
    rebuildTimeline();
    refreshProperties();
    viewport_->setSelectedFeature(selectedId_);
    viewport_->update();
    undoAction_->setEnabled(document_.canUndo());
    redoAction_->setEnabled(document_.canRedo());
    statusSummary_->setText(QString("%1 features · %2 bodies · %3 toolpaths")
        .arg(document_.timeline().size()).arg(document_.bodies().size()).arg(document_.camSetups().size()));
}

bool FusionMainWindow::selectFeature(FeatureId id) {
    const auto found = std::find_if(document_.timeline().begin(), document_.timeline().end(),
        [id](const TimelineEntry& entry) { return entry.id == id; });
    if (found == document_.timeline().end()) return false;
    selectedId_ = id;
    rebuildAll();
    return true;
}

void FusionMainWindow::addSketch() {
    selectedId_ = document_.addSketchRectangle("Rectangle Sketch", 80.0, 50.0);
    setDirty(true); rebuildAll();
}

void FusionMainWindow::addExtrude() {
    const auto found = std::find_if(document_.timeline().rbegin(), document_.timeline().rend(),
        [](const TimelineEntry& entry) { return entry.kind == TimelineKind::Sketch; });
    if (found == document_.timeline().rend()) { statusBar()->showMessage("Create a sketch before extruding", 4000); return; }
    selectedId_ = document_.addExtrude("Extrude", found->id, 15.0);
    setDirty(true); rebuildAll();
}

void FusionMainWindow::addPushPull() {
    if (document_.bodies().empty()) { statusBar()->showMessage("Create a solid before push/pull", 4000); return; }
    selectedId_ = document_.addDirectPushPull("Push Pull", document_.bodies().front().featureId, 5.0);
    setDirty(true); rebuildAll();
}

void FusionMainWindow::createStarterPart() {
    const FeatureId sketch = document_.addSketchRectangle("Base Sketch", 80.0, 50.0);
    selectedId_ = document_.addExtrude("Base Extrude", sketch, 15.0);
    setDirty(true); rebuildAll(); viewport_->fitAll();
}

void FusionMainWindow::addPocket() {
    if (document_.bodies().empty()) { statusBar()->showMessage("Create a solid before generating CAM", 4000); return; }
    selectedId_ = document_.addCAMPocket("2D Pocket", document_.bodies().front().featureId, 6.0, 6.0, 2.5, 2.0);
    setDirty(true); rebuildAll();
    if (!document_.lastError().empty()) statusBar()->showMessage(QString::fromStdString(document_.lastError()), 5000);
}

void FusionMainWindow::addBoard() {
    selectedId_ = document_.addECADBoard("Controller PCB", 70.0, 35.0, 1.6);
    setDirty(true); rebuildAll();
}

bool FusionMainWindow::setSelectedParameter(const QString& parameter, double value) {
    if (!selectedEntry()) return false;
    if (!document_.setFeatureParameter(selectedId_, parameter.toStdString(), value)) {
        statusBar()->showMessage(QString::fromStdString(document_.lastError()), 5000);
        return false;
    }
    setDirty(true); rebuildAll(); return true;
}

void FusionMainWindow::applyProperties() {
    const TimelineEntry* entry = selectedEntry();
    if (!entry) return;
    const FeatureId id = entry->id;
    bool changed = document_.renameFeature(id, featureName_->text().trimmed().toStdString());
    document_.suppressFeature(id, suppressed_->isChecked());
    for (QDoubleSpinBox* box : {parameterA_, parameterB_, parameterC_, parameterD_}) {
        if (!box->isVisible()) continue;
        changed = document_.setFeatureParameter(id, box->property("parameterKey").toString().toStdString(), box->value()) || changed;
    }
    if (changed) setDirty(true);
    rebuildAll();
}

void FusionMainWindow::undo() {
    if (document_.undo()) { setDirty(true); rebuildAll(); }
}

void FusionMainWindow::redo() {
    if (document_.redo()) { setDirty(true); rebuildAll(); }
}

bool FusionMainWindow::saveDocument(const QString& path) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    const QByteArray data = QByteArray::fromStdString(document_.serializeNative());
    if (file.write(data) != data.size() || !file.commit()) return false;
    filePath_ = path;
    setDirty(false);
    statusBar()->showMessage("Saved " + QFileInfo(path).fileName(), 3000);
    return true;
}

bool FusionMainWindow::openDocument(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    std::string error;
    FusionDocument loaded = FusionDocument::deserializeNative(file.readAll().toStdString(), &error);
    if (!error.empty() || (!loaded.regenerate() && !loaded.timeline().empty())) return false;
    document_ = std::move(loaded);
    document_.clearHistory();
    filePath_ = path;
    selectedId_ = document_.timeline().empty() ? 0 : document_.timeline().back().id;
    setDirty(false); rebuildAll(); viewport_->fitAll();
    return true;
}

bool FusionMainWindow::exportGCode(const QString& path) const {
    if (document_.camSetups().empty()) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    const QByteArray data = QByteArray::fromStdString(document_.exportGCode());
    return file.write(data) == data.size() && file.commit();
}

void FusionMainWindow::newDocument() {
    if (!maybeSave()) return;
    document_ = FusionDocument{}; selectedId_ = 0; filePath_.clear(); setDirty(false); rebuildAll(); viewport_->fitAll();
}

void FusionMainWindow::openDialog() {
    if (!maybeSave()) return;
    const QString path = QFileDialog::getOpenFileName(this, "Open Fusion Project", {}, "MyDesk Fusion (*.myfusion)");
    if (!path.isEmpty() && !openDocument(path)) QMessageBox::warning(this, "Open failed", "The project could not be opened or regenerated.");
}

void FusionMainWindow::save() { if (filePath_.isEmpty()) saveAs(); else saveDocument(filePath_); }
void FusionMainWindow::saveAs() {
    const QString path = QFileDialog::getSaveFileName(this, "Save Fusion Project", filePath_, "MyDesk Fusion (*.myfusion)");
    if (!path.isEmpty()) saveDocument(path.endsWith(".myfusion") ? path : path + ".myfusion");
}
void FusionMainWindow::exportDialog() {
    const QString path = QFileDialog::getSaveFileName(this, "Post Process G-code", {}, "G-code (*.nc *.gcode)");
    if (!path.isEmpty() && !exportGCode(path)) QMessageBox::warning(this, "Export failed", "Generate a valid CAM operation before posting G-code.");
}

bool FusionMainWindow::maybeSave() {
    if (!dirty_) return true;
    const auto choice = QMessageBox::question(this, "Unsaved project", "Save changes to this Fusion project?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (choice == QMessageBox::Cancel) return false;
    if (choice == QMessageBox::Save) { save(); return !dirty_; }
    return true;
}

void FusionMainWindow::setDirty(bool dirty) {
    dirty_ = dirty;
    setWindowModified(dirty);
    setWindowTitle(QString("MyDesk Fusion — %1[*]").arg(filePath_.isEmpty() ? "Untitled" : QFileInfo(filePath_).fileName()));
}

void FusionMainWindow::closeEvent(QCloseEvent* event) {
    if (!maybeSave()) { event->ignore(); return; }
    QSettings settings("MyDesk", "Fusion");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    event->accept();
}
