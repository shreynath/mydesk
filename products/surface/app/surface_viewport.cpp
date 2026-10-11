// SPDX-License-Identifier: MPL-2.0
#include "surface_viewport.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using mydesk::surface::Vec3;

namespace {
QColor surfaceColor(std::uint64_t id, bool selected) {
    if (selected) return QColor(53, 184, 219, 220);
    const int hue = static_cast<int>((id * 47U) % 80U) + 185;
    return QColor::fromHsv(hue, 125, 178, 210);
}
}

SurfaceViewport::SurfaceViewport(QWidget* parent) : QWidget(parent) {
    setObjectName("surfaceViewport");
    setMinimumSize(480, 320);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

void SurfaceViewport::setDocument(const mydesk::surface::SurfaceDocument* document) {
    document_ = document;
    frameDocument();
    update();
}

void SurfaceViewport::setSelection(EntityKind kind, std::uint64_t id, int controlPoint) {
    kind_ = kind;
    id_ = id;
    controlPoint_ = controlPoint;
    update();
}

void SurfaceViewport::setControlNetVisible(bool visible) { controlNet_ = visible; update(); }
void SurfaceViewport::setZebraVisible(bool visible) { zebra_ = visible; update(); }

void SurfaceViewport::frameDocument() {
    if (!document_) return;
    Vec3 low{std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
    Vec3 high{-low.x, -low.y, -low.z};
    bool found = false;
    auto include = [&](const Vec3& p) {
        low.x = std::min(low.x, p.x); low.y = std::min(low.y, p.y); low.z = std::min(low.z, p.z);
        high.x = std::max(high.x, p.x); high.y = std::max(high.y, p.y); high.z = std::max(high.z, p.z);
        found = true;
    };
    for (const auto& curve : document_->curves()) for (const auto& cp : curve.controlPoints) include(cp.p);
    for (const auto& surface : document_->surfaces()) for (const auto& cp : surface.controlGrid) include(cp.p);
    if (!found) { center_ = {}; zoom_ = 5.0; return; }
    center_ = {(low.x + high.x) * 0.5, (low.y + high.y) * 0.5, (low.z + high.z) * 0.5};
    const double extent = std::max({high.x - low.x, high.y - low.y, high.z - low.z, 1.0});
    zoom_ = std::max(1.0, 0.72 * static_cast<double>(std::min(width(), height())) / extent);
}

SurfaceViewport::ScreenPoint SurfaceViewport::project(const Vec3& p) const {
    const double x = p.x - center_.x, y = p.y - center_.y, z = p.z - center_.z;
    const double cy = std::cos(yaw_), sy = std::sin(yaw_);
    const double cp = std::cos(pitch_), sp = std::sin(pitch_);
    const double rx = cy * x - sy * y;
    const double ry = sy * x + cy * y;
    const double py = cp * z - sp * ry;
    const double depth = sp * z + cp * ry;
    return {{width() * 0.5 + rx * zoom_, height() * 0.5 - py * zoom_}, depth};
}

void SurfaceViewport::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(27, 31, 38));

    painter.setPen(QPen(QColor(48, 55, 64), 1));
    const int gridStep = 40;
    for (int x = width() / 2 % gridStep; x < width(); x += gridStep) painter.drawLine(x, 0, x, height());
    for (int y = height() / 2 % gridStep; y < height(); y += gridStep) painter.drawLine(0, y, width(), y);
    if (!document_) return;

    struct Triangle { QPolygonF polygon; double depth; QColor color; };
    std::vector<Triangle> triangles;
    constexpr int resolution = 18;
    for (const auto& surface : document_->surfaces()) {
        const bool selected = kind_ == EntityKind::Surface && id_ == surface.id;
        const QColor base = surfaceColor(surface.id, selected);
        for (int u = 0; u < resolution; ++u) {
            for (int v = 0; v < resolution; ++v) {
                const double u0 = static_cast<double>(u) / resolution, u1 = static_cast<double>(u + 1) / resolution;
                const double v0 = static_cast<double>(v) / resolution, v1 = static_cast<double>(v + 1) / resolution;
                const auto a = project(surface.evaluate(u0, v0));
                const auto b = project(surface.evaluate(u1, v0));
                const auto c = project(surface.evaluate(u1, v1));
                const auto d = project(surface.evaluate(u0, v1));
                QColor color = base;
                if (zebra_) {
                    const auto n = surface.normalAt((u0 + u1) * 0.5, (v0 + v1) * 0.5);
                    const int value = 55 + static_cast<int>(170.0 * (0.5 + 0.5 * std::sin(35.0 * n.y)));
                    color = QColor(value, value, value, 235);
                } else {
                    const auto n = surface.normalAt((u0 + u1) * 0.5, (v0 + v1) * 0.5);
                    color = color.lighter(75 + static_cast<int>(35.0 * std::abs(n.z)));
                }
                triangles.push_back({QPolygonF{a.point, b.point, c.point}, (a.depth+b.depth+c.depth)/3.0, color});
                triangles.push_back({QPolygonF{a.point, c.point, d.point}, (a.depth+c.depth+d.depth)/3.0, color});
            }
        }
    }
    std::sort(triangles.begin(), triangles.end(), [](const Triangle& a, const Triangle& b) { return a.depth < b.depth; });
    painter.setPen(QPen(QColor(15, 19, 24, 90), 0.7));
    for (const auto& triangle : triangles) { painter.setBrush(triangle.color); painter.drawPolygon(triangle.polygon); }

    for (const auto& curve : document_->curves()) {
        QPainterPath path;
        constexpr int samples = 80;
        for (int i = 0; i <= samples; ++i) {
            const auto p = project(curve.evaluate(static_cast<double>(i) / samples)).point;
            if (i == 0) path.moveTo(p); else path.lineTo(p);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(kind_ == EntityKind::Curve && id_ == curve.id ? QColor(255, 191, 64) : QColor(238, 133, 73),
                            kind_ == EntityKind::Curve && id_ == curve.id ? 3.0 : 2.0));
        painter.drawPath(path);
    }

    if (controlNet_ && kind_ != EntityKind::None) {
        std::vector<mydesk::surface::WeightedPoint> points;
        std::size_t rows = 1, columns = 0;
        if (kind_ == EntityKind::Curve) {
            if (const auto* curve = document_->findCurve(id_)) points = curve->controlPoints;
        } else if (const auto* surface = document_->findSurface(id_)) {
            points = surface->controlGrid; rows = surface->numU; columns = surface->numV;
        }
        painter.setPen(QPen(QColor(245, 181, 65), 1, Qt::DashLine));
        if (kind_ == EntityKind::Curve) {
            for (std::size_t i = 1; i < points.size(); ++i) painter.drawLine(project(points[i-1].p).point, project(points[i].p).point);
        } else if (columns > 0) {
            for (std::size_t i = 0; i < rows; ++i) for (std::size_t j = 1; j < columns; ++j)
                painter.drawLine(project(points[i*columns+j-1].p).point, project(points[i*columns+j].p).point);
            for (std::size_t j = 0; j < columns; ++j) for (std::size_t i = 1; i < rows; ++i)
                painter.drawLine(project(points[(i-1)*columns+j].p).point, project(points[i*columns+j].p).point);
        }
        for (std::size_t i = 0; i < points.size(); ++i) {
            const QPointF p = project(points[i].p).point;
            painter.setBrush(static_cast<int>(i) == controlPoint_ ? QColor(255, 238, 128) : QColor(249, 177, 44));
            painter.setPen(QPen(QColor(33, 29, 20), 1));
            painter.drawEllipse(p, static_cast<int>(i) == controlPoint_ ? 5.5 : 4.0,
                                static_cast<int>(i) == controlPoint_ ? 5.5 : 4.0);
        }
    }

    painter.setPen(QColor(210, 219, 229));
    painter.drawText(QRect(12, 10, width()-24, 42), Qt::AlignLeft | Qt::AlignTop,
                     QStringLiteral("LMB select / control vertex   MMB orbit   Shift+MMB pan   Wheel zoom"));
}

