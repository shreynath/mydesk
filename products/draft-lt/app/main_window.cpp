// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include "canvas_widget.hpp"
#include "mydesk/exchange/dxf_writer.hpp"
#include "mydesk/exchange/native_format.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace mydesk::draft_lt {
namespace {
constexpr int kLayerIdRole = Qt::UserRole + 1;

QString firstError(const exchange::ConversionReport& report) {
    return report.errors.empty() ? QStringLiteral("Unknown conversion error")
                                 : QString::fromStdString(report.errors.front());
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    session_.document().setName("Drawing1");
    buildUi();
    buildMenus();
    buildToolbars();
    refreshAll();
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("draftLT/geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("draftLT/windowState")).toByteArray());
}

void MainWindow::buildUi() {
    setObjectName(QStringLiteral("draftLtMainWindow"));
    resize(1320, 820);
    setMinimumSize(900, 620);
    setDockNestingEnabled(true);
    setStyleSheet(QStringLiteral(
        "QMainWindow,QWidget{background:#0f172a;color:#e2e8f0;}"
        "QMenuBar,QMenu,QToolBar,QStatusBar{background:#111c31;color:#e2e8f0;}"
        "QToolBar{border-bottom:1px solid #334155;spacing:3px;padding:3px;}"
        "QToolButton{background:#1e293b;border:1px solid #334155;border-radius:3px;padding:5px 9px;}"
        "QToolButton:hover{background:#075985;border-color:#38bdf8;}"
        "QDockWidget::title{background:#1e293b;padding:6px;font-weight:600;}"
        "QTreeWidget,QPlainTextEdit,QLineEdit,QComboBox,QDoubleSpinBox{"
        "background:#111827;border:1px solid #334155;color:#e2e8f0;selection-background-color:#0369a1;}"
        "QPushButton{background:#0369a1;border:1px solid #38bdf8;border-radius:3px;padding:6px;}"
        "QPushButton:hover{background:#075985;}"));

    canvas_ = new CanvasWidget(session_, this);
    setCentralWidget(canvas_);

    auto* layerDock = new QDockWidget(QStringLiteral("Layer Manager"), this);
    layerDock->setObjectName(QStringLiteral("draftLtLayersDock"));
    auto* layerPanel = new QWidget(layerDock);
    auto* layerLayout = new QVBoxLayout(layerPanel);
    layerLayout->setContentsMargins(6, 6, 6, 6);
    auto* layerHelp = new QLabel(QStringLiteral("Double-click a row to make it current."), layerPanel);
    layerHelp->setStyleSheet(QStringLiteral("color:#94a3b8;"));
    layers_ = new QTreeWidget(layerPanel);
    layers_->setObjectName(QStringLiteral("draftLtLayerTree"));
    layers_->setColumnCount(4);
    layers_->setHeaderLabels({QStringLiteral("On"), QStringLiteral("Lock"),
                              QStringLiteral("Layer"), QStringLiteral("Style")});
    layers_->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    auto* addLayer = new QPushButton(QStringLiteral("New Layer"), layerPanel);
    addLayer->setObjectName(QStringLiteral("newLayerButton"));
    layerLayout->addWidget(layerHelp);
    layerLayout->addWidget(layers_, 1);
    layerLayout->addWidget(addLayer);
    layerDock->setWidget(layerPanel);
    addDockWidget(Qt::LeftDockWidgetArea, layerDock);

    auto* propertyDock = new QDockWidget(QStringLiteral("Selection Properties"), this);
    propertyDock->setObjectName(QStringLiteral("draftLtPropertiesDock"));
    auto* propertyPanel = new QWidget(propertyDock);
    auto* propertyLayout = new QVBoxLayout(propertyPanel);
    propertyLayout->setContentsMargins(8, 8, 8, 8);
    selectionSummary_ = new QLabel(propertyPanel);
    selectionSummary_->setObjectName(QStringLiteral("selectionSummary"));
    selectionSummary_->setWordWrap(true);
    selectionSummary_->setStyleSheet(QStringLiteral("color:#cbd5e1;padding:4px;"));
    auto* form = new QFormLayout;
    propertyLayer_ = new QComboBox(propertyPanel);
    propertyLayer_->setObjectName(QStringLiteral("propertyLayer"));
    propertyLinetype_ = new QComboBox(propertyPanel);
    propertyLinetype_->setObjectName(QStringLiteral("propertyLinetype"));
    propertyLinetype_->addItems({QStringLiteral("BYLAYER"), QStringLiteral("CONTINUOUS"),
                                 QStringLiteral("DASHED"), QStringLiteral("HIDDEN"),
                                 QStringLiteral("CENTER"), QStringLiteral("PHANTOM"),
                                 QStringLiteral("DOT")});
    propertyWeightMode_ = new QComboBox(propertyPanel);
    propertyWeightMode_->addItems({QStringLiteral("ByLayer"), QStringLiteral("Explicit")});
    propertyLineweight_ = new QDoubleSpinBox(propertyPanel);
    propertyLineweight_->setRange(0.0, 2.11);
    propertyLineweight_->setDecimals(2);
    propertyLineweight_->setSingleStep(0.05);
    propertyLineweight_->setSuffix(QStringLiteral(" mm"));
    form->addRow(QStringLiteral("Layer"), propertyLayer_);
    form->addRow(QStringLiteral("Linetype"), propertyLinetype_);
    form->addRow(QStringLiteral("Weight mode"), propertyWeightMode_);
    form->addRow(QStringLiteral("Lineweight"), propertyLineweight_);
    auto* applyProperties = new QPushButton(QStringLiteral("Apply to Selection"), propertyPanel);
    applyProperties->setObjectName(QStringLiteral("applyPropertiesButton"));
    propertyLayout->addWidget(selectionSummary_);
    propertyLayout->addLayout(form);
    propertyLayout->addWidget(applyProperties);
    propertyLayout->addStretch(1);
    propertyDock->setWidget(propertyPanel);
    addDockWidget(Qt::RightDockWidgetArea, propertyDock);

    auto* commandDock = new QDockWidget(QStringLiteral("Command Line"), this);
    commandDock->setObjectName(QStringLiteral("draftLtCommandDock"));
    auto* commandPanel = new QWidget(commandDock);
    auto* commandLayout = new QVBoxLayout(commandPanel);
    commandLayout->setContentsMargins(5, 5, 5, 5);
    commandHistory_ = new QPlainTextEdit(commandPanel);
    commandHistory_->setObjectName(QStringLiteral("commandHistory"));
    commandHistory_->setReadOnly(true);
    commandHistory_->setMaximumBlockCount(250);
    commandHistory_->setMaximumHeight(110);
    commandInput_ = new QLineEdit(commandPanel);
    commandInput_->setObjectName(QStringLiteral("commandInput"));
    commandInput_->setPlaceholderText(QStringLiteral("Type LINE, CIRCLE, PLINE, MOVE, CHPROP…"));
    commandLayout->addWidget(commandHistory_);
    commandLayout->addWidget(commandInput_);
    commandDock->setWidget(commandPanel);
    addDockWidget(Qt::BottomDockWidgetArea, commandDock);

    commandStatus_ = new QLabel(QStringLiteral("Ready"), this);
    coordinateStatus_ = new QLabel(QStringLiteral("X 0.000   Y 0.000"), this);
    statusBar()->addWidget(commandStatus_, 1);
    statusBar()->addPermanentWidget(new QLabel(QStringLiteral("2D  |  MODEL"), this));
    statusBar()->addPermanentWidget(coordinateStatus_);

    connect(commandInput_, &QLineEdit::returnPressed, this, [this] {
        const QString command = commandInput_->text();
        commandInput_->clear();
        executeCommand(command);
    });
    connect(canvas_, &CanvasWidget::pointCommitted, this, [this](double x, double y) {
        const auto result = session_.providePoint({x, y});
        showResult(result);
        refreshAll();
    });
    connect(canvas_, &CanvasWidget::selectionChanged, this, &MainWindow::refreshAll);
    connect(canvas_, &CanvasWidget::cursorMoved, this, [this](double x, double y) {
        coordinateStatus_->setText(QStringLiteral("X %1   Y %2").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3));
    });
    connect(addLayer, &QPushButton::clicked, this, &MainWindow::createLayer);
    connect(layers_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item) return;
        session_.document().setActiveLayer(item->data(0, kLayerIdRole).toULongLong());
        refreshAll();
    });
    connect(layers_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
        if (refreshingLayers_ || !item) return;
        const auto id = item->data(0, kLayerIdRole).toULongLong();
        if (column == 0) session_.document().setLayerVisible(id, item->checkState(0) == Qt::Checked);
        if (column == 1) session_.document().setLayerLocked(id, item->checkState(1) == Qt::Checked);
        canvas_->update();
        refreshTitle();
    });
    connect(applyProperties, &QPushButton::clicked, this, [this] {
        applySelectionProperties(propertyLayer_->currentText(), propertyLinetype_->currentText(),
                                 propertyLineweight_->value(),
                                 propertyWeightMode_->currentIndex() == 0);
    });
    connect(propertyWeightMode_, &QComboBox::currentIndexChanged, this, [this](int index) {
        propertyLineweight_->setEnabled(index == 1);
    });
}

