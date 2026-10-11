#include "mydesk/buildings/main_window.hpp"

#include <QActionGroup>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>

namespace mydesk::buildings {
namespace {

QTreeWidgetItem* category(QTreeWidget* tree, const QString& name) {
    auto* item = new QTreeWidgetItem(tree, {name});
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    item->setExpanded(true);
    return item;
}

QTreeWidgetItem* elementItem(QTreeWidgetItem* parent, const QString& label, SelectionKind kind,
                             std::uint64_t id) {
    auto* item = new QTreeWidgetItem(parent, {label});
    item->setData(0, Qt::UserRole, static_cast<int>(kind));
    item->setData(0, Qt::UserRole + 1, QVariant::fromValue<qulonglong>(id));
    return item;
}

QDoubleSpinBox* measurement(double value, QWidget* parent) {
    auto* input = new QDoubleSpinBox(parent);
    input->setRange(-100000.0, 100000.0);
    input->setDecimals(3);
    input->setSuffix(" m");
    input->setValue(value);
    return input;
}

} // namespace

MainWindow::MainWindow() {
    buildInterface();
    refreshAll();
    resize(1280, 820);
}

void MainWindow::buildInterface() {
    viewport_ = new BimViewport(controller_, this);
    setCentralWidget(viewport_);
    viewport_->modelChanged = [this] { refreshAll(); };
    viewport_->selectionChanged = [this] { refreshProperties(); refreshBrowser(); };

    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&New", QKeySequence::New, this, [this] {
        if (!confirmDiscard()) return;
        controller_.newProject();
        refreshAll();
    });
    fileMenu->addAction("&Open…", QKeySequence::Open, this, [this] { openDocument(); });
    fileMenu->addAction("&Save", QKeySequence::Save, this, [this] { saveDocument(); });
    fileMenu->addAction("Save &As…", QKeySequence::SaveAs, this,
                        [this] { saveDocument(true); });
    fileMenu->addSeparator();
    fileMenu->addAction("Export &IFC4…", this, [this] { exportIfc(); });
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", QKeySequence::Quit, this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu("&Edit");
    undoAction_ = editMenu->addAction("&Undo", QKeySequence::Undo, this, [this] {
        if (controller_.undo()) refreshAll();
    });
    redoAction_ = editMenu->addAction("&Redo", QKeySequence::Redo, this, [this] {
        if (controller_.redo()) refreshAll();
    });

    auto* architecture = menuBar()->addMenu("&Architecture");
    architecture->addAction("Create &Level…", this, [this] { createLevel(); });
    architecture->addAction("Draw &Wall", this,
                            [this] { chooseTool(BimViewport::Tool::Wall); });
    architecture->addAction("Place &Door", this,
                            [this] { chooseTool(BimViewport::Tool::Door); });
    architecture->addAction("Place Wi&ndow", this,
                            [this] { chooseTool(BimViewport::Tool::Window); });

    auto* toolbar = addToolBar("BIM Authoring");
    toolbar->setObjectName("BimAuthoringToolbar");
    auto* toolGroup = new QActionGroup(toolbar);
    toolGroup->setExclusive(true);
    const auto addTool = [&](const QString& text, BimViewport::Tool tool, bool checked = false) {
        auto* action = toolbar->addAction(text);
        action->setCheckable(true);
        action->setChecked(checked);
        toolGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, tool] { chooseTool(tool); });
    };
    addTool("Select", BimViewport::Tool::Select, true);
    addTool("Wall", BimViewport::Tool::Wall);
    addTool("Door", BimViewport::Tool::Door);
    addTool("Window", BimViewport::Tool::Window);
    toolbar->addSeparator();
    auto* view3d = toolbar->addAction("3D");
    view3d->setCheckable(true);
    connect(view3d, &QAction::toggled, viewport_, &BimViewport::setAxonometric);
    toolbar->addAction("Level…", this, [this] { createLevel(); });

    auto* browserDock = new QDockWidget("Project Browser", this);
    browserDock->setObjectName("ProjectBrowserDock");
    browser_ = new QTreeWidget(browserDock);
    browser_->setObjectName("projectBrowser");
    browser_->setHeaderHidden(true);
    browserDock->setWidget(browser_);
    addDockWidget(Qt::LeftDockWidgetArea, browserDock);
    connect(browser_, &QTreeWidget::itemSelectionChanged, this,
            [this] { browserSelectionChanged(); });

    auto* propertyDock = new QDockWidget("Properties", this);
    propertyDock->setObjectName("PropertiesDock");
    properties_ = new QWidget(propertyDock);
    auto* propertyLayout = new QVBoxLayout(properties_);
    propertyForm_ = new QFormLayout();
    propertyLayout->addLayout(propertyForm_);
    propertyLayout->addStretch();
    propertyDock->setWidget(properties_);
    addDockWidget(Qt::RightDockWidgetArea, propertyDock);

    summary_ = new QLabel(this);
    statusBar()->addPermanentWidget(summary_);
    statusBar()->showMessage("Ready — draw walls with two clicks; place openings on a wall");
}

