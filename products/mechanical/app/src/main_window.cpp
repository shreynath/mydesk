#include "main_window.hpp"
#include "shape_viewport.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QTableWidget>
#include <QToolBar>
#include <QTreeWidget>

namespace mydesk::mechanical::desktop {
namespace {

QString kindName(FeatureKind kind) {
    switch (kind) {
    case FeatureKind::SketchRectangle: return "Rectangle Sketch";
    case FeatureKind::SketchCircle: return "Circle Sketch";
    case FeatureKind::SketchPolygon: return "Polygon Sketch";
    case FeatureKind::Extrude: return "Extrude";
    case FeatureKind::Revolve: return "Revolve";
    case FeatureKind::Sweep: return "Sweep";
    case FeatureKind::Loft: return "Loft";
    case FeatureKind::Fillet: return "Fillet";
    case FeatureKind::Chamfer: return "Chamfer";
    case FeatureKind::Shell: return "Shell";
    case FeatureKind::RectangularPattern: return "Rectangular Pattern";
    case FeatureKind::MirrorFeature: return "Mirror";
    case FeatureKind::WorkPlane: return "Work Plane";
    case FeatureKind::Hole: return "Hole";
    case FeatureKind::Thread: return "Thread";
    case FeatureKind::Rib: return "Rib";
    case FeatureKind::Emboss: return "Emboss";
    }
    return "Feature";
}

bool askNumber(QWidget* parent, const QString& label, double initial, double minimum,
               double maximum, double& value) {
    bool accepted = false;
    value = QInputDialog::getDouble(parent, "Feature Parameters", label, initial, minimum,
                                    maximum, 3, &accepted);
    return accepted;
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("MyDesk Mechanical — Untitled Part");
    resize(1280, 780);
    viewport_ = new ShapeViewport(this);
    setCentralWidget(viewport_);
    createDocks();
    createActions();
    status_ = new QLabel(this);
    statusBar()->addPermanentWidget(status_);
    connect(viewport_, &ShapeViewport::bodyPicked, this, [this] {
        viewport_->setBodySelected(true);
        statusBar()->showMessage("Evaluated body selected", 1800);
    });
    QSettings settings;
    restoreGeometry(settings.value("mechanical/windowGeometry").toByteArray());
    restoreState(settings.value("mechanical/windowState").toByteArray());
    refreshAll();
}

void MainWindow::createDocks() {
    auto* browserDock = new QDockWidget("Model Browser", this);
    browserDock->setObjectName("MechanicalModelBrowser");
    browser_ = new QTreeWidget(browserDock);
    browser_->setHeaderLabels({"Feature / Component", "Type"});
    browser_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    browserDock->setWidget(browser_);
    addDockWidget(Qt::LeftDockWidgetArea, browserDock);

    auto* propertyDock = new QDockWidget("Parameters", this);
    propertyDock->setObjectName("MechanicalParameters");
    properties_ = new QTableWidget(propertyDock);
    properties_->setColumnCount(3);
    properties_->setHorizontalHeaderLabels({"Parameter", "Value", "Unit"});
    properties_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    properties_->verticalHeader()->hide();
    propertyDock->setWidget(properties_);
    addDockWidget(Qt::RightDockWidgetArea, propertyDock);
    connect(properties_, &QTableWidget::cellChanged, this, &MainWindow::onParameterChanged);
    connect(browser_, &QTreeWidget::itemSelectionChanged, this, [this] {
        viewport_->setBodySelected(!browser_->selectedItems().empty());
    });
}

void MainWindow::createActions() {
    auto* file = menuBar()->addMenu("&File");
    auto* newPart = file->addAction("New &Part");
    newPart->setShortcut(QKeySequence::New);
    connect(newPart, &QAction::triggered, this, [this] {
        if (maybeSave()) { session_.newPart(); refreshAll(); }
    });
    auto* newAssembly = file->addAction("New &Assembly from Current Part");
    connect(newAssembly, &QAction::triggered, this, [this] {
        if (maybeSave()) { session_.newAssembly(); refreshAll(); }
    });
    auto* open = file->addAction("&Open…");
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, this, &MainWindow::openDocument);
    auto* save = file->addAction("&Save");
    save->setShortcut(QKeySequence::Save);
    connect(save, &QAction::triggered, this, [this] { saveDocument(false); });
    auto* saveAs = file->addAction("Save &As…");
    saveAs->setShortcut(QKeySequence::SaveAs);
    connect(saveAs, &QAction::triggered, this, [this] { saveDocument(true); });
    file->addSeparator();
    auto* step = file->addAction("Export STEP…");
    connect(step, &QAction::triggered, this, &MainWindow::exportStep);
    igesAction_ = file->addAction("Export IGES…");
    connect(igesAction_, &QAction::triggered, this, &MainWindow::exportIges);
    file->addSeparator();
    auto* quit = file->addAction("Quit");
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);

