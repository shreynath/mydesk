// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include "studio_viewport.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>

namespace mydesk::studio::ui {

namespace {

constexpr int kObjectRole = Qt::UserRole;
constexpr int kModifierRole = Qt::UserRole + 1;

QDoubleSpinBox* numericBox(const char* objectName, double minimum, double maximum,
                           double value, int decimals = 3) {
    auto* box = new QDoubleSpinBox;
    box->setObjectName(objectName);
    box->setRange(minimum, maximum);
    box->setDecimals(decimals);
    box->setValue(value);
    box->setSingleStep(decimals == 0 ? 1.0 : 0.1);
    box->setKeyboardTracking(false);
    return box;
}

QWidget* vectorEditor(const QString& title, const std::array<QDoubleSpinBox*, 3>& values) {
    auto* group = new QGroupBox(title);
    auto* layout = new QFormLayout(group);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addRow(QStringLiteral("X"), values[0]);
    layout->addRow(QStringLiteral("Y"), values[1]);
    layout->addRow(QStringLiteral("Z"), values[2]);
    return group;
}

QString nativeFilter() {
    return QStringLiteral("MyDesk Studio scenes (*.mystudio);;JSON documents (*.json);;All files (*)");
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setObjectName("studioMainWindow");
    resize(1440, 900);
    setMinimumSize(960, 640);
    buildInterface();
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("studio/geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("studio/windowState")).toByteArray());
    setDarkTheme(settings.value(QStringLiteral("studio/darkTheme"), true).toBool());
    refreshAll(true);
}

void MainWindow::buildInterface() {
    viewport_ = new StudioViewport(this);
    viewport_->setDocument(&document_);
    setCentralWidget(viewport_);
    connect(viewport_, &StudioViewport::objectSelected, this,
            [this](ObjectId id) { selectObject(id); });
    connect(viewport_, &StudioViewport::cameraChanged, this,
            [this](const QString& text) { statusBar()->showMessage(text, 2500); });

    buildActions();
    buildSceneExplorer();
    buildCommandPanel();
    statusBar()->setSizeGripEnabled(true);
}

void MainWindow::buildActions() {
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    auto* newAction = fileMenu->addAction(QStringLiteral("&New Scene"), this,
                                           &MainWindow::newScene, QKeySequence::New);
    newAction->setObjectName("newSceneAction");
    auto* openAction = fileMenu->addAction(QStringLiteral("&Open…"), this,
                                            &MainWindow::openScene, QKeySequence::Open);
    openAction->setObjectName("openSceneAction");
    saveAction_ = fileMenu->addAction(QStringLiteral("&Save"), this,
                                      &MainWindow::saveScene, QKeySequence::Save);
    saveAction_->setObjectName("saveSceneAction");
    auto* saveAsAction = fileMenu->addAction(QStringLiteral("Save &As…"), this,
                                              &MainWindow::saveSceneAs,
                                              QKeySequence::SaveAs);
    saveAsAction->setObjectName("saveSceneAsAction");
    fileMenu->addSeparator();
    auto* exportAction = fileMenu->addAction(QStringLiteral("Export Selected as &OBJ…"), this,
                                              &MainWindow::exportSelectedObject,
                                              QKeySequence(QStringLiteral("Ctrl+E")));
    exportAction->setObjectName("exportObjAction");
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("E&xit"), this, &QWidget::close, QKeySequence::Quit);

    auto* editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
    undoAction_ = editMenu->addAction(QStringLiteral("&Undo"), this, &MainWindow::undo,
                                      QKeySequence::Undo);
    undoAction_->setObjectName("undoAction");
    redoAction_ = editMenu->addAction(QStringLiteral("&Redo"), this, &MainWindow::redo,
                                      QKeySequence::Redo);
    redoAction_->setObjectName("redoAction");
    editMenu->addSeparator();
    deleteAction_ = editMenu->addAction(QStringLiteral("&Delete Selection"), this,
                                        &MainWindow::deleteSelection, QKeySequence::Delete);
    deleteAction_->setObjectName("deleteSelectionAction");

    auto* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    auto* frameAction = viewMenu->addAction(QStringLiteral("Frame &All"), viewport_,
                                             &StudioViewport::frameAll,
                                             QKeySequence(QStringLiteral("Home")));
    frameAction->setObjectName("frameAllAction");
    viewMenu->addSeparator();
    auto* darkAction = viewMenu->addAction(QStringLiteral("Dark Workspace"));
    darkAction->setCheckable(true);
    darkAction->setChecked(true);
    connect(darkAction, &QAction::toggled, this, &MainWindow::setDarkTheme);

    auto* toolbar = addToolBar(QStringLiteral("Modeling"));
    toolbar->setObjectName("modelingToolbar");
    toolbar->setMovable(false);
    toolbar->addAction(newAction);
    toolbar->addAction(openAction);
    toolbar->addAction(saveAction_);
    toolbar->addSeparator();
    toolbar->addAction(undoAction_);
    toolbar->addAction(redoAction_);
    toolbar->addSeparator();
    toolbar->addAction(frameAction);
}

void MainWindow::buildSceneExplorer() {
    auto* dock = new QDockWidget(QStringLiteral("Scene Explorer"), this);
    dock->setObjectName("sceneExplorerDock");
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    sceneTree_ = new QTreeWidget(dock);
    sceneTree_->setObjectName("sceneTree");
    sceneTree_->setHeaderLabels({QStringLiteral("Object / Modifier"), QStringLiteral("Type")});
    sceneTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    sceneTree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    sceneTree_->setAlternatingRowColors(true);
    sceneTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(sceneTree_, &QTreeWidget::itemSelectionChanged, this,
            &MainWindow::sceneSelectionChanged);
    dock->setWidget(sceneTree_);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
}

void MainWindow::buildCommandPanel() {
    auto* dock = new QDockWidget(QStringLiteral("Command Panel"), this);
    dock->setObjectName("commandPanelDock");
    dock->setMinimumWidth(330);
    auto* tabs = new QTabWidget(dock);
    tabs->setObjectName("commandTabs");

    auto* createPage = new QWidget;
    auto* createLayout = new QVBoxLayout(createPage);
    auto* createForm = new QFormLayout;
    createKind_ = new QComboBox;
    createKind_->setObjectName("createPrimitiveKind");
    createKind_->addItems({QStringLiteral("Box"), QStringLiteral("Cylinder"),
                           QStringLiteral("Sphere")});
    createName_ = new QLineEdit(QStringLiteral("Box001"));
    createName_->setObjectName("createObjectName");
    createForm->addRow(QStringLiteral("Primitive"), createKind_);
    createForm->addRow(QStringLiteral("Name"), createName_);
    createLayout->addLayout(createForm);

    createParameters_ = new QStackedWidget;
    createParameters_->setObjectName("createParameterStack");
    auto* boxPage = new QWidget;
    auto* boxForm = new QFormLayout(boxPage);
    boxSize_ = {numericBox("createBoxLength", 0.001, 1.0e6, 10.0),
                numericBox("createBoxWidth", 0.001, 1.0e6, 10.0),
                numericBox("createBoxHeight", 0.001, 1.0e6, 10.0)};
    boxForm->addRow(QStringLiteral("Length"), boxSize_[0]);
    boxForm->addRow(QStringLiteral("Width"), boxSize_[1]);
    boxForm->addRow(QStringLiteral("Height"), boxSize_[2]);
    createParameters_->addWidget(boxPage);
    auto* cylinderPage = new QWidget;
    auto* cylinderForm = new QFormLayout(cylinderPage);
    cylinderRadius_ = numericBox("createCylinderRadius", 0.001, 1.0e6, 5.0);
    cylinderHeight_ = numericBox("createCylinderHeight", 0.001, 1.0e6, 10.0);
    cylinderSides_ = new QSpinBox;
    cylinderSides_->setObjectName("createCylinderSides");
    cylinderSides_->setRange(3, 512);
    cylinderSides_->setValue(24);
    cylinderForm->addRow(QStringLiteral("Radius"), cylinderRadius_);
    cylinderForm->addRow(QStringLiteral("Height"), cylinderHeight_);
    cylinderForm->addRow(QStringLiteral("Sides"), cylinderSides_);
    createParameters_->addWidget(cylinderPage);
    auto* spherePage = new QWidget;
    auto* sphereForm = new QFormLayout(spherePage);
    sphereRadius_ = numericBox("createSphereRadius", 0.001, 1.0e6, 5.0);
    sphereSegments_ = new QSpinBox;
    sphereSegments_->setObjectName("createSphereSegments");
    sphereSegments_->setRange(4, 256);
    sphereSegments_->setValue(16);
    sphereForm->addRow(QStringLiteral("Radius"), sphereRadius_);
    sphereForm->addRow(QStringLiteral("Segments"), sphereSegments_);
    createParameters_->addWidget(spherePage);
    createLayout->addWidget(createParameters_);
    auto* createButton = new QPushButton(QStringLiteral("Create Primitive"));
    createButton->setObjectName("createPrimitiveButton");
    createButton->setDefault(true);
    createLayout->addWidget(createButton);
    createLayout->addStretch();
    connect(createKind_, qOverload<int>(&QComboBox::currentIndexChanged),
            createParameters_, &QStackedWidget::setCurrentIndex);
    connect(createKind_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                static const char* names[] = {"Box", "Cylinder", "Sphere"};
                createName_->setText(QStringLiteral("%1%2")
                    .arg(QLatin1String(names[index]))
                    .arg(document_.objects().size() + 1, 3, 10, QLatin1Char('0')));
            });
    connect(createButton, &QPushButton::clicked, this, &MainWindow::createPrimitive);
    tabs->addTab(createPage, QStringLiteral("Create"));

