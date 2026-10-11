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
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr int roleKind = Qt::UserRole;
constexpr int roleId = Qt::UserRole + 1;

QDoubleSpinBox* makeCoordinateEditor(const QString& name) {
    auto* editor = new QDoubleSpinBox;
    editor->setObjectName(name);
    editor->setDecimals(4);
    editor->setRange(-1000000000.0, 1000000000.0);
    editor->setSingleStep(1.0);
    return editor;
}

QWidget* wrapForm(QFormLayout* form) {
    auto* widget = new QWidget;
    auto* layout = new QVBoxLayout(widget);
    layout->addLayout(form);
    layout->addStretch();
    return widget;
}
}

CivilMainWindow::CivilMainWindow(QWidget* parent) : QMainWindow(parent) {
    setObjectName("civilMainWindow");
    resize(1420, 880);
    setMinimumSize(960, 640);
    buildUi();
    buildMenus();
    newDocument();
    dirty_ = false;
    undoStack_.clear();
    redoStack_.clear();
    updateActions();
    const QSettings settings;
    restoreGeometry(settings.value("civil/windowGeometry").toByteArray());
    restoreState(settings.value("civil/windowState").toByteArray());
}

void CivilMainWindow::buildUi() {
    viewport_ = new CivilViewport(this);
    setCentralWidget(viewport_);

    auto* browserDock = new QDockWidget(tr("Civil Project Browser"), this);
    browserDock->setObjectName("civilProjectBrowserDock");
    projectTree_ = new QTreeWidget(browserDock);
    projectTree_->setObjectName("civilProjectTree");
    projectTree_->setHeaderLabels({tr("Object"), tr("Data")});
    projectTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    projectTree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    projectTree_->setAlternatingRowColors(true);
    browserDock->setWidget(projectTree_);
    addDockWidget(Qt::LeftDockWidgetArea, browserDock);

    auto* propertyDock = new QDockWidget(tr("Civil Properties"), this);
    propertyDock->setObjectName("civilPropertiesDock");
    auto* propertyRoot = new QWidget(propertyDock);
    auto* propertyLayout = new QVBoxLayout(propertyRoot);
    selectionSummary_ = new QLabel(tr("No civil object selected"));
    selectionSummary_->setObjectName("civilSelectionSummary");
    selectionSummary_->setWordWrap(true);
    propertyLayout->addWidget(selectionSummary_);
    propertyStack_ = new QStackedWidget;
    propertyStack_->setObjectName("civilPropertyStack");
    propertyStack_->addWidget(new QLabel(tr("Select a COGO point, TIN surface, alignment, or profile.")));

    auto* pointForm = new QFormLayout;
    pointNumber_ = new QLineEdit; pointNumber_->setObjectName("pointNumberEditor");
    pointDescription_ = new QLineEdit; pointDescription_->setObjectName("pointDescriptionEditor");
    pointEasting_ = makeCoordinateEditor("pointEastingEditor");
    pointNorthing_ = makeCoordinateEditor("pointNorthingEditor");
    pointElevation_ = makeCoordinateEditor("pointElevationEditor");
    pointForm->addRow(tr("Point number"), pointNumber_);
    pointForm->addRow(tr("Easting"), pointEasting_);
    pointForm->addRow(tr("Northing"), pointNorthing_);
    pointForm->addRow(tr("Elevation"), pointElevation_);
    pointForm->addRow(tr("Raw description"), pointDescription_);
    propertyStack_->addWidget(wrapForm(pointForm));

    auto* surfaceForm = new QFormLayout;
    surfaceName_ = new QLineEdit; surfaceName_->setObjectName("surfaceNameEditor");
    surfaceStatistics_ = new QLabel; surfaceStatistics_->setWordWrap(true);
    surfaceDatum_ = makeCoordinateEditor("surfaceDatumEditor");
    contourInterval_ = makeCoordinateEditor("contourIntervalEditor");
    contourInterval_->setRange(0.001, 1000000.0); contourInterval_->setValue(1.0);
    contourVisible_ = new QCheckBox(tr("Display contours")); contourVisible_->setChecked(true);
    earthworkResult_ = new QLabel(tr("Set a datum and analyze cut/fill."));
    earthworkResult_->setWordWrap(true);
    auto* analyzeButton = new QPushButton(tr("Calculate Earthwork"));
    analyzeButton->setObjectName("analyzeSurfaceButton");
    connect(analyzeButton, &QPushButton::clicked, this, &CivilMainWindow::analyzeSurface);
    connect(contourVisible_, &QCheckBox::toggled, viewport_, &CivilViewport::setContoursVisible);
    connect(contourInterval_, &QDoubleSpinBox::valueChanged, viewport_, &CivilViewport::setContourInterval);
    surfaceForm->addRow(tr("Surface name"), surfaceName_);
    surfaceForm->addRow(tr("Definition"), surfaceStatistics_);
    surfaceForm->addRow(tr("Contour interval"), contourInterval_);
    surfaceForm->addRow(QString(), contourVisible_);
    surfaceForm->addRow(tr("Volume datum"), surfaceDatum_);
    surfaceForm->addRow(QString(), analyzeButton);
    surfaceForm->addRow(tr("Earthwork"), earthworkResult_);
    propertyStack_->addWidget(wrapForm(surfaceForm));

    auto* alignmentForm = new QFormLayout;
    alignmentName_ = new QLineEdit; alignmentName_->setObjectName("alignmentNameEditor");
    alignmentStartStation_ = makeCoordinateEditor("alignmentStartStationEditor");
    alignmentStatistics_ = new QLabel; alignmentStatistics_->setWordWrap(true);
    inquiryEasting_ = makeCoordinateEditor("inquiryEastingEditor");
    inquiryNorthing_ = makeCoordinateEditor("inquiryNorthingEditor");
    inquiryResult_ = new QLabel(tr("Enter a coordinate for station/offset inquiry."));
    inquiryResult_->setWordWrap(true);
    auto* inquiryButton = new QPushButton(tr("Compute Station / Offset"));
    inquiryButton->setObjectName("stationOffsetButton");
    connect(inquiryButton, &QPushButton::clicked, this, &CivilMainWindow::inquireStationOffset);
    alignmentForm->addRow(tr("Alignment name"), alignmentName_);
    alignmentForm->addRow(tr("Starting station"), alignmentStartStation_);
    alignmentForm->addRow(tr("Geometry"), alignmentStatistics_);
    alignmentForm->addRow(tr("Inquiry easting"), inquiryEasting_);
    alignmentForm->addRow(tr("Inquiry northing"), inquiryNorthing_);
    alignmentForm->addRow(QString(), inquiryButton);
    alignmentForm->addRow(tr("Result"), inquiryResult_);
    propertyStack_->addWidget(wrapForm(alignmentForm));

    auto* profileForm = new QFormLayout;
    profileName_ = new QLineEdit; profileName_->setObjectName("profileNameEditor");
    profileStatistics_ = new QLabel; profileStatistics_->setWordWrap(true);
    profileForm->addRow(tr("Profile name"), profileName_);
    profileForm->addRow(tr("Vertical definition"), profileStatistics_);
    propertyStack_->addWidget(wrapForm(profileForm));
    propertyLayout->addWidget(propertyStack_, 1);
    auto* applyButton = new QPushButton(tr("Apply Properties"));
    applyButton->setObjectName("applyCivilPropertiesButton");
    connect(applyButton, &QPushButton::clicked, this, &CivilMainWindow::applyProperties);
    propertyLayout->addWidget(applyButton);
    propertyDock->setWidget(propertyRoot);
    addDockWidget(Qt::RightDockWidgetArea, propertyDock);

    connect(projectTree_, &QTreeWidget::itemSelectionChanged, this, [this] {
        const auto items = projectTree_->selectedItems();
        if (items.empty() || !items.front()->data(0, roleKind).isValid()) return;
        selectObject(static_cast<SelectionKind>(items.front()->data(0, roleKind).toInt()),
                     items.front()->data(0, roleId).toULongLong());
    });
    connect(viewport_, &CivilViewport::entitySelected, this,
        [this](CivilViewport::EntityKind kind, std::uint64_t id) {
            selectObject(static_cast<SelectionKind>(kind), id);
        });
    connect(viewport_, &CivilViewport::coordinateHovered, this, [this](double e, double n) {
        statusBar()->showMessage(tr("E %1   N %2").arg(e, 0, 'f', 3).arg(n, 0, 'f', 3));
    });
}