    auto* edit = menuBar()->addMenu("&Edit");
    undoAction_ = edit->addAction("Undo");
    undoAction_->setShortcut(QKeySequence::Undo);
    connect(undoAction_, &QAction::triggered, this, [this] {
        QString error; if (!session_.undo(&error) && !error.isEmpty()) showError("Undo", error);
        refreshAll();
    });
    redoAction_ = edit->addAction("Redo");
    redoAction_->setShortcut(QKeySequence::Redo);
    connect(redoAction_, &QAction::triggered, this, [this] {
        QString error; if (!session_.redo(&error) && !error.isEmpty()) showError("Redo", error);
        refreshAll();
    });

    auto* model = menuBar()->addMenu("&Model");
    boxAction_ = model->addAction("Create Box Extrusion…");
    connect(boxAction_, &QAction::triggered, this, &MainWindow::createBox);
    holeAction_ = model->addAction("Create Hole…");
    connect(holeAction_, &QAction::triggered, this, &MainWindow::createHole);
    insertAction_ = model->addAction("Insert Current Part Occurrence…");
    connect(insertAction_, &QAction::triggered, this, &MainWindow::insertPart);

    auto* view = menuBar()->addMenu("&View");
    auto* fit = view->addAction("Fit All");
    fit->setShortcut(Qt::Key_F);
    connect(fit, &QAction::triggered, viewport_, &ShapeViewport::fitAll);

    auto* toolbar = addToolBar("Mechanical Features");
    toolbar->setObjectName("MechanicalFeatureToolbar");
    toolbar->addAction(boxAction_);
    toolbar->addAction(holeAction_);
    toolbar->addSeparator();
    toolbar->addAction(undoAction_);
    toolbar->addAction(redoAction_);
}

void MainWindow::refreshAll() {
    viewport_->setBodySelected(false);
    viewport_->setShape(session_.displayShape());
    refreshBrowser();
    refreshProperties();
    updateActions();
    const QString fileName = session_.path().isEmpty() ? "Untitled" : session_.path();
    const QString kind = session_.kind() == MechanicalSession::DocumentKind::Part ? "Part" : "Assembly";
    setWindowTitle(QString("MyDesk Mechanical — %1 %2%3")
                       .arg(fileName, kind, session_.isDirty() ? " *" : ""));
    if (session_.kind() == MechanicalSession::DocumentKind::Part) {
        status_->setText(QString("%1 features • %2 mm³")
                             .arg(session_.part().features().size())
                             .arg(session_.part().solidVolume(), 0, 'f', 3));
    } else {
        status_->setText(QString("%1 components • %2 joints • %3 mm³")
                             .arg(session_.assembly().occurrences().size())
                             .arg(session_.assembly().joints().size())
                             .arg(session_.assembly().totalVolume(), 0, 'f', 3));
    }
}

