// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "mydesk/studio/studio_document.hpp"

#include <QMainWindow>

#include <array>
#include <filesystem>

class QAction;
class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace mydesk::studio::ui {

class StudioViewport;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    [[nodiscard]] const StudioDocument& document() const noexcept { return document_; }

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void createPrimitive();
    void applyPrimitiveParameters();
    void applyTransform();
    void addModifier();
    void applyModifierParameter();
    void removeSelectedModifier();
    void moveModifierUp();
    void moveModifierDown();
    void sceneSelectionChanged();
    void modifierSelectionChanged();
    void modifierItemChanged();
    void deleteSelection();
    void undo();
    void redo();
    void newScene();
    void openScene();
    bool saveScene();
    bool saveSceneAs();
    void exportSelectedObject();
    void setDarkTheme(bool dark);

private:
    void buildInterface();
    void buildActions();
    void buildSceneExplorer();
    void buildCommandPanel();
    void refreshAll(bool frame = false);
    void refreshSceneExplorer();
    void refreshInspector();
    void refreshModifierStack();
    void selectObject(ObjectId object, ModifierId modifier = 0);
    void markChanged();
    void setClean();
    void updateWindowTitle();
    void updateStatus();
    bool maybeSave();
    bool openPath(const std::filesystem::path& path);
    bool savePath(const std::filesystem::path& path);
    [[nodiscard]] StudioObject* selectedObject();
    [[nodiscard]] const StudioObject* selectedObject() const;
    [[nodiscard]] Modifier* selectedModifier();
    [[nodiscard]] QString primitiveName(PrimitiveKind kind) const;
    [[nodiscard]] QString modifierName(ModifierKind kind) const;
    void showOperationError(const QString& operation, const QString& detail);

    StudioDocument document_;
    std::filesystem::path currentPath_;
    bool dirty_{false};
    bool refreshing_{false};
    ObjectId selectedObjectId_{0};
    ModifierId selectedModifierId_{0};

    StudioViewport* viewport_{nullptr};
    QTreeWidget* sceneTree_{nullptr};
    QLabel* selectionLabel_{nullptr};

    QComboBox* createKind_{nullptr};
    QLineEdit* createName_{nullptr};
    QStackedWidget* createParameters_{nullptr};
    std::array<QDoubleSpinBox*, 3> boxSize_{};
    QDoubleSpinBox* cylinderRadius_{nullptr};
    QDoubleSpinBox* cylinderHeight_{nullptr};
    QSpinBox* cylinderSides_{nullptr};
    QDoubleSpinBox* sphereRadius_{nullptr};
    QSpinBox* sphereSegments_{nullptr};

    std::array<QDoubleSpinBox*, 3> translate_{};
    std::array<QDoubleSpinBox*, 3> rotate_{};
    std::array<QDoubleSpinBox*, 3> scale_{};

    QLabel* primitiveType_{nullptr};
    std::array<QLabel*, 3> primitiveLabels_{};
    std::array<QDoubleSpinBox*, 3> primitiveValues_{};
    QPushButton* applyPrimitiveButton_{nullptr};

    QComboBox* modifierKind_{nullptr};
    QDoubleSpinBox* modifierPrimary_{nullptr};
    QDoubleSpinBox* modifierSecondary_{nullptr};
    QLabel* modifierPrimaryLabel_{nullptr};
    QLabel* modifierSecondaryLabel_{nullptr};
    QListWidget* modifierStack_{nullptr};
    QPushButton* applyModifierButton_{nullptr};

    QAction* saveAction_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
    QAction* deleteAction_{nullptr};
};

} // namespace mydesk::studio::ui
