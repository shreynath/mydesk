// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "mydesk/mill/mill_document.hpp"

#include <QMainWindow>

#include <optional>

class QAction;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;
class MillViewport;

class MillMainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MillMainWindow(QWidget* parent = nullptr);

    [[nodiscard]] const mydesk::mill::MillDocument& document() const noexcept { return document_; }
    [[nodiscard]] mydesk::mill::MillDocument& document() noexcept { return document_; }
    bool openDocument(const QString& path);
    bool saveDocument(const QString& path);
    bool exportGcode(const QString& path);

public slots:
    void newDocument();
    void createTool();
    void createBoundary();
    void createOperation();
    void applyProperties();
    void deleteSelection();
    void undo();
    void redo();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    enum class SelectionKind { None = 0, Stock = 1, Tool = 2, Boundary = 3, Operation = 4 };
    struct Selection { SelectionKind kind{SelectionKind::None}; std::size_t index{0}; };

    void buildUi();
    void buildMenus();
    void rebuildTree(std::optional<Selection> preferred = std::nullopt);
    void showSelection(Selection selection);
    void markChanged();
    void updateWindowState();
    bool confirmDiscard();
    bool saveCurrent();
    bool saveAs();
    void openInteractive();
    void exportInteractive();
    void setError(const QString& message);

    mydesk::mill::MillDocument document_;
    QString currentPath_;
    bool dirty_{false};
    Selection selection_;

    QTreeWidget* projectTree_{nullptr};
    MillViewport* viewport_{nullptr};
    QStackedWidget* propertyStack_{nullptr};
    QLabel* selectionSummary_{nullptr};

    QDoubleSpinBox* stockValues_[6]{};
    QLineEdit* toolName_{nullptr};
    QComboBox* toolKind_{nullptr};
    QDoubleSpinBox* toolDiameter_{nullptr};
    QDoubleSpinBox* toolFlutes_{nullptr};
    QDoubleSpinBox* toolCutLength_{nullptr};
    QDoubleSpinBox* toolSurfaceSpeed_{nullptr};
    QDoubleSpinBox* toolChipLoad_{nullptr};
    QLineEdit* boundaryName_{nullptr};
    QLabel* boundaryDetails_{nullptr};
    QLineEdit* operationName_{nullptr};
    QComboBox* operationStrategy_{nullptr};
    QComboBox* operationTool_{nullptr};
    QComboBox* operationBoundary_{nullptr};
    QDoubleSpinBox* operationStepover_{nullptr};
    QDoubleSpinBox* operationStepdown_{nullptr};
    QDoubleSpinBox* operationTop_{nullptr};
    QDoubleSpinBox* operationBottom_{nullptr};
    QDoubleSpinBox* operationSafe_{nullptr};
    QDoubleSpinBox* operationTolerance_{nullptr};
    QCheckBox* operationClimb_{nullptr};
    QPushButton* applyButton_{nullptr};

    QAction* saveAction_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
};