void MainWindow::refreshBrowser() {
    browser_->clear();
    if (session_.kind() == MechanicalSession::DocumentKind::Part) {
        auto* origin = new QTreeWidgetItem(browser_, {"Origin", "Reference"});
        new QTreeWidgetItem(origin, {"XY Plane", "Work Plane"});
        new QTreeWidgetItem(origin, {"XZ Plane", "Work Plane"});
        new QTreeWidgetItem(origin, {"YZ Plane", "Work Plane"});
        auto* model = new QTreeWidgetItem(browser_,
            {QString::fromStdString(session_.part().name()), "Part"});
        for (const auto& feature : session_.part().features()) {
            auto* item = new QTreeWidgetItem(model,
                {QString::fromStdString(feature.name), kindName(feature.kind)});
            item->setData(0, Qt::UserRole, static_cast<qulonglong>(feature.id));
        }
        browser_->expandAll();
    } else {
        auto* components = new QTreeWidgetItem(browser_,
            {QString::fromStdString(session_.assembly().name()), "Assembly"});
        for (const auto& occurrence : session_.assembly().occurrences()) {
            auto* item = new QTreeWidgetItem(components,
                {QString::fromStdString(occurrence.name), occurrence.isGrounded ? "Grounded Part" : "Part"});
            item->setData(0, Qt::UserRole, static_cast<qulonglong>(occurrence.id));
        }
        auto* constraints = new QTreeWidgetItem(browser_, {"Constraints", "Folder"});
        for (const auto& constraint : session_.assembly().constraints())
            new QTreeWidgetItem(constraints, {QString::fromStdString(constraint.name), "Constraint"});
        auto* joints = new QTreeWidgetItem(browser_, {"Joints", "Folder"});
        for (const auto& joint : session_.assembly().joints())
            new QTreeWidgetItem(joints, {QString::fromStdString(joint.name), "Motion Joint"});
        browser_->expandAll();
    }
}

void MainWindow::refreshProperties() {
    refreshing_ = true;
    properties_->clearContents();
    if (session_.kind() == MechanicalSession::DocumentKind::Part) {
        const auto& parameters = session_.part().parameters();
        properties_->setRowCount(static_cast<int>(parameters.size()));
        for (int row = 0; row < static_cast<int>(parameters.size()); ++row) {
            const auto& parameter = parameters[static_cast<std::size_t>(row)];
            auto* name = new QTableWidgetItem(QString::fromStdString(parameter.name));
            name->setFlags(name->flags() & ~Qt::ItemIsEditable);
            auto* value = new QTableWidgetItem(QString::number(parameter.value, 'g', 12));
            value->setData(Qt::UserRole, static_cast<qulonglong>(parameter.id));
            auto* unit = new QTableWidgetItem(QString::fromStdString(parameter.unit));
            unit->setFlags(unit->flags() & ~Qt::ItemIsEditable);
            properties_->setItem(row, 0, name);
            properties_->setItem(row, 1, value);
            properties_->setItem(row, 2, unit);
        }
    } else {
        properties_->setRowCount(static_cast<int>(session_.assembly().joints().size()));
        int row = 0;
        for (const auto& joint : session_.assembly().joints()) {
            auto* name = new QTableWidgetItem(QString::fromStdString(joint.name));
            name->setFlags(name->flags() & ~Qt::ItemIsEditable);
            auto* value = new QTableWidgetItem(QString::number(joint.position, 'g', 12));
            value->setData(Qt::UserRole, static_cast<qulonglong>(joint.id));
            auto* unit = new QTableWidgetItem(joint.type == JointType::Revolute ? "deg" : "mm");
            unit->setFlags(unit->flags() & ~Qt::ItemIsEditable);
            properties_->setItem(row, 0, name);
            properties_->setItem(row, 1, value);
            properties_->setItem(row, 2, unit);
            ++row;
        }
    }
    refreshing_ = false;
}

void MainWindow::updateActions() {
    const bool part = session_.kind() == MechanicalSession::DocumentKind::Part;
    undoAction_->setEnabled(session_.canUndo());
    redoAction_->setEnabled(session_.canRedo());
    boxAction_->setEnabled(part);
    holeAction_->setEnabled(part && session_.part().hasSolid());
    insertAction_->setEnabled(!part);
    igesAction_->setEnabled(part && session_.part().hasSolid());
}

void MainWindow::onParameterChanged(int row, int column) {
    if (refreshing_ || column != 1) return;
    auto* item = properties_->item(row, column);
    bool numberOk = false;
    const double value = item->text().toDouble(&numberOk);
    QString error;
    const auto id = static_cast<ParameterId>(item->data(Qt::UserRole).toULongLong());
    const bool ok = numberOk && (session_.kind() == MechanicalSession::DocumentKind::Part
        ? session_.setParameter(id, value, &error)
        : session_.setJointPosition(static_cast<JointId>(id), value, &error));
    if (!ok) showError("Parameter Edit", error.isEmpty() ? "Enter a valid numeric value." : error);
    refreshAll();
}