    auto* modifyPage = new QWidget;
    auto* modifyLayout = new QVBoxLayout(modifyPage);
    selectionLabel_ = new QLabel(QStringLiteral("No object selected"));
    selectionLabel_->setObjectName("selectionLabel");
    selectionLabel_->setWordWrap(true);
    modifyLayout->addWidget(selectionLabel_);

    translate_ = {numericBox("translateX", -1.0e6, 1.0e6, 0.0),
                  numericBox("translateY", -1.0e6, 1.0e6, 0.0),
                  numericBox("translateZ", -1.0e6, 1.0e6, 0.0)};
    rotate_ = {numericBox("rotateX", -36000.0, 36000.0, 0.0),
               numericBox("rotateY", -36000.0, 36000.0, 0.0),
               numericBox("rotateZ", -36000.0, 36000.0, 0.0)};
    scale_ = {numericBox("scaleX", -1000.0, 1000.0, 1.0),
              numericBox("scaleY", -1000.0, 1000.0, 1.0),
              numericBox("scaleZ", -1000.0, 1000.0, 1.0)};
    modifyLayout->addWidget(vectorEditor(QStringLiteral("Position"), translate_));
    modifyLayout->addWidget(vectorEditor(QStringLiteral("Rotation (degrees)"), rotate_));
    modifyLayout->addWidget(vectorEditor(QStringLiteral("Scale"), scale_));
    auto* transformButton = new QPushButton(QStringLiteral("Apply Transform"));
    transformButton->setObjectName("applyTransformButton");
    connect(transformButton, &QPushButton::clicked, this, &MainWindow::applyTransform);
    modifyLayout->addWidget(transformButton);

