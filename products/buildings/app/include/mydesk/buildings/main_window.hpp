#pragma once

#include "mydesk/buildings/bim_viewport.hpp"

#include <QMainWindow>

class QFormLayout;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QAction;
class QWidget;
class QCloseEvent;

namespace mydesk::buildings {

class MainWindow final : public QMainWindow {
public:
    MainWindow();
    [[nodiscard]] const DocumentController& controller() const { return controller_; }

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    DocumentController controller_;
    BimViewport* viewport_{nullptr};
    QTreeWidget* browser_{nullptr};
    QWidget* properties_{nullptr};
    QFormLayout* propertyForm_{nullptr};
    QLabel* summary_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};

    void buildInterface();
    void refreshAll();
    void refreshBrowser();
    void refreshProperties();
    void refreshTitle();
    void chooseTool(BimViewport::Tool tool);
    bool confirmDiscard();
    bool saveDocument(bool forcePath = false);
    void openDocument();
    void exportIfc();
    void createLevel();
    void browserSelectionChanged();
};

} // namespace mydesk::buildings
