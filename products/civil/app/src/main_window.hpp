// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "civil_viewport.hpp"
#include "mydesk/civil/civil_document.hpp"

#include <QMainWindow>

#include <cstdint>
#include <string>
#include <vector>

class QAction;
class QCheckBox;
class QCloseEvent;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QTreeWidget;

class CivilMainWindow final : public QMainWindow {
    Q_OBJECT
public:
    enum class SelectionKind { None, Point, Surface, Alignment, Profile };

    explicit CivilMainWindow(QWidget* parent = nullptr);
    [[nodiscard]] const mydesk::civil::CivilDocument& document() const { return document_; }

    bool openDocument(const QString& path);
    bool saveDocument(const QString& path);
    bool exportLandXml(const QString& path) const;
    mydesk::civil::PointId createPoint(std::string number, double easting, double northing,
        double elevation, std::string description);
    mydesk::civil::SurfaceId createSurface(std::string name);
    mydesk::civil::AlignmentId createAlignment(std::string name);
    mydesk::civil::ProfileId createProfile(std::string name);
    void selectObject(SelectionKind kind, std::uint64_t id);

public slots:
    void newDocument();
    void createPointInteractive();
    void createSurfaceInteractive();
    void createAlignmentInteractive();
    void createProfileInteractive();
    void applyProperties();
    void analyzeSurface();
    void inquireStationOffset();
    void undo();
    void redo();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void buildMenus();
    void rebuildTree();
    void showSelection();
    void pushUndo();
    void restoreSnapshot(const std::string& snapshot);
    void markChanged();
    void updateActions();
    bool confirmDiscard();
    bool saveCurrent();
    bool saveAs();
    void openInteractive();
    void exportInteractive();
    void showError(const QString& message) const;

    mydesk::civil::CivilDocument document_;
    QString currentPath_;
    bool dirty_{false};
    SelectionKind selectionKind_{SelectionKind::None};
    std::uint64_t selectionId_{0};
    std::vector<std::string> undoStack_;
    std::vector<std::string> redoStack_;

    QTreeWidget* projectTree_{nullptr};
    CivilViewport* viewport_{nullptr};
    QStackedWidget* propertyStack_{nullptr};
    QLabel* selectionSummary_{nullptr};

    QLineEdit* pointNumber_{nullptr};
    QLineEdit* pointDescription_{nullptr};
    QDoubleSpinBox* pointEasting_{nullptr};
    QDoubleSpinBox* pointNorthing_{nullptr};
    QDoubleSpinBox* pointElevation_{nullptr};

    QLineEdit* surfaceName_{nullptr};
    QLabel* surfaceStatistics_{nullptr};
    QDoubleSpinBox* surfaceDatum_{nullptr};
    QDoubleSpinBox* contourInterval_{nullptr};
    QCheckBox* contourVisible_{nullptr};
    QLabel* earthworkResult_{nullptr};

    QLineEdit* alignmentName_{nullptr};
    QDoubleSpinBox* alignmentStartStation_{nullptr};
    QLabel* alignmentStatistics_{nullptr};
    QDoubleSpinBox* inquiryEasting_{nullptr};
    QDoubleSpinBox* inquiryNorthing_{nullptr};
    QLabel* inquiryResult_{nullptr};

    QLineEdit* profileName_{nullptr};
    QLabel* profileStatistics_{nullptr};

    QAction* saveAction_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
};
