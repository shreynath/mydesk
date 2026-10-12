// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "infra_viewport.hpp"
#include "mydesk/infra/infra_document.hpp"
#include <QMainWindow>
#include <cstdint>

class QAction; class QCloseEvent; class QComboBox; class QDoubleSpinBox; class QLabel;
class QLineEdit; class QSpinBox; class QStackedWidget; class QTreeWidget;

class InfraMainWindow final : public QMainWindow {
    Q_OBJECT
public:
    enum class Kind { None, Road, Bridge, Building, Terrain };
    explicit InfraMainWindow(QWidget* parent=nullptr);
    const mydesk::infra::InfraDocument& document() const{return document_;}
    bool openDocument(const QString& path); bool saveDocument(const QString& path);
    bool exportGeoJson(const QString& path) const;
    void selectObject(Kind kind,std::uint64_t id);
public slots:
    void newDocument(); void createTerrain(); void createRoad(); void createBridge(); void createBuilding();
    void applyProperties(); void analyzeEarthworks(); void undo(); void redo();
protected: void closeEvent(QCloseEvent*) override;
private:
    void buildUi(); void buildMenus(); void rebuildTree(); void showSelection(); void changed();
    bool confirmDiscard(); bool saveCurrent(); bool saveAs(); void openInteractive(); void exportInteractive();
    void error(const QString&) const;
    mydesk::infra::InfraDocument document_; QString path_; bool dirty_{false}; Kind kind_{Kind::None}; std::uint64_t id_{0};
    QTreeWidget* tree_{nullptr}; InfraViewport* viewport_{nullptr}; QStackedWidget* stack_{nullptr}; QLabel* summary_{nullptr};
    QLineEdit* projectName_{nullptr}; QLineEdit* crs_{nullptr}; QLabel* terrainInfo_{nullptr};
    QLineEdit* roadName_{nullptr}; QSpinBox* roadLanes_{nullptr}; QDoubleSpinBox* roadWidth_{nullptr}; QDoubleSpinBox* roadSpeed_{nullptr}; QDoubleSpinBox* analysisInterval_{nullptr}; QLabel* roadInfo_{nullptr}; QLabel* earthwork_{nullptr};
    QLineEdit* bridgeName_{nullptr}; QDoubleSpinBox* bridgeStart_{nullptr}; QDoubleSpinBox* bridgeEnd_{nullptr}; QSpinBox* bridgePiers_{nullptr}; QDoubleSpinBox* bridgeThickness_{nullptr};
    QLineEdit* buildingName_{nullptr}; QComboBox* buildingUsage_{nullptr}; QDoubleSpinBox* buildingHeight_{nullptr}; QLabel* buildingInfo_{nullptr};
    QAction* saveAction_{nullptr}; QAction* undoAction_{nullptr}; QAction* redoAction_{nullptr};
};
