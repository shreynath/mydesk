#pragma once

#include "mydesk/coordinate/coordinate_document.hpp"

#include <QMainWindow>

#include <array>

class QAction;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class CoordinationViewport;

class CoordinateMainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit CoordinateMainWindow(QWidget* parent = nullptr);
    ~CoordinateMainWindow() override;

    mydesk::coordinate::CoordinateDocument& document() { return document_; }
    const mydesk::coordinate::CoordinateDocument& document() const { return document_; }
    void replaceDocument(mydesk::coordinate::CoordinateDocument document);
    void refreshUi();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void newSession();
    void openSession();
    bool saveSession();
    bool saveSessionAs();
    void exportBcf();
    void exportCsvReport();
    void addFederatedModel();
    void addBoxElement();
    void applyElementProperties();
    void createClashTest();
    void runSelectedClashTest();
    void applyReview();
    void undo();
    void redo();
    void treeSelectionChanged();
    void viewportSelectionChanged(qulonglong id);
    void clashTestChanged(int index);
    void resultSelectionChanged();

private:
    void buildInterface();
    void buildMenus();
    void refreshModelTree();
    void refreshModelSelectors();
    void refreshClashTests();
    void refreshResults();
    void showElementProperties(mydesk::coordinate::ElementId id);
    void setDirty(bool dirty);
    [[nodiscard]] bool confirmDiscardChanges();
    [[nodiscard]] bool writeSession(const QString& path);
    [[nodiscard]] bool writeTextExport(const QString& path, const QByteArray& bytes,
                                       const QString& description);
    [[nodiscard]] mydesk::coordinate::ClashTestId currentTestId() const;
    [[nodiscard]] mydesk::coordinate::ClashResultId currentResultId() const;
    [[nodiscard]] mydesk::coordinate::ModelId selectedModelId() const;
    [[nodiscard]] QString displayTitle() const;
    void updateActions();

    mydesk::coordinate::CoordinateDocument document_;
    QString currentFilePath_;
    bool dirty_{false};
    mydesk::coordinate::ElementId selectedElement_{0};

    CoordinationViewport* viewport_{nullptr};
    QTreeWidget* modelTree_{nullptr};
    QComboBox* setACombo_{nullptr};
    QComboBox* setBCombo_{nullptr};
    QComboBox* clashTypeCombo_{nullptr};
    QLineEdit* clashNameEdit_{nullptr};
    QDoubleSpinBox* toleranceSpin_{nullptr};
    QDoubleSpinBox* clearanceSpin_{nullptr};
    QComboBox* clashTestCombo_{nullptr};
    QTableWidget* resultsTable_{nullptr};
    QComboBox* reviewStatusCombo_{nullptr};
    QPlainTextEdit* reviewCommentsEdit_{nullptr};
    QPushButton* reviewApplyButton_{nullptr};
    QLineEdit* elementNameEdit_{nullptr};
    QLineEdit* elementCategoryEdit_{nullptr};
    std::array<QDoubleSpinBox*, 6> boundsEdits_{};
    QPushButton* propertiesApplyButton_{nullptr};
    QAction* saveAction_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
};