void MainWindow::chooseTool(BimViewport::Tool tool) {
    viewport_->setTool(tool);
    const QString instruction = tool == BimViewport::Tool::Wall
        ? "Wall tool: choose two endpoints"
        : tool == BimViewport::Tool::Door || tool == BimViewport::Tool::Window
            ? "Opening tool: click a wall host"
            : "Select BIM elements to inspect and edit properties";
    statusBar()->showMessage(instruction);
}

void MainWindow::refreshAll() {
    refreshBrowser();
    refreshProperties();
    refreshTitle();
    viewport_->update();
    undoAction_->setEnabled(controller_.canUndo());
    redoAction_->setEnabled(controller_.canRedo());
    const auto& d = controller_.document();
    summary_->setText(QString("Levels %1  Walls %2  Openings %3")
                          .arg(d.levels.size()).arg(d.walls.size())
                          .arg(d.doors.size() + d.windows.size()));
}

void MainWindow::refreshBrowser() {
    browser_->blockSignals(true);
    browser_->clear();
    const auto& d = controller_.document();
    auto* levels = category(browser_, "Views / Levels");
    for (const auto& level : d.levels)
        elementItem(levels, QString("%1  (%2 m)").arg(QString::fromStdString(level.name))
                                             .arg(level.elevation, 0, 'f', 2),
                    SelectionKind::Level, level.id);
    auto* walls = category(browser_, "Walls");
    for (const auto& wall : d.walls)
        elementItem(walls, QString("Wall %1  — %2 m").arg(wall.id)
                         .arg(d.wallLength(wall), 0, 'f', 2), SelectionKind::Wall, wall.id);
    auto* openings = category(browser_, "Hosted Openings");
    for (const auto& door : d.doors)
        elementItem(openings, QString("Door %1 → Wall %2").arg(door.id).arg(door.hostWallId),
                    SelectionKind::Door, door.id);
    for (const auto& window : d.windows)
        elementItem(openings, QString("Window %1 → Wall %2").arg(window.id).arg(window.hostWallId),
                    SelectionKind::Window, window.id);
    auto* rooms = category(browser_, "Rooms");
    for (const auto& room : d.rooms)
        elementItem(rooms, QString::fromStdString(room.name), SelectionKind::Room, room.id);

    const auto selected = controller_.selection();
    QTreeWidgetItemIterator it(browser_);
    while (*it) {
        if ((*it)->data(0, Qt::UserRole).toInt() == static_cast<int>(selected.kind) &&
            (*it)->data(0, Qt::UserRole + 1).toULongLong() == selected.id) {
            browser_->setCurrentItem(*it);
            break;
        }
        ++it;
    }
    browser_->blockSignals(false);
}

void MainWindow::refreshProperties() {
    while (propertyForm_->count() > 0) {
        auto* item = propertyForm_->takeAt(0);
        delete item->widget();
        delete item;
    }
    const auto selected = controller_.selection();
    const auto& d = controller_.document();
    if (selected.kind == SelectionKind::None) {
        propertyForm_->addRow(new QLabel("Select a level, wall, door, or window."));
        return;
    }
    propertyForm_->addRow("Element ID", new QLabel(QString::number(selected.id)));
    if (selected.kind == SelectionKind::Level) {
        const auto* level = d.findLevel(selected.id);
        if (!level) return;
        propertyForm_->addRow("Name", new QLabel(QString::fromStdString(level->name)));
        auto* elevation = measurement(level->elevation, properties_);
        propertyForm_->addRow("Elevation", elevation);
        auto* apply = new QPushButton("Apply level elevation", properties_);
        propertyForm_->addRow(apply);
        connect(apply, &QPushButton::clicked, this, [this, elevation, id = level->id] {
            if (!controller_.setLevelElevation(id, elevation->value()))
                QMessageBox::warning(this, "Invalid constraint", "The elevation would invert a constrained wall.");
            refreshAll();
        });
    } else if (selected.kind == SelectionKind::Wall) {
        const auto* wall = d.findWall(selected.id);
        if (!wall) return;
        const auto* type = d.findWallType(wall->typeId);
        propertyForm_->addRow("Type", new QLabel(type ? QString::fromStdString(type->name)
                                                       : QString("Missing type %1").arg(wall->typeId)));
        propertyForm_->addRow("Length", new QLabel(QString("%1 m").arg(d.wallLength(*wall), 0, 'f', 3)));
        propertyForm_->addRow("Height", new QLabel(QString("%1 m").arg(d.wallHeight(*wall), 0, 'f', 3)));
        auto* x0 = measurement(wall->x0, properties_);
        auto* y0 = measurement(wall->y0, properties_);
        auto* x1 = measurement(wall->x1, properties_);
        auto* y1 = measurement(wall->y1, properties_);
        propertyForm_->addRow("Start X", x0); propertyForm_->addRow("Start Y", y0);
        propertyForm_->addRow("End X", x1); propertyForm_->addRow("End Y", y1);
        auto* apply = new QPushButton("Apply wall location", properties_);
        propertyForm_->addRow(apply);
        connect(apply, &QPushButton::clicked, this, [this, x0, y0, x1, y1, id = wall->id] {
            if (!controller_.moveWall(id, x0->value(), y0->value(), x1->value(), y1->value()))
                QMessageBox::warning(this, "Invalid wall", "Wall endpoints must define a non-zero length.");
            refreshAll();
        });
    } else if (selected.kind == SelectionKind::Door || selected.kind == SelectionKind::Window) {
        double offset = 0.0;
        ElementId host = 0;
        if (selected.kind == SelectionKind::Door) {
            const auto it = std::find_if(d.doors.begin(), d.doors.end(),
                [selected](const Door& v) { return v.id == selected.id; });
            if (it == d.doors.end()) return;
            offset = it->offsetAlongWall; host = it->hostWallId;
        } else {
            const auto it = std::find_if(d.windows.begin(), d.windows.end(),
                [selected](const Window& v) { return v.id == selected.id; });
            if (it == d.windows.end()) return;
            offset = it->offsetAlongWall; host = it->hostWallId;
        }
        propertyForm_->addRow("Host wall", new QLabel(QString::number(host)));
        auto* position = new QDoubleSpinBox(properties_);
        position->setRange(0.0, 1.0); position->setDecimals(3); position->setValue(offset);
        propertyForm_->addRow("Normalized position", position);
        auto* apply = new QPushButton("Apply host position", properties_);
        propertyForm_->addRow(apply);
        connect(apply, &QPushButton::clicked, this, [this, position, selected] {
            controller_.setOpeningOffset(selected.kind, selected.id, position->value());
            refreshAll();
        });
    } else if (selected.kind == SelectionKind::Room) {
        const auto* room = d.findRoom(selected.id);
        if (!room) return;
        propertyForm_->addRow("Name", new QLabel(QString::fromStdString(room->name)));
        propertyForm_->addRow("Area", new QLabel(QString("%1 m²").arg(d.roomArea(*room), 0, 'f', 2)));
    }
}

