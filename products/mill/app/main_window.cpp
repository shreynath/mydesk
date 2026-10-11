// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include "mill_viewport.hpp"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>

using namespace mydesk::mill;

namespace {
constexpr int kKindRole = Qt::UserRole;
constexpr int kIndexRole = Qt::UserRole + 1;

QDoubleSpinBox* spin(double minimum, double maximum, int decimals = 3) {
    auto* value = new QDoubleSpinBox;
    value->setRange(minimum, maximum);
    value->setDecimals(decimals);
    value->setSingleStep(decimals == 0 ? 1.0 : 0.5);
    value->setKeyboardTracking(false);
    value->setMinimumHeight(26);
    return value;
}

QString strategyName(Strategy strategy) {
    switch (strategy) {
        case Strategy::RasterZigZag: return QObject::tr("Raster zig-zag");
        case Strategy::ParallelOffset: return QObject::tr("Contour parallel");
        case Strategy::PencilMilling: return QObject::tr("Pencil boundary");
        case Strategy::Drilling: return QObject::tr("Drilling");
        case Strategy::Profile: return QObject::tr("Profile");
    }
    return {};
}

std::filesystem::path nativePath(const QString& path) {
#ifdef _WIN32
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toStdString());
#endif
}
}

MillMainWindow::MillMainWindow(QWidget* parent) : QMainWindow(parent) {
    setObjectName("millMainWindow");
    setMinimumSize(1100, 700);
    resize(1440, 900);
    buildUi();
    buildMenus();
    newDocument();

    QSettings settings;
    if (settings.contains("mill/mainGeometry")) restoreGeometry(settings.value("mill/mainGeometry").toByteArray());
    if (settings.contains("mill/mainState")) restoreState(settings.value("mill/mainState").toByteArray());
}