void CivilMainWindow::buildMenus() {
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto* newAction = fileMenu->addAction(tr("&New Project")); newAction->setShortcut(QKeySequence::New);
    auto* openAction = fileMenu->addAction(tr("&Open…")); openAction->setShortcut(QKeySequence::Open);
    saveAction_ = fileMenu->addAction(tr("&Save")); saveAction_->setShortcut(QKeySequence::Save);
    auto* saveAsAction = fileMenu->addAction(tr("Save &As…")); saveAsAction->setShortcut(QKeySequence::SaveAs);
    fileMenu->addSeparator();
    auto* exportAction = fileMenu->addAction(tr("Export LandXML…"));
    fileMenu->addSeparator();
    auto* exitAction = fileMenu->addAction(tr("E&xit")); exitAction->setShortcut(QKeySequence::Quit);
    connect(newAction, &QAction::triggered, this, &CivilMainWindow::newDocument);
    connect(openAction, &QAction::triggered, this, &CivilMainWindow::openInteractive);
    connect(saveAction_, &QAction::triggered, this, &CivilMainWindow::saveCurrent);
    connect(saveAsAction, &QAction::triggered, this, &CivilMainWindow::saveAs);
    connect(exportAction, &QAction::triggered, this, &CivilMainWindow::exportInteractive);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu(tr("&Edit"));
    undoAction_ = editMenu->addAction(tr("&Undo")); undoAction_->setShortcut(QKeySequence::Undo);
    redoAction_ = editMenu->addAction(tr("&Redo")); redoAction_->setShortcut(QKeySequence::Redo);
    connect(undoAction_, &QAction::triggered, this, &CivilMainWindow::undo);
    connect(redoAction_, &QAction::triggered, this, &CivilMainWindow::redo);

    auto* createMenu = menuBar()->addMenu(tr("&Create"));
    auto* pointAction = createMenu->addAction(tr("COGO Point"));
    auto* surfaceAction = createMenu->addAction(tr("TIN Surface from Points"));
    auto* alignmentAction = createMenu->addAction(tr("Horizontal Alignment"));
    auto* profileAction = createMenu->addAction(tr("Design Profile"));
    connect(pointAction, &QAction::triggered, this, &CivilMainWindow::createPointInteractive);
    connect(surfaceAction, &QAction::triggered, this, &CivilMainWindow::createSurfaceInteractive);
    connect(alignmentAction, &QAction::triggered, this, &CivilMainWindow::createAlignmentInteractive);
    connect(profileAction, &QAction::triggered, this, &CivilMainWindow::createProfileInteractive);

    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    auto* fitAction = viewMenu->addAction(tr("Fit Project")); fitAction->setShortcut(Qt::Key_F);
    connect(fitAction, &QAction::triggered, viewport_, &CivilViewport::fitProject);

    auto* toolbar = addToolBar(tr("Civil Authoring"));
    toolbar->setObjectName("civilAuthoringToolbar");
    toolbar->addAction(pointAction); toolbar->addAction(surfaceAction);
    toolbar->addAction(alignmentAction); toolbar->addAction(profileAction);
    toolbar->addSeparator(); toolbar->addAction(undoAction_); toolbar->addAction(redoAction_);
}

