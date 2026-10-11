// SPDX-License-Identifier: MPL-2.0
#include "canvas_widget.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace mydesk::draft_lt {
namespace {
constexpr double kPi = 3.14159265358979323846;

QColor colorFromArgb(std::uint32_t argb) {
    return {static_cast<int>((argb >> 16) & 0xffU), static_cast<int>((argb >> 8) & 0xffU),
            static_cast<int>(argb & 0xffU), static_cast<int>((argb >> 24) & 0xffU)};
}
} // namespace

CanvasWidget::CanvasWidget(commands::Session& session, QWidget* parent)
    : QWidget(parent), session_(session) {
    setObjectName(QStringLiteral("draftLtViewport"));
    setMinimumSize(520, 360);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

QPointF CanvasWidget::toScreen(geometry::Vec2 point) const {
    return {pan_.x + point.x * scale_, pan_.y - point.y * scale_};
}

geometry::Vec2 CanvasWidget::toWorld(QPointF point) const {
    return {(point.x() - pan_.x) / scale_, (pan_.y - point.y()) / scale_};
}

void CanvasWidget::fitToDrawing() {
    if (session_.document().entities().empty()) {
        scale_ = 4.0;
        pan_ = {width() / 2.0, height() / 2.0};
        update();
        return;
    }
    geometry::BBox bounds;
    for (const auto& entity : session_.document().entities()) {
        if (session_.document().isEntityVisible(entity)) bounds.expand(entity.bounds());
    }
    const double drawingWidth = std::max(1.0, bounds.maxX - bounds.minX);
    const double drawingHeight = std::max(1.0, bounds.maxY - bounds.minY);
    scale_ = std::clamp(std::min((width() - 80.0) / drawingWidth,
                                 (height() - 80.0) / drawingHeight), 0.02, 500.0);
    const auto center = bounds.center();
    pan_ = {width() / 2.0 - center.x * scale_, height() / 2.0 + center.y * scale_};
    update();
}

void CanvasWidget::drawEntity(QPainter& painter, const document::Entity& entity,
                              const QPen& pen) const {
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    if (const auto* line = std::get_if<document::LineEntity>(&entity.geometry)) {
        painter.drawLine(toScreen(line->start), toScreen(line->end));
    } else if (const auto* circle = std::get_if<document::CircleEntity>(&entity.geometry)) {
        const auto center = toScreen(circle->center);
        const double radius = circle->radius * scale_;
        painter.drawEllipse(center, radius, radius);
    } else if (const auto* arc = std::get_if<document::ArcEntity>(&entity.geometry)) {
        const auto center = toScreen(arc->center);
        const double radius = arc->radius * scale_;
        const QRectF rect(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
        const int start = static_cast<int>(-arc->startAngle * 180.0 / kPi * 16.0);
        double span = arc->endAngle - arc->startAngle;
        if (span <= 0.0) span += 2.0 * kPi;
        painter.drawArc(rect, start, static_cast<int>(-span * 180.0 / kPi * 16.0));
    } else if (const auto* polyline = std::get_if<document::PolylineEntity>(&entity.geometry)) {
        if (polyline->vertices.empty()) return;
        QPainterPath path(toScreen(polyline->vertices.front()));
        for (std::size_t i = 1; i < polyline->vertices.size(); ++i) {
            path.lineTo(toScreen(polyline->vertices[i]));
        }
        if (polyline->closed) path.closeSubpath();
        painter.drawPath(path);
    } else if (const auto* dimension =
                   std::get_if<document::DimensionLinearEntity>(&entity.geometry)) {
        const auto layout = document::layoutDimension(*dimension);
        painter.drawLine(toScreen(layout.start), toScreen(layout.dimStart));
        painter.drawLine(toScreen(layout.end), toScreen(layout.dimEnd));
        painter.drawLine(toScreen(layout.dimStart), toScreen(layout.dimEnd));
        painter.drawText(toScreen(layout.textPos) + QPointF(4.0, -4.0),
                         QString::number(layout.measurement, 'f', 2));
    } else if (const auto* text = std::get_if<document::TextEntity>(&entity.geometry)) {
        painter.save();
        painter.translate(toScreen(text->position));
        painter.rotate(-text->rotationDeg);
        painter.drawText(QPointF{}, QString::fromStdString(text->contents));
        painter.restore();
    } else if (const auto* hatch = std::get_if<document::HatchEntity>(&entity.geometry)) {
        if (hatch->boundary.size() < 3) return;
        QPainterPath path(toScreen(hatch->boundary.front()));
        for (std::size_t i = 1; i < hatch->boundary.size(); ++i) {
            path.lineTo(toScreen(hatch->boundary[i]));
        }
        path.closeSubpath();
        painter.fillPath(path, colorFromArgb(hatch->fillArgb));
        painter.strokePath(path, pen);
    } else {
        const auto bounds = entity.bounds();
        painter.drawRect(QRectF(toScreen({bounds.minX, bounds.maxY}),
                                toScreen({bounds.maxX, bounds.minY})).normalized());
    }
}

void CanvasWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(15, 23, 42));

    if (session_.modes().gridDisplay) {
        const double spacing = session_.modes().gridSpacing * scale_;
        if (spacing >= 7.0) {
            painter.setPen(QPen(QColor(51, 65, 85), 1.0));
            double x = std::fmod(pan_.x, spacing);
            if (x < 0.0) x += spacing;
            for (; x < width(); x += spacing) painter.drawLine(QPointF(x, 0), QPointF(x, height()));
            double y = std::fmod(pan_.y, spacing);
            if (y < 0.0) y += spacing;
            for (; y < height(); y += spacing) painter.drawLine(QPointF(0, y), QPointF(width(), y));
        }
    }
    painter.setPen(QPen(QColor(71, 85, 105), 1.0));
    painter.drawLine(QPointF(0, pan_.y), QPointF(width(), pan_.y));
    painter.drawLine(QPointF(pan_.x, 0), QPointF(pan_.x, height()));

    for (const auto& entity : session_.document().entities()) {
        if (!session_.document().isEntityVisible(entity)) continue;
        QColor color = colorFromArgb(session_.document().resolvedEntityColor(entity));
        double widthPx = std::max(1.0, session_.document().resolvedLineweightMm(entity) * 3.0);
        if (session_.document().selection().contains(entity.id)) {
            color = QColor(250, 204, 21);
            widthPx = std::max(widthPx, 2.5);
        }
        QPen pen(color, widthPx);
        const auto pattern = document::linetypeDashPattern(
            session_.document().resolvedLinetype(entity));
        if (!pattern.empty()) {
            QVector<qreal> dashes;
            for (const double length : pattern) {
                dashes.push_back(std::max(1.0, std::abs(length) * scale_ / widthPx));
            }
            pen.setDashPattern(dashes);
        }
        drawEntity(painter, entity, pen);
    }

    if (session_.hasActiveCommand() && !session_.context().points().empty()) {
        painter.setPen(QPen(QColor(56, 189, 248), 1.5, Qt::DashLine));
        painter.drawLine(toScreen(session_.context().points().back()), toScreen(cursorWorld_));
    }
    if (selecting_) {
        const bool crossing = selectionCurrent_.x() < selectionAnchor_.x();
        painter.setPen(QPen(crossing ? QColor(34, 197, 94) : QColor(59, 130, 246), 1.0));
        painter.setBrush(crossing ? QColor(34, 197, 94, 45) : QColor(59, 130, 246, 45));
        painter.drawRect(QRect(selectionAnchor_, selectionCurrent_).normalized());
    }
}

