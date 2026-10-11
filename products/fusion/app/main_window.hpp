// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "fusion_viewport.hpp"

#include <QMainWindow>

class QAction;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QTreeWidget;

class FusionMainWindow final : public QMainWindow {
public:
    explicit FusionMainWindow(QWidget* parent = nullptr);

    const mydesk::fusion::FusionDocument& document() const { return document_; }
    bool saveDocument(const QString& path);
    bool openDocument(const QString& path);
    bool exportGCode(const QString& path) const;
    void createStarterPart();
    void addPocket();
    void addBoard();
    bool selectFeature(mydesk::fusion::FeatureId id);
    bool setSelectedParameter(const QString& parameter, double value);
    void undo();
    void redo();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void rebuildAll();
    void rebuildBrowser();
    void rebuildTimeline();
    void refreshProperties();
    void addSketch();
    void addExtrude();
    void addPushPull();
    void applyProperties();
    void newDocument();
    void openDialog();
    void save();
    void saveAs();
    void exportDialog();
    bool maybeSave();
    void setDirty(bool dirty);
    const mydesk::fusion::TimelineEntry* selectedEntry() const;

    mydesk::fusion::FusionDocument document_;
    mydesk::fusion::FeatureId selectedId_{0};
    QString filePath_;
    bool dirty_{false};
    FusionViewport* viewport_{nullptr};
    QTreeWidget* browser_{nullptr};
    QListWidget* timeline_{nullptr};
    QLabel* selectionSummary_{nullptr};
    QLineEdit* featureName_{nullptr};
    QCheckBox* suppressed_{nullptr};
    QDoubleSpinBox* parameterA_{nullptr};
    QDoubleSpinBox* parameterB_{nullptr};
    QDoubleSpinBox* parameterC_{nullptr};
    QDoubleSpinBox* parameterD_{nullptr};
    QLabel* parameterALabel_{nullptr};
    QLabel* parameterBLabel_{nullptr};
    QLabel* parameterCLabel_{nullptr};
    QLabel* parameterDLabel_{nullptr};
    QLabel* statusSummary_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
};
