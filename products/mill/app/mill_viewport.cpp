// SPDX-License-Identifier: MPL-2.0
#include "mill_viewport.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

using namespace mydesk::mill;

namespace {
double segmentDistance(const QPointF& point, const QPointF& a, const QPointF& b) {
    const QPointF delta = b - a;
    const double length2 = QPointF::dotProduct(delta, delta);
    if (length2 <= 1e-12) return std::hypot(point.x() - a.x(), point.y() - a.y());
    const double parameter = std::clamp(QPointF::dotProduct(point - a, delta) / length2, 0.0, 1.0);
    const QPointF closest = a + delta * parameter;
    return std::hypot(point.x() - closest.x(), point.y() - closest.y());
}
}

MillViewport::MillViewport(QWidget* parent) : QWidget(parent) {
    setObjectName("machiningViewport");
    setMinimumSize(480, 360);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAccessibleName(tr("Machining viewport"));
    setToolTip(tr("Click a toolpath to select. Drag to orbit, middle-drag to pan, and wheel to zoom."));
}

void MillViewport::setDocument(const MillDocument* document) {
    document_ = document;
    selectedToolpath_.reset();
    fitView();
}

void MillViewport::setSelectedToolpath(std::optional<std::size_t> index) {
    selectedToolpath_ = index;
    update();
}

void MillViewport::setTopView(bool topView) {
    topView_ = topView;
    update();
}

void MillViewport::fitView() {
    zoom_ = 1.0;
    pan_ = {};
    update();
}

QPointF MillViewport::project(const Vec3& input) const {
    if (!document_) return {};
    const StockBox& stock = document_->stock();
    const Vec3 center{(stock.min_corner.x + stock.max_corner.x) * 0.5,
                      (stock.min_corner.y + stock.max_corner.y) * 0.5,
                      (stock.min_corner.z + stock.max_corner.z) * 0.5};
    double x = input.x - center.x;
    double y = input.y - center.y;
    double z = input.z - center.z;
    if (!topView_) {
        const double yaw = yaw_ * 3.14159265358979323846 / 180.0;
        const double pitch = pitch_ * 3.14159265358979323846 / 180.0;
        const double x1 = std::cos(yaw) * x - std::sin(yaw) * y;
        const double y1 = std::sin(yaw) * x + std::cos(yaw) * y;
        const double y2 = std::cos(pitch) * y1 - std::sin(pitch) * z;
        x = x1;
        y = y2;
    }
    const double span = std::max({stock.max_corner.x - stock.min_corner.x,
                                  stock.max_corner.y - stock.min_corner.y,
                                  stock.max_corner.z - stock.min_corner.z, 1.0});
    const double scale = std::min(width(), height()) * 0.72 / span * zoom_;
    return {width() * 0.5 + pan_.x() + x * scale,
            height() * 0.5 + pan_.y() - y * scale};
}