void MainWindow::createBox() {
    bool accepted = false;
    const QString name = QInputDialog::getText(this, "Box Extrusion", "Feature name:",
                                               QLineEdit::Normal, "Base Extrude", &accepted);
    if (!accepted || name.trimmed().isEmpty()) return;
    double width = 40, height = 25, depth = 10;
    if (!askNumber(this, "Width (mm)", width, 0.001, 1e6, width) ||
        !askNumber(this, "Height (mm)", height, 0.001, 1e6, height) ||
        !askNumber(this, "Extrusion depth (mm)", depth, 0.001, 1e6, depth)) return;
    QString error;
    if (!session_.createBox(name.toStdString(), width, height, depth, &error))
        showError("Create Box", error);
    refreshAll();
}

void MainWindow::createHole() {
    double radius = 3, depth = 10, x = 10, y = 10;
    if (!askNumber(this, "Radius (mm)", radius, 0.001, 1e6, radius) ||
        !askNumber(this, "Depth (mm)", depth, 0.001, 1e6, depth) ||
        !askNumber(this, "Center X (mm)", x, -1e6, 1e6, x) ||
        !askNumber(this, "Center Y (mm)", y, -1e6, 1e6, y)) return;
    QString error;
    if (!session_.createHole("Hole", radius, depth, x, y, &error)) showError("Create Hole", error);
    refreshAll();
}

void MainWindow::insertPart() {
    const int number = static_cast<int>(session_.assembly().occurrences().size()) + 1;
    QString error;
    if (!session_.insertCurrentPart("Component" + std::to_string(number),
                                    (number - 1) * 50.0, 0, 0, &error))
        showError("Insert Component", error);
    refreshAll();
}

void MainWindow::openDocument() {
    if (!maybeSave()) return;
    const QString path = QFileDialog::getOpenFileName(this, "Open Mechanical Document", {},
        "MyDesk Mechanical (*.mydesk-mech *.mydesk-assy);;All Files (*)");
    if (path.isEmpty()) return;
    QString error;
    if (!session_.open(path, &error)) showError("Open Document", error);
    refreshAll();
}

bool MainWindow::saveDocument(bool choosePath) {
    QString path = session_.path();
    if (choosePath || path.isEmpty()) {
        const QString filter = session_.kind() == MechanicalSession::DocumentKind::Part
            ? "MyDesk Mechanical Part (*.mydesk-mech)"
            : "MyDesk Mechanical Assembly (*.mydesk-assy)";
        path = QFileDialog::getSaveFileName(this, "Save Mechanical Document", path, filter);
        if (path.isEmpty()) return false;
    }
    QString error;
    const bool ok = session_.saveAs(path, &error);
    if (!ok) showError("Save Document", error);
    refreshAll();
    return ok;
}

void MainWindow::exportStep() {
    const QString path = QFileDialog::getSaveFileName(this, "Export STEP", {}, "STEP (*.step *.stp)");
    if (path.isEmpty()) return;
    QString error;
    if (!session_.exportStep(path, &error)) showError("STEP Export", error);
}

void MainWindow::exportIges() {
    const QString path = QFileDialog::getSaveFileName(this, "Export IGES", {}, "IGES (*.iges *.igs)");
    if (path.isEmpty()) return;
    QString error;
    if (!session_.exportIges(path, &error)) showError("IGES Export", error);
}

bool MainWindow::maybeSave() {
    if (!session_.isDirty()) return true;
    const auto response = QMessageBox::warning(this, "Unsaved Mechanical Document",
        "The current document has unsaved model changes.",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (response == QMessageBox::Cancel) return false;
    return response == QMessageBox::Discard || saveDocument(false);
}

void MainWindow::showError(const QString& title, const QString& message) {
    QMessageBox::critical(this, title, message);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!maybeSave()) { event->ignore(); return; }
    QSettings settings;
    settings.setValue("mechanical/windowGeometry", saveGeometry());
    settings.setValue("mechanical/windowState", saveState());
    event->accept();
}

} // namespace mydesk::mechanical::desktop