void CivilMainWindow::newDocument() {
    if (!confirmDiscard()) return;
    document_ = {};
    document_.setName("Site Development");
    currentPath_.clear(); dirty_ = false; undoStack_.clear(); redoStack_.clear();
    selectionKind_ = SelectionKind::None; selectionId_ = 0;
    viewport_->setDocument(&document_);
    rebuildTree(); showSelection(); updateActions();
}

mydesk::civil::PointId CivilMainWindow::createPoint(std::string number, double easting,
    double northing, double elevation, std::string description) {
    pushUndo();
    const auto id = document_.addPoint(std::move(number), easting, northing, elevation,
                                       std::move(description));
    markChanged(); rebuildTree(); selectObject(SelectionKind::Point, id); viewport_->fitProject();
    return id;
}

void CivilMainWindow::createPointInteractive() {
    const auto index = document_.points().size() + 1;
    const double offset = static_cast<double>(index - 1) * 20.0;
    createPoint(std::to_string(index), offset, (index % 2 == 0 ? 25.0 : 0.0),
                100.0 + static_cast<double>(index), "TOPO");
}

mydesk::civil::SurfaceId CivilMainWindow::createSurface(std::string name) {
    if (document_.points().size() < 3) return 0;
    std::vector<mydesk::civil::PointId> ids;
    ids.reserve(document_.points().size());
    for (const auto& point : document_.points()) ids.push_back(point.id);
    pushUndo();
    const auto id = document_.buildTinSurface(std::move(name), ids);
    markChanged(); rebuildTree(); selectObject(SelectionKind::Surface, id);
    return id;
}