void MainWindow::refreshTitle() {
    const QString file = controller_.filePath().empty()
        ? QString::fromStdString(controller_.document().name)
        : QString::fromStdString(controller_.filePath().filename().string());
    setWindowTitle(QString("%1%2 — MyDesk Buildings").arg(file, controller_.isModified() ? " *" : ""));
}

void MainWindow::browserSelectionChanged() {
    const auto items = browser_->selectedItems();
    if (items.empty()) return;
    const auto* item = items.front();
    controller_.select({static_cast<SelectionKind>(item->data(0, Qt::UserRole).toInt()),
                        item->data(0, Qt::UserRole + 1).toULongLong()});
    refreshProperties();
    viewport_->update();
}

bool MainWindow::confirmDiscard() {
    if (!controller_.isModified()) return true;
    const auto answer = QMessageBox::question(this, "Unsaved building project",
        "Save changes before continuing?", QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel) return false;
    return answer != QMessageBox::Save || saveDocument();
}

bool MainWindow::saveDocument(bool forcePath) {
    std::filesystem::path path = controller_.filePath();
    if (forcePath || path.empty()) {
        const auto chosen = QFileDialog::getSaveFileName(this, "Save Building Project", {},
                                                         "MyDesk BIM (*.mydesk-bim)");
        if (chosen.isEmpty()) return false;
        path = chosen.toStdString();
    }
    std::string error;
    const bool ok = forcePath || controller_.filePath().empty()
        ? controller_.saveAs(path, &error) : controller_.save(&error);
    if (!ok) QMessageBox::critical(this, "Save failed", QString::fromStdString(error));
    refreshTitle();
    return ok;
}

void MainWindow::openDocument() {
    if (!confirmDiscard()) return;
    const auto path = QFileDialog::getOpenFileName(this, "Open Building Project", {},
                                                   "MyDesk BIM (*.mydesk-bim);;All files (*)");
    if (path.isEmpty()) return;
    std::string error;
    if (!controller_.open(path.toStdString(), &error))
        QMessageBox::critical(this, "Open failed", QString::fromStdString(error));
    else refreshAll();
}

void MainWindow::exportIfc() {
    const auto path = QFileDialog::getSaveFileName(this, "Export IFC4", {}, "IFC4 (*.ifc)");
    if (path.isEmpty()) return;
    std::string error;
    if (!controller_.exportIfc(path.toStdString(), &error))
        QMessageBox::critical(this, "IFC export failed", QString::fromStdString(error));
    else statusBar()->showMessage("IFC4 model exported", 5000);
}

void MainWindow::createLevel() {
    QDialog dialog(this);
    dialog.setWindowTitle("Create Level");
    QFormLayout form(&dialog);
    QLineEdit name("Level " + QString::number(controller_.document().levels.size() + 1), &dialog);
    auto* elevation = measurement(static_cast<double>(controller_.document().levels.size()) * 3.2,
                                  &dialog);
    form.addRow("Name", &name); form.addRow("Elevation", elevation);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        controller_.createLevel(name.text().toStdString(), elevation->value());
        refreshAll();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    confirmDiscard() ? event->accept() : event->ignore();
}

} // namespace mydesk::buildings