void MillMainWindow::buildUi() {
    viewport_ = new MillViewport(this);
    setCentralWidget(viewport_);

    auto* projectDock = new QDockWidget(tr("CAM Project"), this);
    projectDock->setObjectName("projectDock");
    projectTree_ = new QTreeWidget(projectDock);
    projectTree_->setObjectName("projectTree");
    projectTree_->setHeaderLabels({tr("Item"), tr("State")});
    projectTree_->setAlternatingRowColors(true);
    projectTree_->setAccessibleName(tr("CAM project browser"));
    projectDock->setWidget(projectTree_);
    addDockWidget(Qt::LeftDockWidgetArea, projectDock);

    auto* propertyDock = new QDockWidget(tr("Machining Properties"), this);
    propertyDock->setObjectName("propertyDock");
    auto* propertyContainer = new QWidget(propertyDock);
    auto* propertyLayout = new QVBoxLayout(propertyContainer);
    selectionSummary_ = new QLabel(tr("Select stock, a tool, boundary, or operation."));
    selectionSummary_->setObjectName("selectionSummary");
    selectionSummary_->setWordWrap(true);
    propertyLayout->addWidget(selectionSummary_);
    propertyStack_ = new QStackedWidget;
    propertyStack_->setObjectName("propertyStack");

    propertyStack_->addWidget(new QLabel(tr("Select an item in the CAM Project browser to edit its parameters.")));
    auto* stockPage = new QWidget;
    auto* stockForm = new QFormLayout(stockPage);
    const QString stockLabels[6] = {tr("Min X (mm)"), tr("Min Y (mm)"), tr("Min Z (mm)"),
                                    tr("Max X (mm)"), tr("Max Y (mm)"), tr("Max Z (mm)")};
    for (int i = 0; i < 6; ++i) {
        stockValues_[i] = spin(-100000.0, 100000.0);
        stockForm->addRow(stockLabels[i], stockValues_[i]);
    }
    propertyStack_->addWidget(stockPage);

    auto* toolPage = new QWidget;
    auto* toolForm = new QFormLayout(toolPage);
    toolName_ = new QLineEdit;
    toolKind_ = new QComboBox;
    toolKind_->addItems({tr("Flat endmill"), tr("Ball endmill"), tr("Bullnose endmill"),
                         tr("Drill bit"), tr("Chamfer mill"), tr("Form tool")});
    toolDiameter_ = spin(0.01, 1000.0);
    toolFlutes_ = spin(1.0, 100.0, 0);
    toolCutLength_ = spin(0.01, 10000.0);
    toolSurfaceSpeed_ = spin(0.01, 10000.0);
    toolChipLoad_ = spin(0.0001, 100.0, 4);
    toolForm->addRow(tr("Name"), toolName_);
    toolForm->addRow(tr("Type"), toolKind_);
    toolForm->addRow(tr("Diameter (mm)"), toolDiameter_);
    toolForm->addRow(tr("Flutes"), toolFlutes_);
    toolForm->addRow(tr("Cut length (mm)"), toolCutLength_);
    toolForm->addRow(tr("Surface speed (m/min)"), toolSurfaceSpeed_);
    toolForm->addRow(tr("Feed/tooth (mm)"), toolChipLoad_);
    propertyStack_->addWidget(toolPage);

    auto* boundaryPage = new QWidget;
    auto* boundaryForm = new QFormLayout(boundaryPage);
    boundaryName_ = new QLineEdit;
    boundaryDetails_ = new QLabel;
    boundaryDetails_->setWordWrap(true);
    boundaryForm->addRow(tr("Name"), boundaryName_);
    boundaryForm->addRow(tr("Geometry"), boundaryDetails_);
    propertyStack_->addWidget(boundaryPage);

    auto* operationPage = new QWidget;
    auto* operationForm = new QFormLayout(operationPage);
    operationName_ = new QLineEdit;
    operationStrategy_ = new QComboBox;
    for (int i = 0; i <= static_cast<int>(Strategy::Profile); ++i) {
        operationStrategy_->addItem(strategyName(static_cast<Strategy>(i)), i);
    }
    operationTool_ = new QComboBox;
    operationBoundary_ = new QComboBox;
    operationStepover_ = spin(0.0, 10000.0);
    operationStepdown_ = spin(0.001, 10000.0);
    operationStepdown_->setObjectName("operationStepdown");
    operationTop_ = spin(-100000.0, 100000.0);
    operationBottom_ = spin(-100000.0, 100000.0);
    operationSafe_ = spin(-100000.0, 100000.0);
    operationTolerance_ = spin(0.000001, 100.0, 5);
    operationClimb_ = new QCheckBox(tr("Use climb direction"));
    operationForm->addRow(tr("Name"), operationName_);
    operationForm->addRow(tr("Strategy"), operationStrategy_);
    operationForm->addRow(tr("Tool"), operationTool_);
    operationForm->addRow(tr("Boundary"), operationBoundary_);
    operationForm->addRow(tr("Stepover (mm)"), operationStepover_);
    operationForm->addRow(tr("Stepdown (mm)"), operationStepdown_);
    operationForm->addRow(tr("Top Z (mm)"), operationTop_);
    operationForm->addRow(tr("Bottom Z (mm)"), operationBottom_);
    operationForm->addRow(tr("Safe Z (mm)"), operationSafe_);
    operationForm->addRow(tr("Tolerance (mm)"), operationTolerance_);
    operationForm->addRow(QString(), operationClimb_);
    propertyStack_->addWidget(operationPage);

    propertyLayout->addWidget(propertyStack_, 1);
    applyButton_ = new QPushButton(tr("Apply / Regenerate"));
    applyButton_->setObjectName("applyPropertiesButton");
    applyButton_->setMinimumHeight(36);
    propertyLayout->addWidget(applyButton_);
    propertyDock->setWidget(propertyContainer);
    addDockWidget(Qt::RightDockWidgetArea, propertyDock);

    connect(projectTree_, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current) {
        if (!current || !current->data(0, kKindRole).isValid()) {
            showSelection({});
            return;
        }
        showSelection({static_cast<SelectionKind>(current->data(0, kKindRole).toInt()),
                       current->data(0, kIndexRole).toULongLong()});
    });
    connect(viewport_, &MillViewport::toolpathSelected, this, [this](int index) {
        rebuildTree(Selection{SelectionKind::Operation, static_cast<std::size_t>(index)});
    });
    connect(applyButton_, &QPushButton::clicked, this, &MillMainWindow::applyProperties);
}