    auto* primitiveGroup = new QGroupBox(QStringLiteral("Primitive Parameters"));
    auto* primitiveForm = new QFormLayout(primitiveGroup);
    primitiveType_ = new QLabel;
    primitiveForm->addRow(QStringLiteral("Type"), primitiveType_);
    for (std::size_t index = 0; index < primitiveValues_.size(); ++index) {
        primitiveLabels_[index] = new QLabel;
        primitiveValues_[index] = numericBox(("primitiveValue" + std::to_string(index)).c_str(),
                                              0.001, 1.0e6, 1.0);
        primitiveForm->addRow(primitiveLabels_[index], primitiveValues_[index]);
    }
    applyPrimitiveButton_ = new QPushButton(QStringLiteral("Apply Primitive Parameters"));
    applyPrimitiveButton_->setObjectName("applyPrimitiveButton");
    primitiveForm->addRow(applyPrimitiveButton_);
    connect(applyPrimitiveButton_, &QPushButton::clicked, this,
            &MainWindow::applyPrimitiveParameters);
    modifyLayout->addWidget(primitiveGroup);
    modifyLayout->addStretch();
    tabs->addTab(modifyPage, QStringLiteral("Modify"));

    auto* stackPage = new QWidget;
    auto* stackLayout = new QVBoxLayout(stackPage);
    modifierKind_ = new QComboBox;
    modifierKind_->setObjectName("modifierKind");
    modifierKind_->addItems({QStringLiteral("Bend"), QStringLiteral("Taper"),
                             QStringLiteral("Twist"), QStringLiteral("TurboSmooth")});
    modifierPrimaryLabel_ = new QLabel;
    modifierSecondaryLabel_ = new QLabel;
    modifierPrimary_ = numericBox("modifierPrimary", -10000.0, 10000.0, 0.0);
    modifierSecondary_ = numericBox("modifierSecondary", -10000.0, 10000.0, 0.0);
    auto* modifierForm = new QFormLayout;
    modifierForm->addRow(QStringLiteral("Modifier"), modifierKind_);
    modifierForm->addRow(modifierPrimaryLabel_, modifierPrimary_);
    modifierForm->addRow(modifierSecondaryLabel_, modifierSecondary_);
    stackLayout->addLayout(modifierForm);
    auto* addModifierButton = new QPushButton(QStringLiteral("Add to Stack"));
    addModifierButton->setObjectName("addModifierButton");
    stackLayout->addWidget(addModifierButton);
    modifierStack_ = new QListWidget;
    modifierStack_->setObjectName("modifierStack");
    modifierStack_->setAlternatingRowColors(true);
    stackLayout->addWidget(modifierStack_, 1);
    auto* row = new QHBoxLayout;
    auto* upButton = new QPushButton(QStringLiteral("Up"));
    auto* downButton = new QPushButton(QStringLiteral("Down"));
    auto* removeButton = new QPushButton(QStringLiteral("Remove"));
    upButton->setObjectName("modifierUpButton");
    downButton->setObjectName("modifierDownButton");
    removeButton->setObjectName("removeModifierButton");
    row->addWidget(upButton);
    row->addWidget(downButton);
    row->addWidget(removeButton);
    stackLayout->addLayout(row);
    applyModifierButton_ = new QPushButton(QStringLiteral("Apply Modifier Parameters"));
    applyModifierButton_->setObjectName("applyModifierButton");
    stackLayout->addWidget(applyModifierButton_);
    connect(modifierKind_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                static const char* primary[] = {"Angle", "Amount", "Angle", "Iterations"};
                static const char* secondary[] = {"Direction", "Curve", "", ""};
                modifierPrimaryLabel_->setText(QLatin1String(primary[index]));
                modifierSecondaryLabel_->setText(QLatin1String(secondary[index]));
                modifierSecondary_->setVisible(index < 2);
                modifierSecondaryLabel_->setVisible(index < 2);
                modifierPrimary_->setDecimals(index == 3 ? 0 : 3);
                modifierPrimary_->setRange(index == 3 ? 1.0 : -10000.0,
                                           index == 3 ? 4.0 : 10000.0);
                modifierPrimary_->setValue(index == 3 ? 1.0 : 0.0);
            });
    modifierKind_->setCurrentIndex(1);
    modifierKind_->setCurrentIndex(0);
    connect(addModifierButton, &QPushButton::clicked, this, &MainWindow::addModifier);
    connect(upButton, &QPushButton::clicked, this, &MainWindow::moveModifierUp);
    connect(downButton, &QPushButton::clicked, this, &MainWindow::moveModifierDown);
    connect(removeButton, &QPushButton::clicked, this, &MainWindow::removeSelectedModifier);
    connect(applyModifierButton_, &QPushButton::clicked, this,
            &MainWindow::applyModifierParameter);
    connect(modifierStack_, &QListWidget::itemSelectionChanged, this,
            &MainWindow::modifierSelectionChanged);
    connect(modifierStack_, &QListWidget::itemChanged, this,
            &MainWindow::modifierItemChanged);
    tabs->addTab(stackPage, QStringLiteral("Stack"));

