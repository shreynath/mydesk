#pragma once

#include "mechanical_session.hpp"

#include <QMainWindow>

class QAction;
class QCloseEvent;
class QLabel;
class QTableWidget;
class QTreeWidget;

namespace mydesk::mechanical::desktop {

class ShapeViewport;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createActions();
    void createDocks();
    void refreshAll();
    void refreshBrowser();
    void refreshProperties();
    void updateActions();
    void showError(const QString& title, const QString& message);
    bool maybeSave();
    bool saveDocument(bool choosePath);
    void createBox();
    void createHole();
    void insertPart();
    void openDocument();
    void exportStep();
    void exportIges();
    void onParameterChanged(int row, int column);

    MechanicalSession session_;
    ShapeViewport* viewport_{nullptr};
    QTreeWidget* browser_{nullptr};
    QTableWidget* properties_{nullptr};
    QLabel* status_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
    QAction* boxAction_{nullptr};
    QAction* holeAction_{nullptr};
    QAction* insertAction_{nullptr};
    QAction* igesAction_{nullptr};
    bool refreshing_{false};
};

} // namespace mydesk::mechanical::desktop
