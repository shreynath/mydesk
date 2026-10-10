#include "main_window.hpp"

#include "mydesk/exchange/dxf_reader.hpp"
#include "mydesk/exchange/dxf_writer.hpp"
#include "mydesk/exchange/native_format.hpp"

#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QVBoxLayout>

namespace mydesk::desktop {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    keymap_ = draft::defaultAutocadInspiredKeymap();
    keyIndex_ = draft::bindingIndex(keymap_);
    buildUi();
    buildMenus();
    refreshUi();
    setStatus(QStringLiteral("Ready — MyDesk Draft"));
}

void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("MyDesk Draft"));
    resize(1280, 800);
    setStyleSheet("QMainWindow { background: #0f141a; }"
                  "QMenuBar { background: #161d27; color: #e8eef4; }"
                  "QMenuBar::item:selected { background: #243040; }"
                  "QMenu { background: #161d27; color: #e8eef4; }"
                  "QStatusBar { background: #121820; color: #b7c6d4; }"
                  "QDockWidget { color: #e8eef4; titlebar-close-icon: url(none); }"
                  "QDockWidget::title { background: #1a2330; padding: 6px; }");

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    canvas_ = new CanvasWidget(session_, central);
    commandLine_ = new CommandLineWidget(central);
    commandLine_->setMinimumHeight(140);
    commandLine_->setMaximumHeight(180);

    layout->addWidget(canvas_, 1);
    layout->addWidget(commandLine_, 0);
    setCentralWidget(central);

    auto* layersDock = new QDockWidget(QStringLiteral("Layers"), this);
    layersPanel_ = new LayersPanel(layersDock);
    layersDock->setWidget(layersPanel_);
    addDockWidget(Qt::LeftDockWidgetArea, layersDock);

    auto* propsDock = new QDockWidget(QStringLiteral("Properties"), this);
    propertiesPanel_ = new PropertiesPanel(propsDock);
    propsDock->setWidget(propertiesPanel_);
    addDockWidget(Qt::RightDockWidgetArea, propsDock);

    statusCoords_ = new QLabel(QStringLiteral("0.000, 0.000"), this);
    statusModes_ = new QLabel(this);
    statusCommand_ = new QLabel(QStringLiteral("Ready"), this);
    statusBar()->addWidget(statusCommand_, 1);
    statusBar()->addPermanentWidget(statusModes_);
    statusBar()->addPermanentWidget(statusCoords_);

    connect(commandLine_, &CommandLineWidget::commandSubmitted, this,
            &MainWindow::onCommandSubmitted);
    connect(canvas_, &CanvasWidget::pointCommitted, this, &MainWindow::onCanvasPoint);
    connect(canvas_, &CanvasWidget::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(canvas_, &CanvasWidget::cursorMoved, this, [this](double x, double y) {
        statusCoords_->setText(QStringLiteral("%1, %2").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3));
    });
    connect(layersPanel_, &LayersPanel::layerActivated, this, [this](quint64 id) {
        session_.document().setActiveLayer(id);
        refreshUi();
    });
}

