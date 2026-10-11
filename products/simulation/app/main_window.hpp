// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "mydesk/simulation/simulation_document.hpp"
#include "simulation_viewport.hpp"

#include <QMainWindow>

class QAction;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class QTreeWidget;

class SimulationMainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit SimulationMainWindow(QWidget* parent=nullptr);
    const mydesk::simulation::SimulationDocument& document() const{return document_;}
    const mydesk::simulation::SimulationResult& staticResult() const{return staticResult_;}
    bool saveDocument(const QString& path);
    bool openDocument(const QString& path);
    bool exportStaticReport(const QString& path);

public slots:
    void loadCantilever();
    void addNode();
    void solveStatic();
    void solveModal();
    void undo();
    void redo();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    enum class Kind {None,Material,Node,Element,Constraint,Load};
    struct Selection{Kind kind{Kind::None};std::size_t index{0};};
    void buildUi();
    void rebuildTree();
    void select(Selection selection);
    void refreshProperties();
    void refreshResults();
    void invalidateResults();
    void applyProperties();
    void addMaterial();
    void addElement();
    void fixNode();
    void loadNode();
    void newDocument();
    void openDialog();
    void save();
    void saveAs();
    void exportReportDialog();
    bool maybeSave();
    void setDirty(bool dirty);
    template<class Action> bool transact(const std::string& label,Action&& action);

    mydesk::simulation::SimulationDocument document_;
    mydesk::simulation::SimulationResult staticResult_;
    mydesk::simulation::ModalResult modalResult_;
    QString filePath_;
    bool dirty_{false};
    Selection selection_;
    SimulationViewport* viewport_{nullptr};
    QTreeWidget* tree_{nullptr};
    QLabel* selectionLabel_{nullptr};
    QLineEdit* nameEdit_{nullptr};
    QDoubleSpinBox* valueA_{nullptr};
    QDoubleSpinBox* valueB_{nullptr};
    QDoubleSpinBox* valueC_{nullptr};
    QDoubleSpinBox* valueD_{nullptr};
    QComboBox* displayMode_{nullptr};
    QLabel* resultSummary_{nullptr};
    QTableWidget* resultTable_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
    QAction* exportAction_{nullptr};
};
