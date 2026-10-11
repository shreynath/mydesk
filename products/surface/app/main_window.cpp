// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kKindRole = Qt::UserRole;
constexpr int kIdRole = Qt::UserRole + 1;

QString orderName(mydesk::surface::ContinuityOrder order) {
    switch (order) {
        case mydesk::surface::ContinuityOrder::G0: return QStringLiteral("G0 positional");
        case mydesk::surface::ContinuityOrder::G1: return QStringLiteral("G1 tangent");
        case mydesk::surface::ContinuityOrder::G2: return QStringLiteral("G2 curvature");
    }
    return QStringLiteral("G0");
}
}

SurfaceMainWindow::SurfaceMainWindow(QWidget* parent) : QMainWindow(parent) {
    setObjectName("surfaceMainWindow");
    buildInterface();
    QSettings settings;
    resize(settings.value("window/size", QSize(1280, 800)).toSize());
    restoreState(settings.value("window/state").toByteArray());
    viewport_->setDocument(&document_);
    rebuildBrowser();
    setModified(false);
}

void SurfaceMainWindow::buildInterface() {
    setWindowTitle("MyDesk Surface — Untitled");
    viewport_ = new SurfaceViewport(this);
    setCentralWidget(viewport_);
    connect(viewport_, &SurfaceViewport::entitySelected, this, &SurfaceMainWindow::viewportSelectionChanged);

    auto* fileMenu = menuBar()->addMenu("&File");
    auto* newAction = fileMenu->addAction("&New", QKeySequence::New, this, &SurfaceMainWindow::newDocument);
    newAction->setObjectName("newDocumentAction");
    fileMenu->addAction("&Open…", QKeySequence::Open, this, &SurfaceMainWindow::openDialog);
    fileMenu->addAction("&Save", QKeySequence::Save, this, &SurfaceMainWindow::save);
    fileMenu->addAction("Save &As…", QKeySequence::SaveAs, this, &SurfaceMainWindow::saveAs);
    fileMenu->addSeparator();
    auto* exportAction = fileMenu->addAction("Export IGES &Preview…", this, &SurfaceMainWindow::exportIgesPreview);
    exportAction->setToolTip("Writes a diagnostic placeholder, not a conforming IGES exchange file");

    auto* editMenu = menuBar()->addMenu("&Edit");
    undoAction_ = editMenu->addAction("&Undo", QKeySequence::Undo, this, &SurfaceMainWindow::undo);
    redoAction_ = editMenu->addAction("&Redo", QKeySequence::Redo, this, &SurfaceMainWindow::redo);

    auto* createMenu = menuBar()->addMenu("&Create");
    auto* curveAction = createMenu->addAction("Cubic NURBS &Curve", this, &SurfaceMainWindow::createCurve);
    curveAction->setObjectName("createCurveAction");
    auto* patchAction = createMenu->addAction("&Bilinear Patch", this, &SurfaceMainWindow::createBilinearPatch);
    patchAction->setObjectName("createPatchAction");
    auto* bicubicAction = createMenu->addAction("&Bicubic Class-A Patch", this, &SurfaceMainWindow::createBicubicPatch);
    bicubicAction->setObjectName("createBicubicPatchAction");

    auto* viewMenu = menuBar()->addMenu("&View");
    auto* netAction = viewMenu->addAction("Control &Net"); netAction->setCheckable(true); netAction->setChecked(true);
    auto* zebraAction = viewMenu->addAction("&Zebra Diagnostic"); zebraAction->setCheckable(true);
    connect(netAction, &QAction::toggled, viewport_, &SurfaceViewport::setControlNetVisible);
    connect(zebraAction, &QAction::toggled, viewport_, &SurfaceViewport::setZebraVisible);

    auto* toolbar = addToolBar("Surface tools");
    toolbar->setObjectName("surfaceTools");
    toolbar->setMovable(false);
    toolbar->addAction(curveAction); toolbar->addAction(patchAction); toolbar->addAction(bicubicAction);
    toolbar->addSeparator(); toolbar->addAction(undoAction_); toolbar->addAction(redoAction_);

    auto* browserDock = new QDockWidget("Surface Browser", this);
    browserDock->setObjectName("surfaceBrowserDock");
    browser_ = new QTreeWidget(browserDock);
    browser_->setObjectName("surfaceBrowser");
    browser_->setHeaderLabels({"Geometry", "Type"});
    browserDock->setWidget(browser_);
    addDockWidget(Qt::LeftDockWidgetArea, browserDock);
    connect(browser_, &QTreeWidget::itemSelectionChanged, this, &SurfaceMainWindow::treeSelectionChanged);

    auto* propertiesDock = new QDockWidget("Object / CV Properties", this);
    propertiesDock->setObjectName("surfacePropertiesDock");
    auto* properties = new QWidget(propertiesDock);
    auto* propertiesLayout = new QVBoxLayout(properties);
    auto* form = new QFormLayout;
    entityType_ = new QLabel("No selection", properties);
    entityName_ = new QLineEdit(properties); entityName_->setObjectName("entityNameEdit");
    degreeLabel_ = new QLabel("—", properties);
    controlPointCombo_ = new QComboBox(properties); controlPointCombo_->setObjectName("controlPointCombo");
    form->addRow("Entity", entityType_); form->addRow("Name", entityName_); form->addRow("Degree", degreeLabel_);
    form->addRow("Control vertex", controlPointCombo_);
    propertiesLayout->addLayout(form);
    auto* cvGroup = new QGroupBox("Control vertex coordinates", properties);
    auto* cvForm = new QFormLayout(cvGroup);
    auto makeSpin = [&](const char* name) {
        auto* spin = new QDoubleSpinBox(cvGroup); spin->setObjectName(name); spin->setRange(-1000000.0, 1000000.0);
        spin->setDecimals(5); spin->setSingleStep(1.0); return spin;
    };
    xSpin_ = makeSpin("controlXSpin"); ySpin_ = makeSpin("controlYSpin"); zSpin_ = makeSpin("controlZSpin");
    weightSpin_ = makeSpin("controlWeightSpin"); weightSpin_->setRange(0.00001, 100000.0); weightSpin_->setValue(1.0);
    cvForm->addRow("X", xSpin_); cvForm->addRow("Y", ySpin_); cvForm->addRow("Z", zSpin_); cvForm->addRow("Weight", weightSpin_);
    propertiesLayout->addWidget(cvGroup);
    auto* apply = new QPushButton("Apply object / CV edit", properties); apply->setObjectName("applyPropertiesButton");
    propertiesLayout->addWidget(apply); propertiesLayout->addStretch();
    propertiesDock->setWidget(properties); addDockWidget(Qt::RightDockWidgetArea, propertiesDock);
    connect(apply, &QPushButton::clicked, this, &SurfaceMainWindow::applyProperties);
    connect(controlPointCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        selectedControlPoint_ = index;
        refreshSelection();
    });

    auto* seamDock = new QDockWidget("Seam Continuity", this);
    seamDock->setObjectName("seamContinuityDock");
    auto* seamWidget = new QWidget(seamDock); auto* seamLayout = new QVBoxLayout(seamWidget); auto* seamForm = new QFormLayout;
    seamA_ = new QComboBox(seamWidget); seamA_->setObjectName("seamPatchACombo");
    seamB_ = new QComboBox(seamWidget); seamB_->setObjectName("seamPatchBCombo");
    continuityOrder_ = new QComboBox(seamWidget); continuityOrder_->setObjectName("continuityOrderCombo");
    continuityOrder_->addItems({"G0 position", "G1 tangent", "G2 curvature"});
    seamForm->addRow("Source U=1", seamA_); seamForm->addRow("Target U=0", seamB_); seamForm->addRow("Alignment", continuityOrder_);
    seamLayout->addLayout(seamForm);
    auto* inspect = new QPushButton("Inspect seam", seamWidget); inspect->setObjectName("inspectSeamButton");
    auto* align = new QPushButton("Align target control rows", seamWidget); align->setObjectName("alignSeamButton");
    diagnosticLabel_ = new QLabel("Choose two patches. Diagnostics sample the shared U boundary.", seamWidget);
    diagnosticLabel_->setObjectName("seamDiagnosticLabel"); diagnosticLabel_->setWordWrap(true);
    seamLayout->addWidget(inspect); seamLayout->addWidget(align); seamLayout->addWidget(diagnosticLabel_); seamLayout->addStretch();
    seamDock->setWidget(seamWidget); addDockWidget(Qt::BottomDockWidgetArea, seamDock);
    connect(inspect, &QPushButton::clicked, this, &SurfaceMainWindow::inspectSeam);
    connect(align, &QPushButton::clicked, this, &SurfaceMainWindow::alignSeam);

    statusBar()->showMessage("Ready — native .mysurface precision document");
}

