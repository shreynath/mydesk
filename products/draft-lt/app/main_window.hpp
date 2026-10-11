#pragma once

#include "mydesk/commands/session.hpp"

#include <QMainWindow>

#include <filesystem>

class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTreeWidget;

namespace mydesk::draft_lt {

class CanvasWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    [[nodiscard]] document::Document& document() { return session_.document(); }
    [[nodiscard]] const document::Document& document() const { return session_.document(); }
    bool executeCommand(const QString& command);
    bool saveDocument(const std::filesystem::path& path);
    bool openDocument(const std::filesystem::path& path);
    bool exportDxf(const std::filesystem::path& path);
    void applySelectionProperties(const QString& layer, const QString& linetype,
                                  double lineweightMm, bool byLayerWeight);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void buildMenus();
    void buildToolbars();
    void refreshAll();
    void refreshLayers();
    void refreshProperties();
    void refreshTitle();
    bool maybeSave();
    void newDrawing();
    void openDialog();
    void saveDialog(bool saveAs);
    void exportDialog();
    void createLayer();
    void showResult(const commands::CommandResult& result);

    commands::Session session_;
    CanvasWidget* canvas_{nullptr};
    QTreeWidget* layers_{nullptr};
    QLabel* selectionSummary_{nullptr};
    QComboBox* propertyLayer_{nullptr};
    QComboBox* propertyLinetype_{nullptr};
    QDoubleSpinBox* propertyLineweight_{nullptr};
    QComboBox* propertyWeightMode_{nullptr};
    QPlainTextEdit* commandHistory_{nullptr};
    QLineEdit* commandInput_{nullptr};
    QLabel* commandStatus_{nullptr};
    QLabel* coordinateStatus_{nullptr};
    bool refreshingLayers_{false};
};

} // namespace mydesk::draft_lt