void CivilMainWindow::createSurfaceInteractive() {
    if (createSurface("Existing Ground") == 0) {
        showError(tr("At least three COGO points are required to build a TIN surface."));
    }
}

mydesk::civil::AlignmentId CivilMainWindow::createAlignment(std::string name) {
    double minE = 0.0, maxE = 100.0, centerN = 0.0;
    if (!document_.points().empty()) {
        minE = document_.points().front().easting; maxE = minE;
        double minN = document_.points().front().northing, maxN = minN;
        for (const auto& point : document_.points()) {
            minE = std::min(minE, point.easting); maxE = std::max(maxE, point.easting);
            minN = std::min(minN, point.northing); maxN = std::max(maxN, point.northing);
        }
        centerN = (minN + maxN) * 0.5;
        if (std::abs(maxE - minE) < 1e-6) maxE = minE + 100.0;
    }
    pushUndo();
    const auto id = document_.createAlignment(std::move(name), 0.0);
    document_.addAlignmentTangent(id, minE, centerN, maxE, centerN);
    markChanged(); rebuildTree(); selectObject(SelectionKind::Alignment, id); viewport_->fitProject();
    return id;
}

void CivilMainWindow::createAlignmentInteractive() { createAlignment("Site Centerline"); }

mydesk::civil::ProfileId CivilMainWindow::createProfile(std::string name) {
    if (document_.alignments().empty()) return 0;
    const auto& alignment = document_.alignments().front();
    pushUndo();
    const auto id = document_.createProfile(std::move(name), alignment.id);
    double startElevation = document_.points().empty() ? 100.0 : document_.points().front().elevation;
    document_.addProfilePVI(id, alignment.startStation, startElevation);
    document_.addProfilePVI(id, alignment.startStation + alignment.totalLength(),
                            startElevation + alignment.totalLength() * 0.02);
    markChanged(); rebuildTree(); selectObject(SelectionKind::Profile, id);
    return id;
}

void CivilMainWindow::createProfileInteractive() {
    if (createProfile("Finished Grade") == 0) showError(tr("Create an alignment before a profile."));
}

void CivilMainWindow::selectObject(SelectionKind kind, std::uint64_t id) {
    selectionKind_ = kind; selectionId_ = id;
    viewport_->setSelection(static_cast<CivilViewport::EntityKind>(kind), id);
    showSelection();
    const auto matches = projectTree_->findItems(QString(), Qt::MatchContains | Qt::MatchRecursive);
    for (auto* item : matches) {
        if (item->data(0, roleKind).toInt() == static_cast<int>(kind) &&
            item->data(0, roleId).toULongLong() == id) {
            projectTree_->setCurrentItem(item); break;
        }
    }
}