void MillMainWindow::buildMenus() {
    auto* file = menuBar()->addMenu(tr("&File"));
    auto* newAction = file->addAction(tr("&New"), QKeySequence::New, this, &MillMainWindow::newDocument);
    newAction->setObjectName("newAction");
    file->addAction(tr("&Open…"), QKeySequence::Open, this, &MillMainWindow::openInteractive);
    saveAction_ = file->addAction(tr("&Save"), QKeySequence::Save, this, [this] { saveCurrent(); });
    file->addAction(tr("Save &As…"), QKeySequence::SaveAs, this, &MillMainWindow::saveAs);
    file->addSeparator();
    auto* exportAction = file->addAction(tr("Export Generic G-code…"), this, &MillMainWindow::exportInteractive);
    exportAction->setObjectName("exportGcodeAction");
    file->addSeparator();
    file->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    auto* edit = menuBar()->addMenu(tr("&Edit"));
    undoAction_ = edit->addAction(tr("&Undo"), QKeySequence::Undo, this, &MillMainWindow::undo);
    redoAction_ = edit->addAction(tr("&Redo"), QKeySequence::Redo, this, &MillMainWindow::redo);
    edit->addSeparator();
    auto* deleteAction = edit->addAction(tr("Delete Selected"), QKeySequence::Delete, this,
                                         &MillMainWindow::deleteSelection);
    deleteAction->setObjectName("deleteSelectionAction");

    auto* create = menuBar()->addMenu(tr("&Create"));
    auto* toolAction = create->addAction(tr("Cutting Tool"), this, &MillMainWindow::createTool);
    toolAction->setObjectName("createToolAction");
    auto* boundaryAction = create->addAction(tr("Rectangular Boundary"), this, &MillMainWindow::createBoundary);
    boundaryAction->setObjectName("createBoundaryAction");
    auto* operationAction = create->addAction(tr("Toolpath Operation"), this, &MillMainWindow::createOperation);
    operationAction->setObjectName("createOperationAction");

    auto* view = menuBar()->addMenu(tr("&View"));
    auto* topAction = view->addAction(tr("Top View"));
    topAction->setCheckable(true);
    connect(topAction, &QAction::toggled, viewport_, &MillViewport::setTopView);
    view->addAction(tr("Fit Stock"), QKeySequence(Qt::Key_F), viewport_, &MillViewport::fitView);

    auto* toolbar = addToolBar(tr("CAM"));
    toolbar->setObjectName("camToolbar");
    toolbar->setMovable(false);
    toolbar->addAction(toolAction);
    toolbar->addAction(boundaryAction);
    toolbar->addAction(operationAction);
    toolbar->addSeparator();
    toolbar->addAction(undoAction_);
    toolbar->addAction(redoAction_);
}

void MillMainWindow::newDocument() {
    if (!confirmDiscard()) return;
    document_ = MillDocument{};
    document_.set_name("Untitled Mill Project");
    document_.set_stock({{0.0, 0.0, -20.0}, {150.0, 100.0, 0.0}});
    Tool tool;
    tool.name = "D10 Flat Rougher";
    tool.diameter_mm = 10.0;
    tool.shank_diameter_mm = 10.0;
    tool.cutting_length_mm = 30.0;
    document_.add_tool(tool);
    document_.clear_history();
    currentPath_.clear();
    dirty_ = false;
    viewport_->setDocument(&document_);
    rebuildTree(Selection{SelectionKind::Stock, 0});
    statusBar()->showMessage(tr("New 150 × 100 × 20 mm setup"), 4000);
    updateWindowState();
}

void MillMainWindow::createTool() {
    Tool tool;
    tool.name = tr("D6 Finishing Endmill").toStdString();
    tool.diameter_mm = 6.0;
    tool.shank_diameter_mm = 6.0;
    tool.cutting_length_mm = 24.0;
    try {
        const std::size_t index = document_.add_tool(tool);
        markChanged();
        rebuildTree(Selection{SelectionKind::Tool, index});
    } catch (const std::exception& error) { setError(QString::fromUtf8(error.what())); }
}