QString SurfaceMainWindow::displayName() const {
    return filePath_.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(filePath_).fileName();
}

void SurfaceMainWindow::setModified(bool modified) {
    modified_ = modified;
    setWindowModified(modified);
    setWindowTitle(QStringLiteral("MyDesk Surface — %1[*]").arg(displayName()));
    refreshActions();
}

void SurfaceMainWindow::refreshActions() {
    undoAction_->setEnabled(document_.canUndo());
    redoAction_->setEnabled(document_.canRedo());
}

void SurfaceMainWindow::rebuildBrowser() {
    const QSignalBlocker blocker(browser_);
    browser_->clear();
    auto* curves = new QTreeWidgetItem(browser_, {"Curves", QString::number(document_.curves().size())});
    curves->setData(0, kKindRole, static_cast<int>(ItemKind::Root));
    for (const auto& curve : document_.curves()) {
        auto* item = new QTreeWidgetItem(curves, {QString::fromStdString(curve.name), QStringLiteral("NURBS %1 CV").arg(curve.controlPoints.size())});
        item->setData(0, kKindRole, static_cast<int>(ItemKind::Curve)); item->setData(0, kIdRole, QVariant::fromValue<qulonglong>(curve.id));
        if (selectedKind_ == SurfaceViewport::EntityKind::Curve && selectedId_ == curve.id) browser_->setCurrentItem(item);
    }
    auto* surfaces = new QTreeWidgetItem(browser_, {"Surfaces", QString::number(document_.surfaces().size())});
    surfaces->setData(0, kKindRole, static_cast<int>(ItemKind::Root));
    for (const auto& surface : document_.surfaces()) {
        auto* item = new QTreeWidgetItem(surfaces, {QString::fromStdString(surface.name), QStringLiteral("%1 × %2 CV").arg(surface.numU).arg(surface.numV)});
        item->setData(0, kKindRole, static_cast<int>(ItemKind::Surface)); item->setData(0, kIdRole, QVariant::fromValue<qulonglong>(surface.id));
        if (selectedKind_ == SurfaceViewport::EntityKind::Surface && selectedId_ == surface.id) browser_->setCurrentItem(item);
    }
    browser_->expandAll(); browser_->resizeColumnToContents(0);
    rebuildSeamSelectors();
    refreshSelection();
    viewport_->update();
}