void MainWindow::buildMenus() {
    auto* file = menuBar()->addMenu(QStringLiteral("&File"));
    file->addAction(QStringLiteral("&New"), QKeySequence::New, this, &MainWindow::newDrawing);
    file->addAction(QStringLiteral("&Open…"), QKeySequence::Open, this, &MainWindow::openDialog);
    file->addAction(QStringLiteral("&Save"), QKeySequence::Save, this,
                    [this] { saveDialog(false); });
    file->addAction(QStringLiteral("Save &As…"), QKeySequence::SaveAs, this,
                    [this] { saveDialog(true); });
    file->addSeparator();
    file->addAction(QStringLiteral("Export DXF…"), this, &MainWindow::exportDialog);
    file->addSeparator();
    file->addAction(QStringLiteral("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    auto* edit = menuBar()->addMenu(QStringLiteral("&Edit"));
    edit->addAction(QStringLiteral("Undo"), QKeySequence::Undo, this,
                    [this] { executeCommand(QStringLiteral("UNDO")); });
    edit->addAction(QStringLiteral("Redo"), QKeySequence::Redo, this,
                    [this] { executeCommand(QStringLiteral("REDO")); });
    edit->addAction(QStringLiteral("Erase Selection"), QKeySequence::Delete, this,
                    [this] { executeCommand(QStringLiteral("ERASE")); });

    auto* draw = menuBar()->addMenu(QStringLiteral("&Draw"));
    for (const auto& command : {"LINE", "CIRCLE", "PLINE", "RECTANGLE", "DIMLINEAR"}) {
        draw->addAction(QString::fromLatin1(command), this,
                        [this, command] { executeCommand(QString::fromLatin1(command)); });
    }
    auto* modify = menuBar()->addMenu(QStringLiteral("&Modify"));
    for (const auto& command : {"MOVE", "COPY", "ROTATE", "SCALE", "OFFSET", "TRIM"}) {
        modify->addAction(QString::fromLatin1(command), this,
                          [this, command] { executeCommand(QString::fromLatin1(command)); });
    }
    auto* view = menuBar()->addMenu(QStringLiteral("&View"));
    view->addAction(QStringLiteral("Fit Drawing"), this, [this] { canvas_->fitToDrawing(); });
}

void MainWindow::buildToolbars() {
    auto* toolbar = addToolBar(QStringLiteral("Drafting"));
    toolbar->setObjectName(QStringLiteral("draftLtDraftingToolbar"));
    for (const auto& command : {"LINE", "CIRCLE", "PLINE", "RECTANGLE", "DIMLINEAR"}) {
        toolbar->addAction(QString::fromLatin1(command), this,
                           [this, command] { executeCommand(QString::fromLatin1(command)); });
    }
    toolbar->addSeparator();
    for (const auto& command : {"MOVE", "COPY", "ERASE"}) {
        toolbar->addAction(QString::fromLatin1(command), this,
                           [this, command] { executeCommand(QString::fromLatin1(command)); });
    }
    toolbar->addSeparator();
    toolbar->addAction(QStringLiteral("FIT"), this, [this] { canvas_->fitToDrawing(); });
}

bool MainWindow::executeCommand(const QString& command) {
    if (command.trimmed().isEmpty()) return false;
    commandHistory_->appendPlainText(QStringLiteral("> %1").arg(command));
    const auto result = session_.executeLine(command.toStdString());
    showResult(result);
    refreshAll();
    return result.ok;
}

void MainWindow::showResult(const commands::CommandResult& result) {
    const QString message = QString::fromStdString(result.message);
    commandHistory_->appendPlainText(message);
    commandStatus_->setText(message);
    commandInput_->setPlaceholderText(session_.context().prompt().empty()
                                          ? QStringLiteral("Command")
                                          : QString::fromStdString(session_.context().prompt()));
}

void MainWindow::refreshLayers() {
    refreshingLayers_ = true;
    layers_->clear();
    propertyLayer_->clear();
    for (const auto& layer : session_.document().layers()) {
        auto* item = new QTreeWidgetItem(layers_);
        item->setData(0, kLayerIdRole, QVariant::fromValue<qulonglong>(layer.id));
        item->setCheckState(0, layer.visible ? Qt::Checked : Qt::Unchecked);
        item->setCheckState(1, layer.locked ? Qt::Checked : Qt::Unchecked);
        item->setText(2, QString::fromStdString(layer.name) +
                             (layer.id == session_.document().activeLayerId()
                                  ? QStringLiteral("  CURRENT") : QString()));
        item->setText(3, QStringLiteral("%1 / %2 mm")
                             .arg(QString::fromUtf8(document::linetypeName(layer.linetype).data()))
                             .arg(layer.lineweightMm, 0, 'f', 2));
        propertyLayer_->addItem(QString::fromStdString(layer.name));
    }
    refreshingLayers_ = false;
}

void MainWindow::refreshProperties() {
    const auto selection = session_.document().selectionOrdered();
    const bool hasSelection = !selection.empty();
    propertyLayer_->setEnabled(hasSelection);
    propertyLinetype_->setEnabled(hasSelection);
    propertyWeightMode_->setEnabled(hasSelection);
    if (!hasSelection) {
        propertyLineweight_->setEnabled(false);
        selectionSummary_->setText(QStringLiteral("No selection\n%1 entities on %2 layers")
                                       .arg(session_.document().entities().size())
                                       .arg(session_.document().layers().size()));
        return;
    }
    const auto* entity = session_.document().findEntity(selection.front());
    if (!entity) return;
    const auto* layer = session_.document().findLayer(entity->layerId);
    selectionSummary_->setText(QStringLiteral("%1 selected\nPrimary: #%2  %3")
                                   .arg(selection.size()).arg(entity->id)
                                   .arg(QString::fromUtf8(document::entityKindName(entity->kind()).data())));
    if (layer) propertyLayer_->setCurrentText(QString::fromStdString(layer->name));
    propertyLinetype_->setCurrentText(
        QString::fromUtf8(document::linetypeName(entity->linetype).data()).toUpper());
    propertyWeightMode_->setCurrentIndex(entity->lineweightByLayer ? 0 : 1);
    propertyLineweight_->setValue(session_.document().resolvedLineweightMm(*entity));
    propertyLineweight_->setEnabled(!entity->lineweightByLayer);
}

void MainWindow::refreshTitle() {
    const QString file = session_.filePath()
                             ? QString::fromStdString(session_.filePath()->filename().string())
                             : QStringLiteral("Drawing1.mydesk");
    setWindowTitle(QStringLiteral("MyDesk Draft LT — %1%2")
                       .arg(file, session_.document().isDirty() ? QStringLiteral(" *") : QString()));
}

void MainWindow::refreshAll() {
    refreshLayers();
    refreshProperties();
    refreshTitle();
    canvas_->update();
}

void MainWindow::applySelectionProperties(const QString& layer, const QString& linetype,
                                          double lineweightMm, bool byLayerWeight) {
    if (session_.document().selection().empty()) return;
    executeCommand(QStringLiteral("CHPROP LAYER %1").arg(layer));
    executeCommand(QStringLiteral("CHPROP LTYPE %1").arg(linetype));
    executeCommand(byLayerWeight ? QStringLiteral("CHPROP LWEIGHT BYLAYER")
                                 : QStringLiteral("CHPROP LWEIGHT %1").arg(lineweightMm, 0, 'f', 2));
}

bool MainWindow::saveDocument(const std::filesystem::path& path) {
    const auto report = exchange::saveNative(session_.document(), path);
    if (!report.ok) return false;
    session_.setFilePath(path);
    session_.document().markClean();
    commandStatus_->setText(QStringLiteral("Saved %1 entities").arg(report.entityCount));
    refreshAll();
    return true;
}

bool MainWindow::openDocument(const std::filesystem::path& path) {
    const auto report = exchange::loadNative(session_.document(), path);
    if (!report.ok) return false;
    session_.setFilePath(path);
    session_.document().markClean();
    commandHistory_->appendPlainText(QStringLiteral("Opened %1").arg(QString::fromStdString(path.string())));
    refreshAll();
    canvas_->fitToDrawing();
    return true;
}

bool MainWindow::exportDxf(const std::filesystem::path& path) {
    const auto report = exchange::exportDxfR12(session_.document(), path);
    if (!report.ok) return false;
    commandStatus_->setText(QStringLiteral("Exported DXF: %1 entities").arg(report.entityCount));
    return true;
}

void MainWindow::newDrawing() {
    if (!maybeSave()) return;
    session_.executeLine("NEW");
    session_.setFilePath(std::nullopt);
    refreshAll();
    canvas_->fitToDrawing();
}

void MainWindow::openDialog() {
    if (!maybeSave()) return;
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open Draft LT Drawing"), QString(),
        QStringLiteral("MyDesk Draft LT (*.mydesk);;All Files (*)"));
    if (!path.isEmpty() && !openDocument(path.toStdString())) {
        QMessageBox::warning(this, QStringLiteral("Open failed"), QStringLiteral("The drawing could not be read."));
    }
}

void MainWindow::saveDialog(bool saveAs) {
    if (!saveAs && session_.filePath()) {
        if (!saveDocument(*session_.filePath()))
            QMessageBox::warning(this, QStringLiteral("Save failed"), QStringLiteral("The drawing could not be saved."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save Draft LT Drawing"), QStringLiteral("Drawing1.mydesk"),
        QStringLiteral("MyDesk Draft LT (*.mydesk)"));
    if (!path.isEmpty() && !saveDocument(path.toStdString())) {
        QMessageBox::warning(this, QStringLiteral("Save failed"), QStringLiteral("The drawing could not be saved."));
    }
}

void MainWindow::exportDialog() {
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export DXF"),
                                                       QStringLiteral("Drawing1.dxf"),
                                                       QStringLiteral("DXF R12 (*.dxf)"));
    if (!path.isEmpty()) {
        const auto report = exchange::exportDxfR12(session_.document(), path.toStdString());
        if (!report.ok) QMessageBox::warning(this, QStringLiteral("DXF export failed"), firstError(report));
        else QMessageBox::information(this, QStringLiteral("DXF export"),
                                      QStringLiteral("Exported %1 supported entities.\nDXF interoperability remains partial.")
                                          .arg(report.entityCount));
    }
}

void MainWindow::createLayer() {
    bool accepted = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("New Layer"),
                                               QStringLiteral("Layer name:"), QLineEdit::Normal,
                                               QString(), &accepted).trimmed();
    if (accepted && !name.isEmpty()) executeCommand(QStringLiteral("LAYER NEW %1").arg(name));
}

bool MainWindow::maybeSave() {
    if (!session_.document().isDirty()) return true;
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Unsaved drawing"), QStringLiteral("Save changes to this drawing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Save) {
        saveDialog(false);
        return !session_.document().isDirty();
    }
    return true;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!maybeSave()) {
        event->ignore();
        return;
    }
    QSettings settings;
    settings.setValue(QStringLiteral("draftLT/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("draftLT/windowState"), saveState());
    event->accept();
}

} // namespace mydesk::draft_lt