    dock->setWidget(tabs);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}

QString MainWindow::primitiveName(PrimitiveKind kind) const {
    switch (kind) {
    case PrimitiveKind::Box: return QStringLiteral("Box");
    case PrimitiveKind::Cylinder: return QStringLiteral("Cylinder");
    case PrimitiveKind::Sphere: return QStringLiteral("Sphere");
    }
    return QStringLiteral("Unknown");
}

QString MainWindow::modifierName(ModifierKind kind) const {
    switch (kind) {
    case ModifierKind::Bend: return QStringLiteral("Bend");
    case ModifierKind::Taper: return QStringLiteral("Taper");
    case ModifierKind::Twist: return QStringLiteral("Twist");
    case ModifierKind::TurboSmooth: return QStringLiteral("TurboSmooth");
    }
    return QStringLiteral("Unknown");
}

void MainWindow::createPrimitive() {
    try {
        const std::string name = createName_->text().trimmed().toStdString();
        ObjectId id = 0;
        if (createKind_->currentIndex() == 0) {
            id = document_.addBox(name, boxSize_[0]->value(), boxSize_[1]->value(),
                                  boxSize_[2]->value());
        } else if (createKind_->currentIndex() == 1) {
            id = document_.addCylinder(name, cylinderRadius_->value(), cylinderHeight_->value(),
                                       cylinderSides_->value());
        } else {
            id = document_.addSphere(name, sphereRadius_->value(), sphereSegments_->value());
        }
        selectedObjectId_ = id;
        selectedModifierId_ = 0;
        markChanged();
        refreshAll(true);
    } catch (const std::exception& error) {
        showOperationError(QStringLiteral("Create primitive"), QString::fromUtf8(error.what()));
    }
}

StudioObject* MainWindow::selectedObject() { return document_.findObject(selectedObjectId_); }
const StudioObject* MainWindow::selectedObject() const { return document_.findObject(selectedObjectId_); }

Modifier* MainWindow::selectedModifier() {
    StudioObject* object = selectedObject();
    if (!object) return nullptr;
    const auto it = std::find_if(object->modifierStack.begin(), object->modifierStack.end(),
                                 [this](const Modifier& modifier) {
                                     return modifier.id == selectedModifierId_;
                                 });
    return it == object->modifierStack.end() ? nullptr : &*it;
}