void MainWindow::buildMenus() {
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(QStringLiteral("&New"), QKeySequence::New, this, &MainWindow::newFile);
    fileMenu->addAction(QStringLiteral("&Open…"), QKeySequence::Open, this, &MainWindow::openFile);
    fileMenu->addAction(QStringLiteral("&Save"), QKeySequence::Save, this, &MainWindow::saveFile);
    fileMenu->addAction(QStringLiteral("Save &As…"), QKeySequence::SaveAs, this,
                        &MainWindow::saveFileAs);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Import DXF…"), this, &MainWindow::importDxf);
    fileMenu->addAction(QStringLiteral("Export DXF…"), this, &MainWindow::exportDxf);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
    editMenu->addAction(QStringLiteral("&Undo"), QKeySequence::Undo, this, &MainWindow::undo);
    editMenu->addAction(QStringLiteral("&Redo"), QKeySequence::Redo, this, &MainWindow::redo);
    editMenu->addAction(QStringLiteral("Cancel"), QKeySequence(Qt::Key_Escape), this,
                        &MainWindow::cancelCommand);

    auto* drawMenu = menuBar()->addMenu(QStringLiteral("&Draw"));
    drawMenu->addAction(QStringLiteral("Line"), this, [this]() { onCommandSubmitted("LINE"); });
    drawMenu->addAction(QStringLiteral("Circle"), this, [this]() { onCommandSubmitted("CIRCLE"); });
    drawMenu->addAction(QStringLiteral("Rectangle"), this,
                        [this]() { onCommandSubmitted("RECTANGLE"); });

    auto* modifyMenu = menuBar()->addMenu(QStringLiteral("&Modify"));
    modifyMenu->addAction(QStringLiteral("Move"), this, [this]() { onCommandSubmitted("MOVE"); });
    modifyMenu->addAction(QStringLiteral("Copy"), this, [this]() { onCommandSubmitted("COPY"); });
    modifyMenu->addAction(QStringLiteral("Rotate"), this, [this]() { onCommandSubmitted("ROTATE"); });
    modifyMenu->addAction(QStringLiteral("Scale"), this, [this]() { onCommandSubmitted("SCALE"); });
    modifyMenu->addAction(QStringLiteral("Trim"), this, [this]() { onCommandSubmitted("TRIM"); });
    modifyMenu->addAction(QStringLiteral("Extend"), this, [this]() { onCommandSubmitted("EXTEND"); });
    modifyMenu->addAction(QStringLiteral("Offset"), this, [this]() { onCommandSubmitted("OFFSET"); });
    modifyMenu->addAction(QStringLiteral("Fillet"), this, [this]() { onCommandSubmitted("FILLET"); });
    modifyMenu->addAction(QStringLiteral("Chamfer"), this,
                          [this]() { onCommandSubmitted("CHAMFER"); });
    modifyMenu->addAction(QStringLiteral("Erase"), this, [this]() { onCommandSubmitted("ERASE"); });
    drawMenu->addAction(QStringLiteral("Polyline"), this, [this]() { onCommandSubmitted("PLINE"); });

    auto* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    viewMenu->addAction(QStringLiteral("Fit"), this, [this]() { canvas_->fitToContent(); });
}

void MainWindow::onCommandSubmitted(const QString& text) {
    const auto trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        if (!session_.hasActiveCommand() && !session_.context().lastCommand().empty()) {
            onCommandSubmitted(QString::fromStdString(session_.context().lastCommand()));
        }
        return;
    }
    commandLine_->appendHistory(QStringLiteral("Command: %1").arg(trimmed));
    const auto result = session_.executeLine(trimmed.toStdString());
    commandLine_->appendHistory(QString::fromStdString(result.message));
    commandLine_->setPrompt(QString::fromStdString(session_.context().prompt()));
    setStatus(QString::fromStdString(result.message));
    refreshUi();
}

void MainWindow::onCanvasPoint(double x, double y, bool snapped) {
    if (!session_.hasActiveCommand()) {
        return;
    }
    const auto result = session_.providePoint({x, y});
    commandLine_->appendHistory(QStringLiteral("Point: %1,%2%3")
                                    .arg(x, 0, 'f', 4)
                                    .arg(y, 0, 'f', 4)
                                    .arg(snapped ? QStringLiteral(" (snap)") : QString()));
    commandLine_->appendHistory(QString::fromStdString(result.message));
    commandLine_->setPrompt(QString::fromStdString(session_.context().prompt()));
    setStatus(QString::fromStdString(result.message));
    refreshUi();
}

void MainWindow::onSelectionChanged() { refreshUi(); }

void MainWindow::newFile() {
    if (!maybeSave()) {
        return;
    }
    session_.executeLine("NEW");
    session_.setFilePath(std::nullopt);
    refreshUi();
    setStatus(QStringLiteral("New drawing"));
}