void CivilMainWindow::rebuildTree() {
    projectTree_->clear();
    auto addRoot = [this](const QString& label) {
        auto* root = new QTreeWidgetItem(projectTree_, {label}); root->setExpanded(true); return root;
    };
    auto* pointsRoot = addRoot(tr("COGO Points (%1)").arg(document_.points().size()));
    for (const auto& point : document_.points()) {
        auto* item = new QTreeWidgetItem(pointsRoot,
            {QString::fromStdString(point.pointNumber), tr("Z %1").arg(point.elevation, 0, 'f', 3)});
        item->setData(0, roleKind, static_cast<int>(SelectionKind::Point));
        item->setData(0, roleId, QVariant::fromValue<qulonglong>(point.id));
    }
    auto* surfacesRoot = addRoot(tr("TIN Surfaces (%1)").arg(document_.surfaces().size()));
    for (const auto& surface : document_.surfaces()) {
        auto* item = new QTreeWidgetItem(surfacesRoot, {QString::fromStdString(surface.name),
            tr("%1 faces").arg(surface.triangles.size())});
        item->setData(0, roleKind, static_cast<int>(SelectionKind::Surface));
        item->setData(0, roleId, QVariant::fromValue<qulonglong>(surface.id));
    }
    auto* alignmentsRoot = addRoot(tr("Alignments (%1)").arg(document_.alignments().size()));
    for (const auto& alignment : document_.alignments()) {
        auto* item = new QTreeWidgetItem(alignmentsRoot, {QString::fromStdString(alignment.name),
            tr("%1 m").arg(alignment.totalLength(), 0, 'f', 2)});
        item->setData(0, roleKind, static_cast<int>(SelectionKind::Alignment));
        item->setData(0, roleId, QVariant::fromValue<qulonglong>(alignment.id));
    }
    auto* profilesRoot = addRoot(tr("Profiles (%1)").arg(document_.profiles().size()));
    for (const auto& profile : document_.profiles()) {
        auto* item = new QTreeWidgetItem(profilesRoot, {QString::fromStdString(profile.name),
            tr("%1 PVIs").arg(profile.pvis.size())});
        item->setData(0, roleKind, static_cast<int>(SelectionKind::Profile));
        item->setData(0, roleId, QVariant::fromValue<qulonglong>(profile.id));
    }
}

void CivilMainWindow::showSelection() {
    propertyStack_->setCurrentIndex(static_cast<int>(selectionKind_));
    if (selectionKind_ == SelectionKind::Point) {
        if (const auto* point = document_.findPoint(selectionId_)) {
            selectionSummary_->setText(tr("COGO Point %1").arg(QString::fromStdString(point->pointNumber)));
            pointNumber_->setText(QString::fromStdString(point->pointNumber));
            pointEasting_->setValue(point->easting); pointNorthing_->setValue(point->northing);
            pointElevation_->setValue(point->elevation);
            pointDescription_->setText(QString::fromStdString(point->rawDescription));
        }
    } else if (selectionKind_ == SelectionKind::Surface) {
        if (const auto* surface = document_.findSurface(selectionId_)) {
            selectionSummary_->setText(tr("TIN Surface: %1").arg(QString::fromStdString(surface->name)));
            surfaceName_->setText(QString::fromStdString(surface->name));
            surfaceStatistics_->setText(tr("%1 source points • %2 Delaunay triangles")
                .arg(surface->points.size()).arg(surface->triangles.size()));
            if (!surface->points.empty()) surfaceDatum_->setValue(surface->points.front().elevation);
        }
    } else if (selectionKind_ == SelectionKind::Alignment) {
        if (const auto* alignment = document_.findAlignment(selectionId_)) {
            selectionSummary_->setText(tr("Horizontal Alignment: %1").arg(QString::fromStdString(alignment->name)));
            alignmentName_->setText(QString::fromStdString(alignment->name));
            alignmentStartStation_->setValue(alignment->startStation);
            alignmentStatistics_->setText(tr("%1 entities • %2 m • %3 to %4")
                .arg(alignment->segments.size()).arg(alignment->totalLength(), 0, 'f', 3)
                .arg(QString::fromStdString(alignment->formatStation(alignment->startStation)))
                .arg(QString::fromStdString(alignment->formatStation(alignment->startStation + alignment->totalLength()))));
        }
    } else if (selectionKind_ == SelectionKind::Profile) {
        if (const auto* profile = document_.findProfile(selectionId_)) {
            selectionSummary_->setText(tr("Design Profile: %1").arg(QString::fromStdString(profile->name)));
            profileName_->setText(QString::fromStdString(profile->name));
            profileStatistics_->setText(tr("Alignment %1 • %2 vertical PVIs")
                .arg(profile->alignmentId).arg(profile->pvis.size()));
        }
    } else {
        selectionSummary_->setText(tr("No civil object selected"));
    }
}