void MainWindow::applyTransform() {
    if (!selectedObject()) return;
    ObjectTransform transform;
    transform.translation = {translate_[0]->value(), translate_[1]->value(), translate_[2]->value()};
    transform.rotation_deg = {rotate_[0]->value(), rotate_[1]->value(), rotate_[2]->value()};
    transform.scale = {scale_[0]->value(), scale_[1]->value(), scale_[2]->value()};
    if (!document_.setObjectTransform(selectedObjectId_, transform)) {
        showOperationError(QStringLiteral("Apply transform"),
                           QStringLiteral("Scale components must be finite and non-zero."));
        return;
    }
    markChanged();
    refreshAll();
}

void MainWindow::applyPrimitiveParameters() {
    const StudioObject* object = selectedObject();
    if (!object) return;
    QStringList keys;
    if (object->primitive == PrimitiveKind::Box) keys = {"length", "width", "height"};
    else if (object->primitive == PrimitiveKind::Cylinder) keys = {"radius", "height", "sides"};
    else keys = {"radius", "segments"};
    for (int index = 0; index < keys.size(); ++index) {
        if (!document_.setPrimitiveParameter(selectedObjectId_, keys[index].toStdString(),
                                             primitiveValues_[static_cast<std::size_t>(index)]->value())) {
            showOperationError(QStringLiteral("Edit primitive"),
                               QStringLiteral("The parameter value is invalid."));
            refreshInspector();
            return;
        }
    }
    markChanged();
    refreshAll();
}

void MainWindow::addModifier() {
    if (!selectedObject()) return;
    ModifierId id = 0;
    switch (static_cast<ModifierKind>(modifierKind_->currentIndex())) {
    case ModifierKind::Bend:
        id = document_.addBendModifier(selectedObjectId_, modifierPrimary_->value(),
                                       modifierSecondary_->value());
        break;
    case ModifierKind::Taper:
        id = document_.addTaperModifier(selectedObjectId_, modifierPrimary_->value(),
                                        modifierSecondary_->value());
        break;
    case ModifierKind::Twist:
        id = document_.addTwistModifier(selectedObjectId_, modifierPrimary_->value());
        break;
    case ModifierKind::TurboSmooth:
        id = document_.addTurboSmoothModifier(selectedObjectId_,
                                              static_cast<int>(modifierPrimary_->value()));
        break;
    }
    if (id == 0) {
        showOperationError(QStringLiteral("Add modifier"), QStringLiteral("The parameters are invalid."));
        return;
    }
    selectedModifierId_ = id;
    markChanged();
    refreshAll();
}

void MainWindow::applyModifierParameter() {
    Modifier* modifier = selectedModifier();
    if (!modifier) return;
    bool ok = false;
    switch (modifier->kind) {
    case ModifierKind::Bend:
        ok = document_.setModifierParameter(selectedObjectId_, modifier->id, "angle",
                                            modifierPrimary_->value()) &&
             document_.setModifierParameter(selectedObjectId_, modifier->id, "direction",
                                            modifierSecondary_->value());
        break;
    case ModifierKind::Taper:
        ok = document_.setModifierParameter(selectedObjectId_, modifier->id, "amount",
                                            modifierPrimary_->value()) &&
             document_.setModifierParameter(selectedObjectId_, modifier->id, "curve",
                                            modifierSecondary_->value());
        break;
    case ModifierKind::Twist:
        ok = document_.setModifierParameter(selectedObjectId_, modifier->id, "angle",
                                            modifierPrimary_->value());
        break;
    case ModifierKind::TurboSmooth:
        ok = document_.setModifierParameter(selectedObjectId_, modifier->id, "iterations",
                                            modifierPrimary_->value());
        break;
    }
    if (!ok) {
        showOperationError(QStringLiteral("Edit modifier"), QStringLiteral("The parameters are invalid."));
        return;
    }
    markChanged();
    refreshAll();
}

void MainWindow::removeSelectedModifier() {
    if (selectedModifierId_ == 0 || !document_.removeModifier(selectedObjectId_, selectedModifierId_)) return;
    selectedModifierId_ = 0;
    markChanged();
    refreshAll();
}

void MainWindow::moveModifierUp() {
    StudioObject* object = selectedObject();
    if (!object || selectedModifierId_ == 0) return;
    const auto it = std::find_if(object->modifierStack.begin(), object->modifierStack.end(),
                                 [this](const Modifier& modifier) { return modifier.id == selectedModifierId_; });
    if (it == object->modifierStack.end() || it == object->modifierStack.begin()) return;
    const auto index = static_cast<std::size_t>(std::distance(object->modifierStack.begin(), it));
    if (document_.moveModifier(selectedObjectId_, selectedModifierId_, index - 1)) {
        markChanged(); refreshAll();
    }
}

