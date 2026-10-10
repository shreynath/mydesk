#pragma once

#include "mydesk/commands/session.hpp"
#include "mydesk/draft/snaps.hpp"
#include "mydesk/geometry/vec2.hpp"

#include <QWidget>

#include <optional>

namespace mydesk::desktop {

class CanvasWidget : public QWidget {
    Q_OBJECT
public:
    explicit CanvasWidget(commands::Session& session, QWidget* parent = nullptr);

    void fitToContent();
    void refresh();

signals:
    void pointCommitted(double x, double y, bool snapped);
    void cursorMoved(double x, double y);
    void selectionChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    [[nodiscard]] QPointF worldToScreen(geometry::Vec2 world) const;
    [[nodiscard]] geometry::Vec2 screenToWorld(QPointF screen) const;
    void drawEntity(QPainter& painter, const document::Entity& entity, const QPen& pen) const;
    [[nodiscard]] geometry::Vec2 resolveWorld(QPointF screen, bool* snapped);

    commands::Session& session_;
    draft::SnapSettings snaps_{};
    double scale_{1.0};
    geometry::Vec2 pan_{0.0, 0.0};
    bool panning_{false};
    bool boxSelecting_{false};
    QPoint lastMouse_;
    QPoint boxOrigin_;
    QPoint boxCurrent_;
    std::optional<draft::SnapResult> hoverSnap_;
    document::EntityId hoverEntity_{document::kInvalidEntityId};
    geometry::Vec2 cursorWorld_{};
};

} // namespace mydesk::desktop