void CanvasWidget::wheelEvent(QWheelEvent* event) {
    const auto fixedWorld = toWorld(event->position());
    scale_ = std::clamp(scale_ * (event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15),
                        0.02, 500.0);
    const auto shifted = toScreen(fixedWorld);
    pan_.x += event->position().x() - shifted.x();
    pan_.y += event->position().y() - shifted.y();
    update();
}

void CanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning_ = true;
        mouseAnchor_ = event->pos();
    } else if (event->button() == Qt::LeftButton) {
        cursorWorld_ = session_.applyOrtho(toWorld(event->position()));
        if (session_.hasActiveCommand()) emit pointCommitted(cursorWorld_.x, cursorWorld_.y);
        else selecting_ = true, selectionAnchor_ = selectionCurrent_ = event->pos();
    }
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (panning_) {
        const QPoint delta = event->pos() - mouseAnchor_;
        pan_.x += delta.x();
        pan_.y += delta.y();
        mouseAnchor_ = event->pos();
    } else if (selecting_) {
        selectionCurrent_ = event->pos();
    } else {
        cursorWorld_ = session_.applyOrtho(toWorld(event->position()));
        emit cursorMoved(cursorWorld_.x, cursorWorld_.y);
    }
    update();
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning_ = false;
        return;
    }
    if (event->button() != Qt::LeftButton || !selecting_) return;
    selecting_ = false;
    const bool additive = event->modifiers().testFlag(Qt::ShiftModifier);
    if (!additive) session_.document().clearSelection();
    const QPoint delta = event->pos() - selectionAnchor_;
    if (std::hypot(delta.x(), delta.y()) < 4.0) {
        const auto id = session_.document().hitTestTop(toWorld(event->position()), 8.0 / scale_);
        if (id != document::kInvalidEntityId) {
            if (additive) session_.document().toggleSelect(id);
            else session_.document().select(id);
        }
    } else {
        const auto a = toWorld(selectionAnchor_);
        const auto b = toWorld(event->position());
        geometry::BBox rect;
        rect.minX = std::min(a.x, b.x);
        rect.maxX = std::max(a.x, b.x);
        rect.minY = std::min(a.y, b.y);
        rect.maxY = std::max(a.y, b.y);
        session_.document().selectInRect(rect, event->pos().x() < selectionAnchor_.x(), additive);
    }
    emit selectionChanged();
    update();
}

} // namespace mydesk::draft_lt