void MainWindow::openFile() {
    if (!maybeSave()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open MyDesk Draft drawing"), QString(),
        QStringLiteral("MyDesk Draft (*.mydesk);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    auto report = exchange::loadNative(session_.document(), path.toStdString());
    if (!report.ok) {
        QMessageBox::warning(this, QStringLiteral("Open failed"),
                             report.errors.empty()
                                 ? QStringLiteral("Unknown error")
                                 : QString::fromStdString(report.errors.front()));
        return;
    }
    session_.setFilePath(path.toStdString());
    commandLine_->appendHistory(QStringLiteral("Opened %1 (%2 entities)")
                                    .arg(path)
                                    .arg(report.entityCount));
    refreshUi();
    canvas_->fitToContent();
    setStatus(QStringLiteral("Opened %1").arg(path));
}

void MainWindow::saveFile() {
    if (!session_.filePath()) {
        saveFileAs();
        return;
    }
    writeToPath(QString::fromStdString(session_.filePath()->string()));
}

void MainWindow::saveFileAs() {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save MyDesk Draft drawing"), QStringLiteral("Drawing1.mydesk"),
        QStringLiteral("MyDesk Draft (*.mydesk)"));
    if (path.isEmpty()) {
        return;
    }
    if (writeToPath(path)) {
        session_.setFilePath(path.toStdString());
    }
}

void MainWindow::exportDxf() {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export DXF"), QStringLiteral("Drawing1.dxf"),
        QStringLiteral("DXF (*.dxf)"));
    if (path.isEmpty()) {
        return;
    }
    auto report = exchange::exportDxfR12(session_.document(), path.toStdString());
    if (!report.ok) {
        QMessageBox::warning(this, QStringLiteral("Export failed"),
                             report.errors.empty()
                                 ? QStringLiteral("Unknown error")
                                 : QString::fromStdString(report.errors.front()));
        return;
    }
    QString msg = QStringLiteral("Exported DXF (partial support — see warnings).");
    for (const auto& warning : report.warnings) {
        msg += '\n' + QString::fromStdString(warning);
    }
    QMessageBox::information(this, QStringLiteral("Export DXF"), msg);
}

