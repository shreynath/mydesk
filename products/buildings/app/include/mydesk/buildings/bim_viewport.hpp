#pragma once

#include "mydesk/buildings/document_controller.hpp"

#include <QPointF>
#include <QWidget>

#include <functional>
#include <optional>

namespace mydesk::buildings {

class BimViewport final : public QWidget {
public:
    enum class Tool { Select, Wall, Door, Window };

    explicit BimViewport(DocumentController& controller, QWidget* parent = nullptr);
    void setTool(Tool tool);
    void setAxonometric(bool enabled);
    [[nodiscard]] Tool tool() const { return tool_; }

    std::function<void()> modelChanged;
    std::function<void()> selectionChanged;

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    DocumentController& controller_;
    Tool tool_{Tool::Select};
    bool axonometric_{false};
    double scale_{45.0};
    QPointF origin_{0.0, 0.0};
    std::optional<QPointF> wallStart_;

    [[nodiscard]] QPointF modelToScreen(double x, double y, double z = 0.0) const;
    [[nodiscard]] QPointF screenToModel(const QPointF& point) const;
    [[nodiscard]] std::pair<ElementId, double> nearestWall(const QPointF& modelPoint) const;
    [[nodiscard]] Selection hitTest(const QPointF& screenPoint) const;
};

} // namespace mydesk::buildings