void MainWindow::moveModifierDown() {
    StudioObject* object = selectedObject();
    if (!object || selectedModifierId_ == 0) return;
    const auto it = std::find_if(object->modifierStack.begin(), object->modifierStack.end(),
                                 [this](const Modifier& modifier) { return modifier.id == selectedModifierId_; });
    if (it == object->modifierStack.end()) return;
    const auto index = static_cast<std::size_t>(std::distance(object->modifierStack.begin(), it));
    if (index + 1 >= object->modifierStack.size()) return;
    if (document_.moveModifier(selectedObjectId_, selectedModifierId_, index + 1)) {
        markChanged(); refreshAll();
    }
}

void MainWindow::sceneSelectionChanged() {
    if (refreshing_) return;
    const auto selected = sceneTree_->selectedItems();
    if (selected.empty()) return;
    selectObject(selected.front()->data(0, kObjectRole).toULongLong(),
                 selected.front()->data(0, kModifierRole).toULongLong());
}

void MainWindow::modifierSelectionChanged() {
    if (refreshing_ || !modifierStack_->currentItem()) return;
    selectedModifierId_ = modifierStack_->currentItem()->data(kModifierRole).toULongLong();
    refreshInspector();
}

void MainWindow::modifierItemChanged() {
    if (refreshing_ || !modifierStack_->currentItem()) return;
    const ModifierId id = modifierStack_->currentItem()->data(kModifierRole).toULongLong();
    const bool enabled = modifierStack_->currentItem()->checkState() == Qt::Checked;
    if (document_.setModifierEnabled(selectedObjectId_, id, enabled)) {
        selectedModifierId_ = id;
        markChanged(); refreshAll();
    }
}

void MainWindow::deleteSelection() {
    if (selectedModifierId_ != 0) {
        removeSelectedModifier();
        return;
    }
    if (selectedObjectId_ != 0 && document_.removeObject(selectedObjectId_)) {
        selectedObjectId_ = 0;
        markChanged();
        refreshAll(true);
    }
}

void MainWindow::undo() {
    if (!document_.undo()) return;
    if (!document_.findObject(selectedObjectId_)) selectedObjectId_ = 0;
    selectedModifierId_ = 0;
    markChanged();
    refreshAll();
}

void MainWindow::redo() {
    if (!document_.redo()) return;
    if (!document_.findObject(selectedObjectId_)) selectedObjectId_ = 0;
    selectedModifierId_ = 0;
    markChanged();
    refreshAll();
}

void MainWindow::newScene() {
    if (!maybeSave()) return;
    document_ = StudioDocument{};
    currentPath_.clear();
    selectedObjectId_ = 0;
    selectedModifierId_ = 0;
    viewport_->setDocument(&document_);
    setClean();
    refreshAll(true);
}

void MainWindow::openScene() {
    if (!maybeSave()) return;
    const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("Open Studio Scene"),
                                                       QString(), nativeFilter());
    if (!file.isEmpty()) openPath(file.toStdString());
}

bool MainWindow::openPath(const std::filesystem::path& path) {
    std::string error;
    StudioDocument loaded = StudioDocument::loadNativeFile(path, &error);
    if (!error.empty()) {
        showOperationError(QStringLiteral("Open scene"), QString::fromStdString(error));
        return false;
    }
    document_ = std::move(loaded);
    currentPath_ = path;
    selectedObjectId_ = document_.objects().empty() ? 0 : document_.objects().front().id;
    selectedModifierId_ = 0;
    viewport_->setDocument(&document_);
    setClean();
    refreshAll(true);
    return true;
}

bool MainWindow::saveScene() {
    return currentPath_.empty() ? saveSceneAs() : savePath(currentPath_);
}

bool MainWindow::saveSceneAs() {
    QString file = QFileDialog::getSaveFileName(this, QStringLiteral("Save Studio Scene"),
                                                 QString(), nativeFilter());
    if (file.isEmpty()) return false;
    if (QFileInfo(file).suffix().isEmpty()) file += QStringLiteral(".mystudio");
    return savePath(file.toStdString());
}

bool MainWindow::savePath(const std::filesystem::path& path) {
    std::string error;
    if (!document_.saveNativeFile(path, &error)) {
        showOperationError(QStringLiteral("Save scene"), QString::fromStdString(error));
        return false;
    }
    currentPath_ = path;
    setClean();
    statusBar()->showMessage(QStringLiteral("Saved %1").arg(QString::fromStdString(path.string())), 3500);
    return true;
}