void MillMainWindow::createBoundary() {
    const StockBox& stock = document_.stock();
    const double insetX = (stock.max_corner.x - stock.min_corner.x) * 0.1;
    const double insetY = (stock.max_corner.y - stock.min_corner.y) * 0.1;
    MachiningBoundary boundary{
        tr("Machining Boundary %1").arg(document_.boundary_count() + 1).toStdString(),
        {{stock.min_corner.x + insetX, stock.min_corner.y + insetY},
         {stock.max_corner.x - insetX, stock.min_corner.y + insetY},
         {stock.max_corner.x - insetX, stock.max_corner.y - insetY},
         {stock.min_corner.x + insetX, stock.max_corner.y - insetY}}};
    try {
        const std::size_t index = document_.add_boundary(std::move(boundary));
        markChanged();
        rebuildTree(Selection{SelectionKind::Boundary, index});
    } catch (const std::exception& error) { setError(QString::fromUtf8(error.what())); }
}

void MillMainWindow::createOperation() {
    if (document_.tool_count() == 0) {
        setError(tr("Create a cutting tool before calculating an operation."));
        return;
    }
    const StockBox& stock = document_.stock();
    OperationParams params;
    params.strategy = Strategy::RasterZigZag;
    params.tool_index = 0;
    params.boundary_index = document_.boundary_count() ? std::optional<std::size_t>(0) : std::nullopt;
    params.stepover_mm = document_.tool(0).diameter_mm * 0.45;
    params.stepdown_mm = std::min(3.0, (stock.max_corner.z - stock.min_corner.z) * 0.5);
    params.z_top_mm = stock.max_corner.z;
    params.z_bottom_mm = std::max(stock.min_corner.z, stock.max_corner.z - 5.0);
    params.safe_z_mm = stock.max_corner.z + 10.0;
    try {
        const std::size_t index = document_.generate(
            params, tr("Raster %1").arg(document_.toolpath_count() + 1).toStdString());
        markChanged();
        rebuildTree(Selection{SelectionKind::Operation, index});
        statusBar()->showMessage(tr("Calculated %1 moves").arg(document_.toolpath(index).moves.size()), 4000);
    } catch (const std::exception& error) { setError(QString::fromUtf8(error.what())); }
}

void MillMainWindow::applyProperties() {
    try {
        switch (selection_.kind) {
            case SelectionKind::Stock: {
                document_.set_stock({{stockValues_[0]->value(), stockValues_[1]->value(), stockValues_[2]->value()},
                                     {stockValues_[3]->value(), stockValues_[4]->value(), stockValues_[5]->value()}});
                break;
            }
            case SelectionKind::Tool: {
                Tool tool = document_.tool(selection_.index);
                tool.name = toolName_->text().trimmed().toStdString();
                tool.kind = static_cast<ToolKind>(toolKind_->currentIndex());
                tool.diameter_mm = toolDiameter_->value();
                tool.shank_diameter_mm = toolDiameter_->value();
                tool.corner_radius_mm = tool.kind == ToolKind::BallEndmill ? tool.diameter_mm * 0.5 : 0.0;
                tool.flutes = toolFlutes_->value();
                tool.cutting_length_mm = toolCutLength_->value();
                tool.overall_length_mm = std::max(tool.overall_length_mm, tool.cutting_length_mm);
                tool.surface_speed_m_min = toolSurfaceSpeed_->value();
                tool.feed_per_tooth_mm = toolChipLoad_->value();
                document_.set_tool(selection_.index, std::move(tool));
                break;
            }
            case SelectionKind::Boundary: {
                MachiningBoundary boundary = document_.boundary(selection_.index);
                boundary.name = boundaryName_->text().trimmed().toStdString();
                document_.set_boundary(selection_.index, std::move(boundary));
                break;
            }
            case SelectionKind::Operation: {
                const std::string operationName = operationName_->text().trimmed().toStdString();
                if (operationName.empty()) throw std::invalid_argument("operation name cannot be empty");
                OperationParams params = document_.toolpath(selection_.index).params;
                params.strategy = static_cast<Strategy>(operationStrategy_->currentData().toInt());
                params.tool_index = operationTool_->currentData().toULongLong();
                const int boundary = operationBoundary_->currentData().toInt();
                params.boundary_index = boundary < 0 ? std::nullopt
                                                     : std::optional<std::size_t>(static_cast<std::size_t>(boundary));
                params.stepover_mm = operationStepover_->value();
                params.stepdown_mm = operationStepdown_->value();
                params.z_top_mm = operationTop_->value();
                params.z_bottom_mm = operationBottom_->value();
                params.safe_z_mm = operationSafe_->value();
                params.tolerance_mm = operationTolerance_->value();
                params.climb_cut = operationClimb_->isChecked();
                if (params.strategy == Strategy::Drilling && params.drill_points.empty()) {
                    const StockBox& stock = document_.stock();
                    params.drill_points.push_back({(stock.min_corner.x + stock.max_corner.x) * 0.5,
                                                   (stock.min_corner.y + stock.max_corner.y) * 0.5});
                }
                document_.regenerate(selection_.index, params, operationName);
                break;
            }
            case SelectionKind::None: return;
        }
        const Selection selected = selection_;
        markChanged();
        rebuildTree(selected);
        statusBar()->showMessage(tr("Parameters applied"), 3000);
    } catch (const std::exception& error) {
        setError(QString::fromUtf8(error.what()));
    }
}

