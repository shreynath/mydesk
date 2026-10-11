#pragma once

#include "mydesk/commands/session.hpp"

#include <QWidget>

namespace mydesk::draft_lt {

class CanvasWidget final : public QWidget {
    Q_OBJECT
public:
    explicit CanvasWidget(commands::Session& session, QWidget* parent = nullptr);
    void fitToDrawing();

signals:
    void pointCommitted(double x, double y);
    void selectionChanged();
    void cursorMoved(double x, double y);

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] QPointF toScreen(geometry::Vec2 point) const;
    [[nodiscard]] geometry::Vec2 toWorld(QPointF point) const;
    void drawEntity(QPainter& painter, const document::Entity& entity, const QPen& pen) const;

    commands::Session& session_;
    double scale_{4.0};
    geometry::Vec2 pan_{380.0, 340.0};
    bool panning_{false};
    bool selecting_{false};
    QPoint mouseAnchor_;
    QPoint selectionAnchor_;
    QPoint selectionCurrent_;
    geometry::Vec2 cursorWorld_{};
};

} // namespace mydesk::draft_lt