void MillViewport::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#111827"));

    painter.setPen(QPen(QColor("#233044"), 1));
    constexpr int grid = 32;
    for (int x = 0; x < width(); x += grid) painter.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += grid) painter.drawLine(0, y, width(), y);

    if (!document_) return;
    const StockBox& s = document_->stock();
    const Vec3 corners[8] = {{s.min_corner.x, s.min_corner.y, s.min_corner.z},
                             {s.max_corner.x, s.min_corner.y, s.min_corner.z},
                             {s.max_corner.x, s.max_corner.y, s.min_corner.z},
                             {s.min_corner.x, s.max_corner.y, s.min_corner.z},
                             {s.min_corner.x, s.min_corner.y, s.max_corner.z},
                             {s.max_corner.x, s.min_corner.y, s.max_corner.z},
                             {s.max_corner.x, s.max_corner.y, s.max_corner.z},
                             {s.min_corner.x, s.max_corner.y, s.max_corner.z}};
    const int edges[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},
                              {0,4},{1,5},{2,6},{3,7}};
    if (!topView_) {
        QPainterPath top;
        top.moveTo(project(corners[4]));
        for (int i = 5; i <= 7; ++i) top.lineTo(project(corners[i]));
        top.closeSubpath();
        painter.fillPath(top, QColor(46, 87, 112, 70));
    }
    painter.setPen(QPen(QColor("#64748b"), 1.4));
    for (const auto& edge : edges) painter.drawLine(project(corners[edge[0]]), project(corners[edge[1]]));

    const QColor rapid("#f59e0b");
    const QColor cutting("#22d3ee");
    const QColor selected("#f8fafc");
    for (std::size_t pathIndex = 0; pathIndex < document_->toolpath_count(); ++pathIndex) {
        const Toolpath& path = document_->toolpath(pathIndex);
        for (std::size_t i = 1; i < path.moves.size(); ++i) {
            const ToolMove& move = path.moves[i];
            const bool isRapid = move.kind == MoveKind::Rapid || move.kind == MoveKind::Retract;
            QColor color = isRapid ? rapid : cutting;
            double width = isRapid ? 1.0 : 1.7;
            if (selectedToolpath_ && *selectedToolpath_ == pathIndex) {
                color = selected;
                width = isRapid ? 1.8 : 2.8;
            }
            QPen pen(color, width);
            if (isRapid) pen.setStyle(Qt::DashLine);
            painter.setPen(pen);
            painter.drawLine(project(path.moves[i - 1].pos), project(move.pos));
        }
    }

    painter.setPen(QColor("#cbd5e1"));
    painter.drawText(QRect(14, 12, width() - 28, 24), Qt::AlignLeft,
                     topView_ ? tr("TOP · XY · mm") : tr("ISOMETRIC · WCS · mm"));
    painter.setPen(QColor("#94a3b8"));
    painter.drawText(QRect(14, height() - 32, width() - 28, 20), Qt::AlignLeft,
                     tr("Cutting: cyan  ·  Rapid/retract: amber dashed"));
}

std::optional<std::size_t> MillViewport::hitToolpath(const QPointF& point) const {
    if (!document_) return std::nullopt;
    double best = 9.0;
    std::optional<std::size_t> result;
    for (std::size_t pathIndex = 0; pathIndex < document_->toolpath_count(); ++pathIndex) {
        const Toolpath& path = document_->toolpath(pathIndex);
        for (std::size_t i = 1; i < path.moves.size(); ++i) {
            const double distance = segmentDistance(point, project(path.moves[i - 1].pos),
                                                    project(path.moves[i].pos));
            if (distance < best) {
                best = distance;
                result = pathIndex;
            }
        }
    }
    return result;
}

void MillViewport::mousePressEvent(QMouseEvent* event) {
    pressPos_ = event->position().toPoint();
    lastPos_ = pressPos_;
    panning_ = event->button() == Qt::MiddleButton;
    orbiting_ = event->button() == Qt::LeftButton;
    event->accept();
}

void MillViewport::mouseMoveEvent(QMouseEvent* event) {
    const QPoint current = event->position().toPoint();
    const QPoint delta = current - lastPos_;
    lastPos_ = current;
    if (panning_) {
        pan_ += QPointF(delta);
        update();
    } else if (orbiting_ && !topView_ && (current - pressPos_).manhattanLength() > 3) {
        yaw_ += delta.x() * 0.45;
        pitch_ = std::clamp(pitch_ - delta.y() * 0.35, -85.0, 85.0);
        update();
    }
    event->accept();
}

void MillViewport::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton &&
        (event->position().toPoint() - pressPos_).manhattanLength() <= 3) {
        const auto hit = hitToolpath(event->position());
        if (hit) emit toolpathSelected(static_cast<int>(*hit));
    }
    orbiting_ = false;
    panning_ = false;
    event->accept();
}

void MillViewport::wheelEvent(QWheelEvent* event) {
    zoom_ = std::clamp(zoom_ * std::pow(1.0015, event->angleDelta().y()), 0.15, 12.0);
    update();
    event->accept();
}