void CivilMainWindow::applyProperties() {
    bool changed = false;
    pushUndo();
    if (selectionKind_ == SelectionKind::Point) {
        changed = document_.updatePoint(selectionId_, pointNumber_->text().toStdString(),
            pointEasting_->value(), pointNorthing_->value(), pointElevation_->value(),
            pointDescription_->text().toStdString());
    } else if (selectionKind_ == SelectionKind::Surface) {
        changed = document_.renameSurface(selectionId_, surfaceName_->text().toStdString());
    } else if (selectionKind_ == SelectionKind::Alignment) {
        changed = document_.updateAlignment(selectionId_, alignmentName_->text().toStdString(),
                                             alignmentStartStation_->value());
    } else if (selectionKind_ == SelectionKind::Profile) {
        changed = document_.renameProfile(selectionId_, profileName_->text().toStdString());
    }
    if (!changed) {
        if (!undoStack_.empty()) undoStack_.pop_back();
        return;
    }
    markChanged(); rebuildTree(); showSelection(); viewport_->update();
}

void CivilMainWindow::analyzeSurface() {
    const auto* surface = document_.findSurface(selectionId_);
    if (!surface) return;
    double cut = 0.0, fill = 0.0;
    surface->calculateCutFillVolume(surfaceDatum_->value(), cut, fill);
    earthworkResult_->setText(tr("Cut %1 m³  •  Fill %2 m³  •  Net %3 m³")
        .arg(cut, 0, 'f', 2).arg(fill, 0, 'f', 2).arg(cut - fill, 0, 'f', 2));
}

void CivilMainWindow::inquireStationOffset() {
    const auto* alignment = document_.findAlignment(selectionId_);
    if (!alignment) return;
    const auto result = alignment->getStationOffset(inquiryEasting_->value(), inquiryNorthing_->value());
    inquiryResult_->setText(result ? tr("%1   Offset %2 m")
        .arg(QString::fromStdString(alignment->formatStation(result->station)))
        .arg(result->offset, 0, 'f', 3) : tr("No tangent station/offset solution."));
}

void CivilMainWindow::pushUndo() {
    undoStack_.push_back(document_.serializeNative());
    if (undoStack_.size() > 100) undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
}

void CivilMainWindow::restoreSnapshot(const std::string& snapshot) {
    std::string error;
    auto restored = mydesk::civil::CivilDocument::deserializeNative(snapshot, &error);
    if (!error.empty()) { showError(QString::fromStdString(error)); return; }
    document_ = std::move(restored);
    selectionKind_ = SelectionKind::None; selectionId_ = 0;
    markChanged(); viewport_->setDocument(&document_); rebuildTree(); showSelection();
}

void CivilMainWindow::undo() {
    if (undoStack_.empty()) return;
    redoStack_.push_back(document_.serializeNative());
    const auto snapshot = std::move(undoStack_.back()); undoStack_.pop_back();
    restoreSnapshot(snapshot); updateActions();
}