void SurfaceViewport::pick(const QPoint& position) {
    if (!document_) return;
    double best = 14.0;
    EntityKind bestKind = EntityKind::None;
    std::uint64_t bestId = 0;
    int bestCp = -1;
    auto considerPoint = [&](EntityKind kind, std::uint64_t id, int cp, const Vec3& p) {
        const QPointF d = project(p).point - QPointF(position);
        const double distance = std::hypot(d.x(), d.y());
        if (distance < best) { best = distance; bestKind = kind; bestId = id; bestCp = cp; }
    };
    if (kind_ == EntityKind::Curve) if (const auto* c = document_->findCurve(id_))
        for (std::size_t i=0;i<c->controlPoints.size();++i) considerPoint(kind_, id_, static_cast<int>(i), c->controlPoints[i].p);
    if (kind_ == EntityKind::Surface) if (const auto* s = document_->findSurface(id_))
        for (std::size_t i=0;i<s->controlGrid.size();++i) considerPoint(kind_, id_, static_cast<int>(i), s->controlGrid[i].p);
    if (bestKind == EntityKind::None) {
        for (const auto& curve : document_->curves()) for (int i=0;i<=40;++i)
            considerPoint(EntityKind::Curve, curve.id, -1, curve.evaluate(static_cast<double>(i)/40.0));
        for (const auto& surface : document_->surfaces()) for (int i=0;i<=8;++i) for (int j=0;j<=8;++j)
            considerPoint(EntityKind::Surface, surface.id, -1, surface.evaluate(static_cast<double>(i)/8.0, static_cast<double>(j)/8.0));
    }
    if (bestKind != EntityKind::None) emit entitySelected(bestKind, bestId, bestCp);
    else emit entitySelected(EntityKind::None, 0, -1);
}

void SurfaceViewport::mousePressEvent(QMouseEvent* event) {
    lastMouse_ = event->position().toPoint();
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) navigationButton_ = event->button();
    else if (event->button() == Qt::LeftButton) pick(lastMouse_);
}

void SurfaceViewport::mouseMoveEvent(QMouseEvent* event) {
    if (navigationButton_ == Qt::NoButton) return;
    const QPoint now = event->position().toPoint();
    const QPoint delta = now - lastMouse_;
    lastMouse_ = now;
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        const double scale = 1.0 / std::max(zoom_, 0.01);
        center_.x -= delta.x() * scale;
        center_.z += delta.y() * scale;
    } else {
        yaw_ += delta.x() * 0.008;
        pitch_ = std::clamp(pitch_ + delta.y() * 0.008, -1.45, 1.45);
    }
    update();
}

void SurfaceViewport::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == navigationButton_) navigationButton_ = Qt::NoButton;
}

void SurfaceViewport::wheelEvent(QWheelEvent* event) {
    zoom_ = std::clamp(zoom_ * std::pow(1.0015, event->angleDelta().y()), 0.05, 5000.0);
    update();
}