void SurfaceMainWindow::rebuildSeamSelectors() {
    const auto a = seamA_->currentData().toULongLong(), b = seamB_->currentData().toULongLong();
    const QSignalBlocker blockA(seamA_), blockB(seamB_); seamA_->clear(); seamB_->clear();
    for (const auto& surface : document_.surfaces()) {
        const QString name = QString::fromStdString(surface.name);
        seamA_->addItem(name, QVariant::fromValue<qulonglong>(surface.id)); seamB_->addItem(name, QVariant::fromValue<qulonglong>(surface.id));
    }
    int ai = seamA_->findData(a), bi = seamB_->findData(b);
    seamA_->setCurrentIndex(ai >= 0 ? ai : 0); seamB_->setCurrentIndex(bi >= 0 ? bi : std::min(1, seamB_->count()-1));
}

void SurfaceMainWindow::refreshSelection() {
    const QSignalBlocker blocker(controlPointCombo_);
    entityName_->setEnabled(false); controlPointCombo_->setEnabled(false); controlPointCombo_->clear();
    mydesk::surface::WeightedPoint point; bool hasPoint = false;
    if (selectedKind_ == SurfaceViewport::EntityKind::Curve) {
        if (const auto* curve = document_.findCurve(selectedId_)) {
            entityType_->setText("NURBS curve"); entityName_->setText(QString::fromStdString(curve->name)); entityName_->setEnabled(true);
            degreeLabel_->setText(QStringLiteral("%1").arg(curve->degree)); controlPointCombo_->setEnabled(true);
            for (std::size_t i=0;i<curve->controlPoints.size();++i) controlPointCombo_->addItem(QStringLiteral("CV %1").arg(i));
            selectedControlPoint_ = std::clamp(selectedControlPoint_, 0, static_cast<int>(curve->controlPoints.size())-1);
            if (!curve->controlPoints.empty()) { point = curve->controlPoints[static_cast<std::size_t>(selectedControlPoint_)]; hasPoint = true; }
        }
    } else if (selectedKind_ == SurfaceViewport::EntityKind::Surface) {
        if (const auto* surface = document_.findSurface(selectedId_)) {
            entityType_->setText("NURBS surface patch"); entityName_->setText(QString::fromStdString(surface->name)); entityName_->setEnabled(true);
            degreeLabel_->setText(QStringLiteral("%1 × %2").arg(surface->degreeU).arg(surface->degreeV)); controlPointCombo_->setEnabled(true);
            for (std::size_t i=0;i<surface->controlGrid.size();++i)
                controlPointCombo_->addItem(QStringLiteral("CV [%1,%2]").arg(i/surface->numV).arg(i%surface->numV));
            selectedControlPoint_ = std::clamp(selectedControlPoint_, 0, static_cast<int>(surface->controlGrid.size())-1);
            if (!surface->controlGrid.empty()) { point = surface->controlGrid[static_cast<std::size_t>(selectedControlPoint_)]; hasPoint = true; }
            const auto d = surface->derivativesAt(0.5, 0.5);
            diagnosticLabel_->setText(QStringLiteral("Selected patch center: K=%1  H=%2  normal=(%3, %4, %5)")
                .arg(surface->gaussianCurvatureAt(0.5,0.5), 0, 'g', 5).arg(surface->meanCurvatureAt(0.5,0.5),0,'g',5)
                .arg(d.du.cross(d.dv).normalized().x,0,'f',3).arg(d.du.cross(d.dv).normalized().y,0,'f',3).arg(d.du.cross(d.dv).normalized().z,0,'f',3));
        }
    } else {
        entityType_->setText("No selection"); entityName_->clear(); degreeLabel_->setText("—"); selectedControlPoint_ = -1;
    }
    if (hasPoint) {
        controlPointCombo_->setCurrentIndex(selectedControlPoint_); xSpin_->setValue(point.p.x); ySpin_->setValue(point.p.y);
        zSpin_->setValue(point.p.z); weightSpin_->setValue(point.w);
    }
    for (auto* spin : {xSpin_, ySpin_, zSpin_, weightSpin_}) spin->setEnabled(hasPoint);
    viewport_->setSelection(selectedKind_, selectedId_, selectedControlPoint_);
}