void CivilMainWindow::redo() {
    if (redoStack_.empty()) return;
    undoStack_.push_back(document_.serializeNative());
    const auto snapshot = std::move(redoStack_.back()); redoStack_.pop_back();
    restoreSnapshot(snapshot); updateActions();
}

bool CivilMainWindow::openDocument(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { showError(file.errorString()); return false; }
    const QByteArray bytes = file.readAll();
    std::string error;
    auto loaded = mydesk::civil::CivilDocument::deserializeNative(
        std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())), &error);
    if (!error.empty()) { showError(QString::fromStdString(error)); return false; }
    document_ = std::move(loaded); currentPath_ = QFileInfo(path).absoluteFilePath();
    dirty_ = false; undoStack_.clear(); redoStack_.clear(); selectionKind_ = SelectionKind::None;
    selectionId_ = 0; viewport_->setDocument(&document_); rebuildTree(); showSelection(); updateActions();
    return true;
}

bool CivilMainWindow::saveDocument(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) { showError(file.errorString()); return false; }
    const std::string data = document_.serializeNative();
    if (file.write(data.data(), static_cast<qint64>(data.size())) != static_cast<qint64>(data.size())) {
        showError(file.errorString()); return false;
    }
    currentPath_ = QFileInfo(path).absoluteFilePath(); dirty_ = false; updateActions(); return true;
}

bool CivilMainWindow::exportLandXml(const QString& path) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) { showError(file.errorString()); return false; }
    const std::string data = document_.exportLandXml();
    return file.write(data.data(), static_cast<qint64>(data.size())) == static_cast<qint64>(data.size());
}

void CivilMainWindow::markChanged() { dirty_ = true; updateActions(); }
void CivilMainWindow::updateActions() {
    undoAction_->setEnabled(!undoStack_.empty()); redoAction_->setEnabled(!redoStack_.empty());
    saveAction_->setEnabled(dirty_ || currentPath_.isEmpty());
    setWindowTitle(tr("%1%2 — MyDesk Civil").arg(QString::fromStdString(document_.name()), dirty_ ? "*" : ""));
}

bool CivilMainWindow::confirmDiscard() {
    if (!dirty_) return true;
    const auto answer = QMessageBox::question(this, tr("Unsaved civil project"),
        tr("Save changes before continuing?"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel) return false;
    return answer != QMessageBox::Save || saveCurrent();
}

bool CivilMainWindow::saveCurrent() { return currentPath_.isEmpty() ? saveAs() : saveDocument(currentPath_); }
bool CivilMainWindow::saveAs() {
    QString path = QFileDialog::getSaveFileName(this, tr("Save Civil Project"), currentPath_,
                                                tr("MyDesk Civil (*.mydesk-civil)"));
    if (path.isEmpty()) return false;
    if (!path.endsWith(".mydesk-civil", Qt::CaseInsensitive)) path += ".mydesk-civil";
    return saveDocument(path);
}
void CivilMainWindow::openInteractive() {
    if (!confirmDiscard()) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Civil Project"), {},
                                                       tr("MyDesk Civil (*.mydesk-civil);;All Files (*)"));
    if (!path.isEmpty()) openDocument(path);
}
void CivilMainWindow::exportInteractive() {
    QString path = QFileDialog::getSaveFileName(this, tr("Export LandXML"), {}, tr("LandXML (*.xml)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(".xml", Qt::CaseInsensitive)) path += ".xml";
    if (exportLandXml(path)) statusBar()->showMessage(tr("LandXML exported to %1").arg(path), 5000);
}
void CivilMainWindow::showError(const QString& message) const {
    QMessageBox::critical(const_cast<CivilMainWindow*>(this), tr("MyDesk Civil"), message);
}

void CivilMainWindow::closeEvent(QCloseEvent* event) {
    if (!confirmDiscard()) { event->ignore(); return; }
    QSettings settings; settings.setValue("civil/windowGeometry", saveGeometry());
    settings.setValue("civil/windowState", saveState()); event->accept();
}