void MainWindow::exportSelectedObject() {
    const StudioObject* object = selectedObject();
    if (!object) return;
    QString file = QFileDialog::getSaveFileName(this, QStringLiteral("Export Evaluated Mesh"),
                                                 QString::fromStdString(object->name) + ".obj",
                                                 QStringLiteral("Wavefront OBJ (*.obj)"));
    if (file.isEmpty()) return;
    if (QFileInfo(file).suffix().isEmpty()) file += QStringLiteral(".obj");
    std::ofstream output(file.toStdString(), std::ios::binary | std::ios::trunc);
    if (!output) {
        showOperationError(QStringLiteral("Export OBJ"), QStringLiteral("Cannot open the output file."));
        return;
    }
    output << document_.exportObj(selectedObjectId_);
    if (!output) {
        showOperationError(QStringLiteral("Export OBJ"), QStringLiteral("Writing the output file failed."));
        return;
    }
    statusBar()->showMessage(QStringLiteral("Exported %1").arg(file), 3500);
}

void MainWindow::refreshAll(bool frame) {
    refreshSceneExplorer();
    refreshModifierStack();
    refreshInspector();
    viewport_->setSelectedObject(selectedObjectId_);
    if (frame) viewport_->frameAll(); else viewport_->update();
    undoAction_->setEnabled(document_.canUndo());
    redoAction_->setEnabled(document_.canRedo());
    deleteAction_->setEnabled(selectedObjectId_ != 0);
    updateWindowTitle();
    updateStatus();
}

void MainWindow::refreshSceneExplorer() {
    refreshing_ = true;
    sceneTree_->clear();
    for (const StudioObject& object : document_.objects()) {
        auto* root = new QTreeWidgetItem(sceneTree_,
            {QString::fromStdString(object.name), primitiveName(object.primitive)});
        root->setData(0, kObjectRole, QVariant::fromValue<qulonglong>(object.id));
        root->setData(0, kModifierRole, QVariant::fromValue<qulonglong>(0));
        for (const Modifier& modifier : object.modifierStack) {
            auto* child = new QTreeWidgetItem(root,
                {QString::fromStdString(modifier.name), modifierName(modifier.kind)});
            child->setData(0, kObjectRole, QVariant::fromValue<qulonglong>(object.id));
            child->setData(0, kModifierRole, QVariant::fromValue<qulonglong>(modifier.id));
        }
        root->setExpanded(true);
        if (object.id == selectedObjectId_ && selectedModifierId_ == 0) root->setSelected(true);
        if (object.id == selectedObjectId_ && selectedModifierId_ != 0) {
            for (int index = 0; index < root->childCount(); ++index) {
                if (root->child(index)->data(0, kModifierRole).toULongLong() == selectedModifierId_) {
                    root->child(index)->setSelected(true);
                }
            }
        }
    }
    refreshing_ = false;
}

void MainWindow::refreshModifierStack() {
    refreshing_ = true;
    modifierStack_->clear();
    const StudioObject* object = selectedObject();
    if (object) {
        for (const Modifier& modifier : object->modifierStack) {
            auto* item = new QListWidgetItem(modifierName(modifier.kind), modifierStack_);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(modifier.enabled ? Qt::Checked : Qt::Unchecked);
            item->setData(kModifierRole, QVariant::fromValue<qulonglong>(modifier.id));
            if (modifier.id == selectedModifierId_) item->setSelected(true);
        }
    }
    refreshing_ = false;
}