void SurfaceMainWindow::viewportSelectionChanged(SurfaceViewport::EntityKind kind, std::uint64_t id, int controlPoint) {
    selectedKind_ = kind; selectedId_ = id; selectedControlPoint_ = controlPoint < 0 ? 0 : controlPoint; rebuildBrowser();
}

void SurfaceMainWindow::treeSelectionChanged() {
    const auto items = browser_->selectedItems(); if (items.isEmpty()) return;
    const auto kind = static_cast<ItemKind>(items.front()->data(0, kKindRole).toInt());
    selectedId_ = items.front()->data(0, kIdRole).toULongLong(); selectedControlPoint_ = 0;
    selectedKind_ = kind == ItemKind::Curve ? SurfaceViewport::EntityKind::Curve :
                    kind == ItemKind::Surface ? SurfaceViewport::EntityKind::Surface : SurfaceViewport::EntityKind::None;
    refreshSelection();
}

void SurfaceMainWindow::selectSurface(mydesk::surface::SurfacePatchId id, int controlPoint) {
    selectedKind_ = SurfaceViewport::EntityKind::Surface; selectedId_ = id; selectedControlPoint_ = controlPoint < 0 ? 0 : controlPoint; rebuildBrowser();
}

void SurfaceMainWindow::createCurve() {
    const double offset = static_cast<double>(document_.curves().size()) * 12.0;
    selectedId_ = document_.addCubicCurve("Feature Curve " + std::to_string(document_.curves().size()+1),
        {{0,offset,0},{25,offset+14,8},{55,offset-10,12},{85,offset,0}});
    selectedKind_ = SurfaceViewport::EntityKind::Curve; selectedControlPoint_ = 0; setModified(true); viewport_->setDocument(&document_); rebuildBrowser();
}