void MillMainWindow::deleteSelection() {
    bool changed = false;
    switch (selection_.kind) {
        case SelectionKind::Tool: changed = document_.remove_tool(selection_.index); break;
        case SelectionKind::Boundary: changed = document_.remove_boundary(selection_.index); break;
        case SelectionKind::Operation: changed = document_.remove_toolpath(selection_.index); break;
        default: break;
    }
    if (changed) {
        markChanged();
        rebuildTree();
    }
}

void MillMainWindow::undo() {
    if (document_.undo()) {
        markChanged();
        rebuildTree();
    }
}

void MillMainWindow::redo() {
    if (document_.redo()) {
        markChanged();
        rebuildTree();
    }
}

void MillMainWindow::rebuildTree(std::optional<Selection> preferred) {
    projectTree_->clear();
    auto* setup = new QTreeWidgetItem(projectTree_, {tr("Setup"), tr("1")});
    auto* stock = new QTreeWidgetItem(setup, {tr("Stock"), tr("Box")});
    stock->setData(0, kKindRole, static_cast<int>(SelectionKind::Stock));
    stock->setData(0, kIndexRole, 0);
    auto* tools = new QTreeWidgetItem(projectTree_, {tr("Cutting Tools"), QString::number(document_.tool_count())});
    for (std::size_t i = 0; i < document_.tool_count(); ++i) {
        const Tool& tool = document_.tool(i);
        auto* item = new QTreeWidgetItem(tools, {QString::fromStdString(tool.name),
                                                tr("D%1").arg(tool.diameter_mm, 0, 'f', 2)});
        item->setData(0, kKindRole, static_cast<int>(SelectionKind::Tool));
        item->setData(0, kIndexRole, static_cast<qulonglong>(i));
    }
    auto* boundaries = new QTreeWidgetItem(projectTree_, {tr("Boundaries"), QString::number(document_.boundary_count())});
    for (std::size_t i = 0; i < document_.boundary_count(); ++i) {
        auto* item = new QTreeWidgetItem(boundaries, {QString::fromStdString(document_.boundary(i).name),
                                                      tr("%1 vertices").arg(document_.boundary(i).polygon.size())});
        item->setData(0, kKindRole, static_cast<int>(SelectionKind::Boundary));
        item->setData(0, kIndexRole, static_cast<qulonglong>(i));
    }
    auto* operations = new QTreeWidgetItem(projectTree_, {tr("Toolpath Operations"), QString::number(document_.toolpath_count())});
    for (std::size_t i = 0; i < document_.toolpath_count(); ++i) {
        const Toolpath& path = document_.toolpath(i);
        auto* item = new QTreeWidgetItem(operations, {QString::fromStdString(path.name),
                                                      tr("%1 moves").arg(path.moves.size())});
        item->setData(0, kKindRole, static_cast<int>(SelectionKind::Operation));
        item->setData(0, kIndexRole, static_cast<qulonglong>(i));
    }
    projectTree_->expandAll();
    projectTree_->resizeColumnToContents(0);

    if (preferred) {
        QTreeWidgetItemIterator iterator(projectTree_);
        while (*iterator) {
            QTreeWidgetItem* item = *iterator;
            if (item->data(0, kKindRole).isValid() &&
                item->data(0, kKindRole).toInt() == static_cast<int>(preferred->kind) &&
                item->data(0, kIndexRole).toULongLong() == preferred->index) {
                projectTree_->setCurrentItem(item);
                break;
            }
            ++iterator;
        }
    } else {
        showSelection({});
    }
    viewport_->update();
    updateWindowState();
}