void MainWindow::importDxf() {
    if (!maybeSave()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import DXF"), QString(), QStringLiteral("DXF (*.dxf);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    auto report = exchange::importDxf(session_.document(), path.toStdString());
    if (!report.ok) {
        QMessageBox::warning(this, QStringLiteral("Import failed"),
                             report.errors.empty()
                                 ? QStringLiteral("Unknown error")
                                 : QString::fromStdString(report.errors.front()));
        return;
    }
    session_.setFilePath(std::nullopt);
    QString msg = QStringLiteral("Imported %1 entities (partial DXF support).")
                      .arg(report.entityCount);
    for (const auto& warning : report.warnings) {
        msg += '\n' + QString::fromStdString(warning);
    }
    commandLine_->appendHistory(msg);
    QMessageBox::information(this, QStringLiteral("Import DXF"), msg);
    refreshUi();
    canvas_->fitToContent();
}

void MainWindow::undo() { onCommandSubmitted(QStringLiteral("UNDO")); }
void MainWindow::redo() { onCommandSubmitted(QStringLiteral("REDO")); }
void MainWindow::cancelCommand() {
    if (session_.hasActiveCommand()) {
        const auto result = session_.cancel();
        commandLine_->appendHistory(QString::fromStdString(result.message));
        commandLine_->setPrompt(QString());
        setStatus(QString::fromStdString(result.message));
    } else {
        session_.document().clearSelection();
        setStatus(QStringLiteral("Selection cleared"));
    }
    refreshUi();
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        cancelCommand();
        return;
    }

    // AutoCAD-inspired function-key toggles (public shortcut guide).
    if (event->key() == Qt::Key_F3) {
        session_.modes().objectSnap = !session_.modes().objectSnap;
        setStatus(session_.modes().objectSnap ? QStringLiteral("Object snap on (F3)")
                                              : QStringLiteral("Object snap off (F3)"));
        refreshUi();
        return;
    }
    if (event->key() == Qt::Key_F7) {
        session_.modes().gridDisplay = !session_.modes().gridDisplay;
        setStatus(session_.modes().gridDisplay ? QStringLiteral("Grid on (F7)")
                                               : QStringLiteral("Grid off (F7)"));
        refreshUi();
        return;
    }
    if (event->key() == Qt::Key_F8) {
        session_.modes().ortho = !session_.modes().ortho;
        setStatus(session_.modes().ortho ? QStringLiteral("Ortho on (F8)")
                                         : QStringLiteral("Ortho off (F8)"));
        refreshUi();
        return;
    }
    if (event->key() == Qt::Key_F9) {
        session_.modes().gridSnap = !session_.modes().gridSnap;
        setStatus(session_.modes().gridSnap ? QStringLiteral("Snap on (F9)")
                                            : QStringLiteral("Snap off (F9)"));
        refreshUi();
        return;
    }

    QString key;
    if (event->modifiers() & Qt::ControlModifier) {
        key = QStringLiteral("Ctrl+");
    }
    if (event->modifiers() & Qt::MetaModifier) {
        key = QStringLiteral("Meta+");
        if (event->modifiers() & Qt::ShiftModifier) {
            key = QStringLiteral("Meta+Shift+");
        }
    }
    if (event->key() == Qt::Key_Z) {
        key += 'Z';
    } else if (event->key() == Qt::Key_Y) {
        key += 'Y';
    } else if (event->key() >= Qt::Key_A && event->key() <= Qt::Key_Z &&
               event->modifiers() == Qt::NoModifier) {
        key = QChar(event->key());
    } else {
        QMainWindow::keyPressEvent(event);
        return;
    }

    const auto it = keyIndex_.find(key.toStdString());
    if (it != keyIndex_.end()) {
        if (it->second == "CANCEL") {
            cancelCommand();
        } else if (it->second == "REPEAT") {
            onCommandSubmitted(QString());
        } else {
            onCommandSubmitted(QString::fromStdString(it->second));
        }
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::refreshUi() {
    layersPanel_->setDocument(session_.document());
    propertiesPanel_->setDocument(session_.document());
    canvas_->refresh();
    if (statusModes_) {
        statusModes_->setText(
            QStringLiteral("OSNAP:%1  GRID:%2  ORTHO:%3  SNAP:%4")
                .arg(session_.modes().objectSnap ? QStringLiteral("On") : QStringLiteral("Off"))
                .arg(session_.modes().gridDisplay ? QStringLiteral("On") : QStringLiteral("Off"))
                .arg(session_.modes().ortho ? QStringLiteral("On") : QStringLiteral("Off"))
                .arg(session_.modes().gridSnap ? QStringLiteral("On") : QStringLiteral("Off")));
    }
    const QString title = session_.filePath()
                              ? QStringLiteral("MyDesk Draft — %1%2")
                                    .arg(QString::fromStdString(session_.filePath()->filename().string()))
                                    .arg(session_.document().isDirty() ? QStringLiteral("*")
                                                                       : QString())
                              : QStringLiteral("MyDesk Draft — Drawing1%1")
                                    .arg(session_.document().isDirty() ? QStringLiteral("*")
                                                                       : QString());
    setWindowTitle(title);
}

void MainWindow::setStatus(const QString& text) {
    statusCommand_->setText(text);
}

bool MainWindow::maybeSave() {
    if (!session_.document().isDirty()) {
        return true;
    }
    const auto answer =
        QMessageBox::question(this, QStringLiteral("Unsaved changes"),
                              QStringLiteral("Save changes before continuing?"),
                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel) {
        return false;
    }
    if (answer == QMessageBox::Save) {
        saveFile();
        return !session_.document().isDirty();
    }
    return true;
}

bool MainWindow::writeToPath(const QString& path) {
    auto report = exchange::saveNative(session_.document(), path.toStdString());
    if (!report.ok) {
        QMessageBox::warning(this, QStringLiteral("Save failed"),
                             report.errors.empty()
                                 ? QStringLiteral("Unknown error")
                                 : QString::fromStdString(report.errors.front()));
        return false;
    }
    session_.document().markClean();
    commandLine_->appendHistory(QStringLiteral("Saved %1").arg(path));
    setStatus(QStringLiteral("Saved %1").arg(path));
    refreshUi();
    return true;
}

} // namespace mydesk::desktop
