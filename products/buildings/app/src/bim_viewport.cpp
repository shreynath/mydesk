#include "mydesk/buildings/bim_viewport.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mydesk::buildings {

BimViewport::BimViewport(DocumentController& controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
    setObjectName("bimViewport");
    setMinimumSize(640, 480);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

void BimViewport::setTool(Tool tool) {
    tool_ = tool;
    wallStart_.reset();
    setCursor(tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
    update();
}

void BimViewport::setAxonometric(bool enabled) {
    axonometric_ = enabled;
    wallStart_.reset();
    update();
}

QPointF BimViewport::modelToScreen(double x, double y, double z) const {
    if (axonometric_) {
        const double projectedX = (x - y) * 0.866;
        const double projectedY = (x + y) * 0.5 - z;
        return {width() * 0.5 + origin_.x() + projectedX * scale_,
                height() * 0.62 + origin_.y() - projectedY * scale_};
    }
    return {width() * 0.5 + origin_.x() + x * scale_,
            height() * 0.5 + origin_.y() - y * scale_};
}

QPointF BimViewport::screenToModel(const QPointF& point) const {
    if (axonometric_) {
        const double px = (point.x() - width() * 0.5 - origin_.x()) / scale_;
        const double py = -(point.y() - height() * 0.62 - origin_.y()) / scale_;
        return {(px / 0.866 + py * 2.0) * 0.5,
                (py * 2.0 - px / 0.866) * 0.5};
    }
    return {(point.x() - width() * 0.5 - origin_.x()) / scale_,
            -(point.y() - height() * 0.5 - origin_.y()) / scale_};
}

std::pair<ElementId, double> BimViewport::nearestWall(const QPointF& point) const {
    ElementId bestId = 0;
    double bestOffset = 0.0;
    double bestDistance = std::numeric_limits<double>::max();
    for (const auto& wall : controller_.document().walls) {
        const double dx = wall.x1 - wall.x0;
        const double dy = wall.y1 - wall.y0;
        const double lengthSquared = dx * dx + dy * dy;
        if (lengthSquared < 1.0e-12) continue;
        const double offset = std::clamp(((point.x() - wall.x0) * dx +
                                          (point.y() - wall.y0) * dy) / lengthSquared,
                                         0.0, 1.0);
        const double x = wall.x0 + dx * offset;
        const double y = wall.y0 + dy * offset;
        const double distance = std::hypot(point.x() - x, point.y() - y);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestId = wall.id;
            bestOffset = offset;
        }
    }
    if (bestDistance > 12.0 / scale_) return {0, 0.0};
    return {bestId, bestOffset};
}

Selection BimViewport::hitTest(const QPointF& screenPoint) const {
    const auto& document = controller_.document();
    for (const auto& door : document.doors) {
        if (const auto p = document.elementPosition(door.id);
            p && QLineF(screenPoint, modelToScreen(p->x, p->y, p->z)).length() < 10.0)
            return {SelectionKind::Door, door.id};
    }
    for (const auto& window : document.windows) {
        if (const auto p = document.elementPosition(window.id);
            p && QLineF(screenPoint, modelToScreen(p->x, p->y, p->z)).length() < 10.0)
            return {SelectionKind::Window, window.id};
    }
    double closest = 9.0;
    ElementId closestWall = 0;
    for (const auto& wall : document.walls) {
        const auto a = modelToScreen(wall.x0, wall.y0, document.wallBaseElevation(wall));
        const auto b = modelToScreen(wall.x1, wall.y1, document.wallBaseElevation(wall));
        QLineF segment(a, b);
        if (segment.length() < 1.0) continue;
        const QPointF delta = b - a;
        const double t = std::clamp(QPointF::dotProduct(screenPoint - a, delta) /
                                    QPointF::dotProduct(delta, delta), 0.0, 1.0);
        const double distance = QLineF(screenPoint, a + delta * t).length();
        if (distance < closest) { closest = distance; closestWall = wall.id; }
    }
    return closestWall ? Selection{SelectionKind::Wall, closestWall} : Selection{};
}

void BimViewport::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#20252b"));

    painter.setPen(QPen(QColor("#323941"), 1.0));
    const int grid = std::max(10, static_cast<int>(scale_));
    for (int x = static_cast<int>(std::fmod(width() * 0.5 + origin_.x(), grid)); x < width(); x += grid)
        painter.drawLine(x, 0, x, height());
    for (int y = static_cast<int>(std::fmod(height() * 0.5 + origin_.y(), grid)); y < height(); y += grid)
        painter.drawLine(0, y, width(), y);

    const auto& document = controller_.document();
    const auto selected = controller_.selection();
    if (axonometric_) {
        for (const auto& wall : document.walls) {
            QPainterPath face;
            face.moveTo(modelToScreen(wall.x0, wall.y0, document.wallBaseElevation(wall)));
            face.lineTo(modelToScreen(wall.x1, wall.y1, document.wallBaseElevation(wall)));
            face.lineTo(modelToScreen(wall.x1, wall.y1, document.wallTopElevation(wall)));
            face.lineTo(modelToScreen(wall.x0, wall.y0, document.wallTopElevation(wall)));
            face.closeSubpath();
            painter.setPen(QPen(selected.kind == SelectionKind::Wall && selected.id == wall.id
                                    ? QColor("#ffb74d") : QColor("#b7c6d8"), 1.5));
            painter.setBrush(QColor(92, 112, 132, 175));
            painter.drawPath(face);
        }
    } else {
        for (const auto& room : document.rooms) {
            QPolygonF polygon;
            for (const auto& [x, y] : room.boundary) polygon << modelToScreen(x, y);
            painter.setPen(QPen(QColor("#547e8d"), 1.0, Qt::DashLine));
            painter.setBrush(QColor(55, 111, 126, 45));
            painter.drawPolygon(polygon);
        }
        for (const auto& wall : document.walls) {
            const auto type = document.findWallType(wall.typeId);
            const double width = std::max(3.0, (type ? type->thickness : 0.2) * scale_);
            painter.setPen(QPen(selected.kind == SelectionKind::Wall && selected.id == wall.id
                                    ? QColor("#ffb74d") : QColor("#d9e1e8"), width, Qt::SolidLine,
                                Qt::SquareCap));
            painter.drawLine(modelToScreen(wall.x0, wall.y0), modelToScreen(wall.x1, wall.y1));
        }
    }

    for (const auto& door : document.doors) {
        const auto p = document.elementPosition(door.id);
        if (!p) continue;
        painter.setPen(QPen(selected.kind == SelectionKind::Door && selected.id == door.id
                                ? QColor("#ffb74d") : QColor("#5ee6a8"), 3.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(modelToScreen(p->x, p->y, p->z), 6, 6);
    }
    for (const auto& window : document.windows) {
        const auto p = document.elementPosition(window.id);
        if (!p) continue;
        painter.setPen(QPen(selected.kind == SelectionKind::Window && selected.id == window.id
                                ? QColor("#ffb74d") : QColor("#64b5f6"), 3.0));
        painter.drawRect(QRectF(modelToScreen(p->x, p->y, p->z) - QPointF(5, 5), QSizeF(10, 10)));
    }

    painter.setPen(QColor("#d6dde5"));
    painter.drawText(16, 24, axonometric_ ? "Axonometric BIM view" : "Level plan");
    if (wallStart_) painter.drawText(16, 44, "Choose wall endpoint");
}

void BimViewport::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    const QPointF modelPoint = screenToModel(event->position());
    if (tool_ == Tool::Select) {
        controller_.select(hitTest(event->position()));
        if (selectionChanged) selectionChanged();
    } else if (tool_ == Tool::Wall) {
        if (!wallStart_) {
            wallStart_ = modelPoint;
        } else {
            const auto selected = controller_.selection();
            const auto level = selected.kind == SelectionKind::Level
                ? static_cast<LevelId>(selected.id)
                : controller_.document().levels.front().id;
            if (controller_.createWall(level, wallStart_->x(), wallStart_->y(),
                                       modelPoint.x(), modelPoint.y()) != 0 && modelChanged)
                modelChanged();
            wallStart_.reset();
        }
    } else {
        const auto [wall, offset] = nearestWall(modelPoint);
        ElementId created = 0;
        if (wall && tool_ == Tool::Door) created = controller_.createDoor(wall, offset);
        if (wall && tool_ == Tool::Window) created = controller_.createWindow(wall, offset);
        if (created && modelChanged) modelChanged();
    }
    update();
}

void BimViewport::wheelEvent(QWheelEvent* event) {
    scale_ = std::clamp(scale_ * (event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15), 10.0, 220.0);
    update();
}

} // namespace mydesk::buildings
