#pragma once

#include "canvas_widget.hpp"
#include "command_line_widget.hpp"
#include "layers_panel.hpp"
#include "properties_panel.hpp"

#include "mydesk/commands/session.hpp"
#include "mydesk/draft/keymap.hpp"

#include <QMainWindow>

#include <string>
#include <unordered_map>

class QLabel;

namespace mydesk::desktop {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onCommandSubmitted(const QString& text);
    void onCanvasPoint(double x, double y, bool snapped);
    void onSelectionChanged();
    void newFile();
    void openFile();
    void saveFile();
    void saveFileAs();
    void exportDxf();
    void importDxf();
    void undo();
    void redo();
    void cancelCommand();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void buildMenus();
    void refreshUi();
    void setStatus(const QString& text);
    bool maybeSave();
    bool writeToPath(const QString& path);

    commands::Session session_;
    draft::KeymapProfile keymap_;
    std::unordered_map<std::string, std::string> keyIndex_;

    CanvasWidget* canvas_{nullptr};
    CommandLineWidget* commandLine_{nullptr};
    LayersPanel* layersPanel_{nullptr};
    PropertiesPanel* propertiesPanel_{nullptr};
    QLabel* statusCoords_{nullptr};
    QLabel* statusModes_{nullptr};
    QLabel* statusCommand_{nullptr};
};

} // namespace mydesk::desktop