void MillMainWindow::showSelection(Selection selection) {
    selection_ = selection;
    propertyStack_->setCurrentIndex(static_cast<int>(selection.kind));
    viewport_->setSelectedToolpath(selection.kind == SelectionKind::Operation
        ? std::optional<std::size_t>(selection.index) : std::nullopt);
    switch (selection.kind) {
        case SelectionKind::Stock: {
            const StockBox& stock = document_.stock();
            const double values[6] = {stock.min_corner.x, stock.min_corner.y, stock.min_corner.z,
                                      stock.max_corner.x, stock.max_corner.y, stock.max_corner.z};
            for (int i = 0; i < 6; ++i) stockValues_[i]->setValue(values[i]);
            selectionSummary_->setText(tr("Stock envelope · editing invalidates calculated paths"));
            break;
        }
        case SelectionKind::Tool: {
            if (selection.index >= document_.tool_count()) return;
            const Tool& tool = document_.tool(selection.index);
            toolName_->setText(QString::fromStdString(tool.name));
            toolKind_->setCurrentIndex(static_cast<int>(tool.kind));
            toolDiameter_->setValue(tool.diameter_mm);
            toolFlutes_->setValue(tool.flutes);
            toolCutLength_->setValue(tool.cutting_length_mm);
            toolSurfaceSpeed_->setValue(tool.surface_speed_m_min);
            toolChipLoad_->setValue(tool.feed_per_tooth_mm);
            const auto cutting = document_.calc_feed_speed(selection.index);
            selectionSummary_->setText(tr("Tool %1 · %2 rpm · %3 mm/min")
                .arg(selection.index + 1).arg(cutting.rpm, 0, 'f', 0).arg(cutting.feed_mm_min, 0, 'f', 0));
            break;
        }
        case SelectionKind::Boundary: {
            if (selection.index >= document_.boundary_count()) return;
            const MachiningBoundary& boundary = document_.boundary(selection.index);
            boundaryName_->setText(QString::fromStdString(boundary.name));
            boundaryDetails_->setText(tr("Strictly convex planar polygon with %1 vertices. Geometry editing is not yet available in the viewport.")
                                      .arg(boundary.polygon.size()));
            selectionSummary_->setText(tr("Selected machining boundary"));
            break;
        }
        case SelectionKind::Operation: {
            if (selection.index >= document_.toolpath_count()) return;
            const Toolpath& path = document_.toolpath(selection.index);
            operationName_->setText(QString::fromStdString(path.name));
            operationStrategy_->setCurrentIndex(operationStrategy_->findData(static_cast<int>(path.params.strategy)));
            operationTool_->clear();
            for (std::size_t i = 0; i < document_.tool_count(); ++i) {
                operationTool_->addItem(QString::fromStdString(document_.tool(i).name), static_cast<qulonglong>(i));
            }
            operationTool_->setCurrentIndex(operationTool_->findData(static_cast<qulonglong>(path.params.tool_index)));
            operationBoundary_->clear();
            operationBoundary_->addItem(tr("Stock outline"), -1);
            for (std::size_t i = 0; i < document_.boundary_count(); ++i) {
                operationBoundary_->addItem(QString::fromStdString(document_.boundary(i).name), static_cast<int>(i));
            }
            operationBoundary_->setCurrentIndex(operationBoundary_->findData(
                path.params.boundary_index ? static_cast<int>(*path.params.boundary_index) : -1));
            operationStepover_->setValue(path.params.stepover_mm);
            operationStepdown_->setValue(path.params.stepdown_mm);
            operationTop_->setValue(path.params.z_top_mm);
            operationBottom_->setValue(path.params.z_bottom_mm);
            operationSafe_->setValue(path.params.safe_z_mm);
            operationTolerance_->setValue(path.params.tolerance_mm);
            operationClimb_->setChecked(path.params.climb_cut);
            selectionSummary_->setText(tr("%1 · %2 moves · %3 mm cut · %4 min")
                .arg(strategyName(path.params.strategy)).arg(path.moves.size())
                .arg(path.total_cut_length_mm(), 0, 'f', 1).arg(path.estimated_time_min(), 0, 'f', 2));
            break;
        }
        case SelectionKind::None:
            selectionSummary_->setText(tr("Select stock, a tool, boundary, or operation."));
            break;
    }
    applyButton_->setEnabled(selection.kind != SelectionKind::None);
}

