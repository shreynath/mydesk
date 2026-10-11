// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "mydesk/surface/surface_document.hpp"
#include "surface_viewport.hpp"

#include <QMainWindow>

class QAction;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

class SurfaceMainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit SurfaceMainWindow(QWidget* parent = nullptr);
    [[nodiscard]] const mydesk::surface::SurfaceDocument& document() const { return document_; }
    bool saveDocument(const QString& path);
    bool openDocument(const QString& path);
    void selectSurface(mydesk::surface::SurfacePatchId id, int controlPoint = -1);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void newDocument();
    void openDialog();
    void save();
    void saveAs();
    void exportIgesPreview();
    void createCurve();
    void createBilinearPatch();
    void createBicubicPatch();
    void undo();
    void redo();
    void treeSelectionChanged();
    void viewportSelectionChanged(SurfaceViewport::EntityKind kind, std::uint64_t id, int controlPoint);
    void applyProperties();
    void inspectSeam();
    void alignSeam();

private:
    enum class ItemKind { Root, Curve, Surface };
    void buildInterface();
    void rebuildBrowser();
    void rebuildSeamSelectors();
    void refreshSelection();
    void refreshActions();
    void setModified(bool modified);
    bool maybeSave();
    QString displayName() const;

    mydesk::surface::SurfaceDocument document_;
    QString filePath_;
    bool modified_{false};
    SurfaceViewport* viewport_{nullptr};
    QTreeWidget* browser_{nullptr};
    QLineEdit* entityName_{nullptr};
    QLabel* entityType_{nullptr};
    QLabel* degreeLabel_{nullptr};
    QLabel* diagnosticLabel_{nullptr};
    QComboBox* controlPointCombo_{nullptr};
    QDoubleSpinBox* xSpin_{nullptr};
    QDoubleSpinBox* ySpin_{nullptr};
    QDoubleSpinBox* zSpin_{nullptr};
    QDoubleSpinBox* weightSpin_{nullptr};
    QComboBox* seamA_{nullptr};
    QComboBox* seamB_{nullptr};
    QComboBox* continuityOrder_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
    SurfaceViewport::EntityKind selectedKind_{SurfaceViewport::EntityKind::None};
    std::uint64_t selectedId_{0};
    int selectedControlPoint_{-1};
};
