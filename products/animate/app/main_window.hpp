#pragma once

#include "animate_viewport.hpp"

#include <QMainWindow>

#include <array>
#include <filesystem>

class QAction;
class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class QSpinBox;
class QTreeWidget;
class QTimer;

namespace mydesk::animate::ui {

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    [[nodiscard]] const AnimateDocument& document() const { return document_; }

protected:
    void closeEvent(QCloseEvent*) override;

private:
    AnimateDocument document_;
    std::filesystem::path path_;
    bool dirty_{false};
    NodeId selected_{kInvalidNode};
    double frame_{0.0};
    AnimateViewport* viewport_{nullptr};
    QTreeWidget* outliner_{nullptr};
    QLineEdit* name_{nullptr};
    std::array<QDoubleSpinBox*,3> translate_{};
    std::array<QDoubleSpinBox*,3> rotate_{};
    std::array<QDoubleSpinBox*,3> scale_{};
    QSlider* timeline_{nullptr};
    QSpinBox* frameSpin_{nullptr};
    QComboBox* channel_{nullptr};
    QDoubleSpinBox* keyValue_{nullptr};
    QListWidget* keys_{nullptr};
    QTimer* playback_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};

    void buildUi();
    void refresh();
    void refreshOutliner();
    void refreshProperties();
    void refreshKeys();
    void select(NodeId id);
    void createBox();
    void createSphere();
    void applyTransform();
    void setKey();
    void deleteSelection();
    void setFrame(double frame);
    void togglePlayback();
    bool maybeSave();
    bool save(bool choosePath = false);
    void open();
    void exportUsd();
    void markDirty();
};

} // namespace mydesk::animate::ui