void SurfaceMainWindow::createBilinearPatch() {
    const double offset = static_cast<double>(document_.surfaces().size()) * 32.0;
    selectedId_ = document_.addBilinearPatch("Patch " + std::to_string(document_.surfaces().size()+1),
        {offset,0,0},{offset+30,0,0},{offset,24,0},{offset+30,24,4});
    selectedKind_ = SurfaceViewport::EntityKind::Surface; selectedControlPoint_ = 0; setModified(true); viewport_->setDocument(&document_); rebuildBrowser();
}

void SurfaceMainWindow::createBicubicPatch() {
    const double offset = static_cast<double>(document_.surfaces().size()) * 31.0;
    std::vector<mydesk::surface::Vec3> grid; grid.reserve(16);
    for (int i=0;i<4;++i) for (int j=0;j<4;++j) grid.push_back({offset+10.0*i, 10.0*j, 5.0*std::sin(i*1.047)*std::sin(j*1.047)});
    selectedId_ = document_.addBicubicPatch("Class-A Patch " + std::to_string(document_.surfaces().size()+1), grid);
    selectedKind_ = SurfaceViewport::EntityKind::Surface; selectedControlPoint_ = 0; setModified(true); viewport_->setDocument(&document_); rebuildBrowser();
}

void SurfaceMainWindow::applyProperties() {
    bool changed = false;
    const std::string name = entityName_->text().trimmed().toStdString();
    const mydesk::surface::WeightedPoint point{{xSpin_->value(),ySpin_->value(),zSpin_->value()},weightSpin_->value()};
    if (selectedKind_ == SurfaceViewport::EntityKind::Curve) {
        if (const auto* curve = document_.findCurve(selectedId_); curve && curve->name != name) changed |= document_.renameCurve(selectedId_, name);
        if (selectedControlPoint_ >= 0) changed |= document_.setCurveControlPoint(selectedId_, static_cast<std::size_t>(selectedControlPoint_), point);
    } else if (selectedKind_ == SurfaceViewport::EntityKind::Surface) {
        if (const auto* surface = document_.findSurface(selectedId_); surface && surface->name != name) changed |= document_.renameSurface(selectedId_, name);
        if (selectedControlPoint_ >= 0) changed |= document_.setSurfaceControlPoint(selectedId_, static_cast<std::size_t>(selectedControlPoint_), point);
    }
    if (changed) { setModified(true); rebuildBrowser(); statusBar()->showMessage("Control vertex edit applied", 3000); }
}

void SurfaceMainWindow::inspectSeam() {
    const auto a = seamA_->currentData().toULongLong(), b = seamB_->currentData().toULongLong();
    if (!a || !b || a == b) { diagnosticLabel_->setText("Choose two different surface patches."); return; }
    const auto report = document_.checkSeamContinuity(a,b,25);
    diagnosticLabel_->setText(QStringLiteral("%1 sampled: gap %2 mm; normal angle %3°; curvature deviation %4. Position tolerance: %5")
        .arg(orderName(report.achieved)).arg(report.maxPositionalGap,0,'g',6).arg(report.maxAngularDeviationDeg,0,'g',6)
        .arg(report.maxCurvatureDeviation,0,'g',6).arg(report.passesTolerance ? "PASS" : "FAIL"));
}