void MillMainWindow::markChanged() {
    dirty_ = true;
    updateWindowState();
}

void MillMainWindow::updateWindowState() {
    const QString fileName = currentPath_.isEmpty() ? tr("Untitled.mymill")
                                                    : QFileInfo(currentPath_).fileName();
    setWindowTitle(tr("%1%2 — MyDesk Mill").arg(fileName, dirty_ ? QStringLiteral(" *") : QString()));
    undoAction_->setEnabled(document_.can_undo());
    redoAction_->setEnabled(document_.can_redo());
}

bool MillMainWindow::confirmDiscard() {
    if (!dirty_) return true;
    const QMessageBox::StandardButton choice = QMessageBox::warning(
        this, tr("Unsaved CAM project"), tr("Save changes before continuing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Cancel) return false;
    if (choice == QMessageBox::Save) return saveCurrent();
    return true;
}

bool MillMainWindow::saveDocument(const QString& path) {
    std::string error;
    if (!document_.save_native_file(nativePath(path), &error)) {
        setError(QString::fromStdString(error));
        return false;
    }
    currentPath_ = path;
    dirty_ = false;
    updateWindowState();
    statusBar()->showMessage(tr("Saved %1").arg(QFileInfo(path).fileName()), 4000);
    return true;
}

bool MillMainWindow::openDocument(const QString& path) {
    std::string error;
    MillDocument loaded = MillDocument::load_native_file(nativePath(path), &error);
    if (!error.empty()) {
        setError(QString::fromStdString(error));
        return false;
    }
    document_ = std::move(loaded);
    currentPath_ = path;
    dirty_ = false;
    viewport_->setDocument(&document_);
    rebuildTree(Selection{SelectionKind::Stock, 0});
    statusBar()->showMessage(tr("Opened %1").arg(QFileInfo(path).fileName()), 4000);
    return true;
}

bool MillMainWindow::saveCurrent() {
    return currentPath_.isEmpty() ? saveAs() : saveDocument(currentPath_);
}

bool MillMainWindow::saveAs() {
    QString path = QFileDialog::getSaveFileName(this, tr("Save MyDesk Mill Project"), currentPath_,
                                                tr("MyDesk Mill Project (*.mymill)"));
    if (path.isEmpty()) return false;
    if (!path.endsWith(".mymill", Qt::CaseInsensitive)) path += ".mymill";
    return saveDocument(path);
}

void MillMainWindow::openInteractive() {
    if (!confirmDiscard()) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open MyDesk Mill Project"), QString(),
                                                      tr("MyDesk Mill Project (*.mymill)"));
    if (!path.isEmpty()) openDocument(path);
}

bool MillMainWindow::exportGcode(const QString& path) {
    try {
        if (document_.toolpath_count() == 0) throw std::runtime_error("No calculated toolpaths to export");
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            throw std::runtime_error("Cannot open G-code destination");
        }
        const QByteArray data = QByteArray::fromStdString(document_.export_gcode());
        if (file.write(data) != data.size() || !file.commit()) {
            throw std::runtime_error("Failed to commit G-code output");
        }
        statusBar()->showMessage(tr("Exported generic Fanuc-style metric G-code"), 5000);
        return true;
    } catch (const std::exception& error) {
        setError(QString::fromUtf8(error.what()));
        return false;
    }
}

void MillMainWindow::exportInteractive() {
    QString path = QFileDialog::getSaveFileName(this, tr("Export Generic G-code"), QString(),
                                                tr("CNC program (*.nc *.tap *.gcode)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += ".nc";
    exportGcode(path);
}

void MillMainWindow::setError(const QString& message) {
    statusBar()->showMessage(message, 8000);
    QMessageBox::critical(this, tr("MyDesk Mill"), message);
}

void MillMainWindow::closeEvent(QCloseEvent* event) {
    if (!confirmDiscard()) {
        event->ignore();
        return;
    }
    QSettings settings;
    settings.setValue("mill/mainGeometry", saveGeometry());
    settings.setValue("mill/mainState", saveState());
    event->accept();
}