void MainWindow::refreshInspector() {
    refreshing_ = true;
    const StudioObject* object = selectedObject();
    const bool enabled = object != nullptr;
    selectionLabel_->setText(enabled
        ? QStringLiteral("%1  ·  %2 vertices / %3 triangles")
              .arg(QString::fromStdString(object->name))
              .arg(object->evaluatedMesh.vertices.size())
              .arg(object->evaluatedMesh.triangles.size())
        : QStringLiteral("No object selected"));
    for (auto* value : translate_) value->setEnabled(enabled);
    for (auto* value : rotate_) value->setEnabled(enabled);
    for (auto* value : scale_) value->setEnabled(enabled);
    applyPrimitiveButton_->setEnabled(enabled);
    modifierKind_->setEnabled(enabled);
    if (!enabled) {
        primitiveType_->clear();
        for (auto* value : primitiveValues_) value->setEnabled(false);
        refreshing_ = false;
        return;
    }
    const Vec3 vectors[] = {object->transform.translation, object->transform.rotation_deg,
                            object->transform.scale};
    const std::array<QDoubleSpinBox*, 3>* controls[] = {&translate_, &rotate_, &scale_};
    for (int row = 0; row < 3; ++row) {
        (*controls[row])[0]->setValue(vectors[row].x);
        (*controls[row])[1]->setValue(vectors[row].y);
        (*controls[row])[2]->setValue(vectors[row].z);
    }
    primitiveType_->setText(primitiveName(object->primitive));
    QStringList keys;
    QStringList labels;
    if (object->primitive == PrimitiveKind::Box) {
        keys = {"length", "width", "height"}; labels = {"Length", "Width", "Height"};
    } else if (object->primitive == PrimitiveKind::Cylinder) {
        keys = {"radius", "height", "sides"}; labels = {"Radius", "Height", "Sides"};
    } else {
        keys = {"radius", "segments"}; labels = {"Radius", "Segments"};
    }
    for (std::size_t index = 0; index < primitiveValues_.size(); ++index) {
        const bool visible = static_cast<int>(index) < keys.size();
        primitiveLabels_[index]->setVisible(visible);
        primitiveValues_[index]->setVisible(visible);
        primitiveValues_[index]->setEnabled(visible);
        if (visible) {
            primitiveLabels_[index]->setText(labels[static_cast<int>(index)]);
            primitiveValues_[index]->setValue(object->primitiveParams.at(
                keys[static_cast<int>(index)].toStdString()));
        }
    }
    if (Modifier* modifier = selectedModifier()) {
        modifierKind_->setCurrentIndex(static_cast<int>(modifier->kind));
        if (modifier->kind == ModifierKind::Bend) {
            modifierPrimary_->setValue(modifier->parameters.at("angle"));
            modifierSecondary_->setValue(modifier->parameters.at("direction"));
        } else if (modifier->kind == ModifierKind::Taper) {
            modifierPrimary_->setValue(modifier->parameters.at("amount"));
            modifierSecondary_->setValue(modifier->parameters.at("curve"));
        } else if (modifier->kind == ModifierKind::Twist) {
            modifierPrimary_->setValue(modifier->parameters.at("angle"));
        } else {
            modifierPrimary_->setValue(modifier->parameters.at("iterations"));
        }
        applyModifierButton_->setEnabled(true);
    } else {
        applyModifierButton_->setEnabled(false);
    }
    refreshing_ = false;
}

void MainWindow::selectObject(ObjectId object, ModifierId modifier) {
    selectedObjectId_ = document_.findObject(object) ? object : 0;
    selectedModifierId_ = modifier;
    refreshAll();
}

void MainWindow::markChanged() {
    dirty_ = true;
    updateWindowTitle();
}

void MainWindow::setClean() {
    dirty_ = false;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle() {
    const QString file = currentPath_.empty()
        ? QStringLiteral("Untitled")
        : QString::fromStdString(currentPath_.filename().string());
    setWindowTitle(QStringLiteral("%1%2 — MyDesk Studio").arg(file, dirty_ ? "*" : ""));
}

void MainWindow::updateStatus() {
    std::size_t triangles = 0;
    for (const StudioObject& object : document_.objects()) triangles += object.evaluatedMesh.triangles.size();
    statusBar()->showMessage(QStringLiteral("%1 objects  |  %2 triangles  |  Native schema 3")
                             .arg(document_.objects().size()).arg(triangles));
}

bool MainWindow::maybeSave() {
    if (!dirty_) return true;
    const auto answer = QMessageBox::warning(this, QStringLiteral("Unsaved Studio Scene"),
        QStringLiteral("Save changes before closing this scene?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false;
    return answer == QMessageBox::Discard || saveScene();
}

void MainWindow::setDarkTheme(bool dark) {
    QSettings().setValue(QStringLiteral("studio/darkTheme"), dark);
    if (!dark) {
        qApp->setStyleSheet({});
        return;
    }
    qApp->setStyleSheet(QStringLiteral(
        "QWidget{background:#111827;color:#E5E7EB;}"
        "QMainWindow,QMenuBar,QMenu,QStatusBar,QToolBar{background:#0F172A;}"
        "QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox,QListWidget,QTreeWidget,QTabWidget::pane{"
        "background:#111827;border:1px solid #475569;border-radius:3px;padding:3px;}"
        "QPushButton{background:#1D4ED8;border:1px solid #60A5FA;border-radius:3px;padding:6px 10px;}"
        "QPushButton:hover{background:#2563EB;} QPushButton:disabled{background:#334155;color:#94A3B8;}"
        "QTreeWidget::item:selected,QListWidget::item:selected{background:#1D4ED8;color:white;}"
        "QDockWidget::title{background:#1E293B;padding:6px;font-weight:600;}"
        "QTabBar::tab{background:#1E293B;padding:7px 13px;} QTabBar::tab:selected{background:#1D4ED8;}"
        "QGroupBox{border:1px solid #334155;margin-top:10px;padding-top:8px;}"
        "QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px;}"));
}

void MainWindow::showOperationError(const QString& operation, const QString& detail) {
    QMessageBox::critical(this, operation, detail);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!maybeSave()) {
        event->ignore();
        return;
    }
    QSettings settings;
    settings.setValue(QStringLiteral("studio/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("studio/windowState"), saveState());
    event->accept();
}

} // namespace mydesk::studio::ui