void SurfaceMainWindow::alignSeam() {
    const auto a = seamA_->currentData().toULongLong(), b = seamB_->currentData().toULongLong();
    const auto order = static_cast<mydesk::surface::ContinuityOrder>(continuityOrder_->currentIndex());
    if (!document_.alignSeam(a,b,order)) {
        diagnosticLabel_->setText("Alignment requires two different non-rational bicubic Bezier patches sharing the source U=1 / target U=0 topology."); return;
    }
    setModified(true); viewport_->update(); rebuildBrowser(); inspectSeam();
}

void SurfaceMainWindow::undo() { if (document_.undo()) { setModified(true); rebuildBrowser(); } }
void SurfaceMainWindow::redo() { if (document_.redo()) { setModified(true); rebuildBrowser(); } }

bool SurfaceMainWindow::saveDocument(const QString& path) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    const QByteArray data = QByteArray::fromStdString(document_.serializeNative());
    if (file.write(data) != data.size() || !file.commit()) return false;
    filePath_ = path; setModified(false); statusBar()->showMessage(QStringLiteral("Saved %1").arg(path), 3000); return true;
}

bool SurfaceMainWindow::openDocument(const QString& path) {
    QFile file(path); if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    std::string error; auto loaded = mydesk::surface::SurfaceDocument::deserializeNative(file.readAll().toStdString(), &error);
    if (!error.empty()) { statusBar()->showMessage(QString::fromStdString(error), 5000); return false; }
    document_ = std::move(loaded); filePath_ = path; selectedKind_ = SurfaceViewport::EntityKind::None; selectedId_ = 0; selectedControlPoint_ = -1;
    viewport_->setDocument(&document_); rebuildBrowser(); setModified(false); return true;
}

bool SurfaceMainWindow::maybeSave() {
    if (!modified_) return true;
    const auto choice = QMessageBox::warning(this, "Unsaved surface document", "Save changes before continuing?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Cancel) return false;
    if (choice == QMessageBox::Save) { save(); return !modified_; }
    return true;
}

void SurfaceMainWindow::newDocument() {
    if (!maybeSave()) return; document_ = {}; filePath_.clear(); selectedKind_ = SurfaceViewport::EntityKind::None; selectedId_ = 0;
    viewport_->setDocument(&document_); rebuildBrowser(); setModified(false);
}

void SurfaceMainWindow::openDialog() {
    if (!maybeSave()) return;
    const QString path = QFileDialog::getOpenFileName(this, "Open MyDesk Surface document", {}, "MyDesk Surface (*.mysurface)");
    if (!path.isEmpty() && !openDocument(path)) QMessageBox::critical(this, "Open failed", "The file is not a valid supported .mysurface document.");
}

void SurfaceMainWindow::save() { if (filePath_.isEmpty()) saveAs(); else if (!saveDocument(filePath_)) QMessageBox::critical(this,"Save failed","The document could not be written atomically."); }
void SurfaceMainWindow::saveAs() {
    QString path = QFileDialog::getSaveFileName(this,"Save MyDesk Surface document",filePath_,"MyDesk Surface (*.mysurface)");
    if (path.isEmpty()) return; if (!path.endsWith(".mysurface",Qt::CaseInsensitive)) path += ".mysurface";
    if (!saveDocument(path)) QMessageBox::critical(this,"Save failed","The document could not be written atomically.");
}

void SurfaceMainWindow::exportIgesPreview() {
    QString path = QFileDialog::getSaveFileName(this,"Export diagnostic IGES preview",{},"IGES preview (*.igs)"); if (path.isEmpty()) return;
    QFile file(path); if (!file.open(QIODevice::WriteOnly|QIODevice::Text) || file.write(QByteArray::fromStdString(document_.exportIges())) < 0)
        QMessageBox::critical(this,"Export failed","The preview could not be written.");
    else QMessageBox::information(this,"IGES preview written","This is a diagnostic Entity-128 preview, not a conforming production IGES exchange file.");
}

void SurfaceMainWindow::closeEvent(QCloseEvent* event) {
    if (!maybeSave()) { event->ignore(); return; }
    QSettings settings; settings.setValue("window/size",size()); settings.setValue("window/state",saveState()); event->accept();
}
