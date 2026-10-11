// SPDX-License-Identifier: MPL-2.0
#include "civil_viewport.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

CivilViewport::CivilViewport(QWidget* parent) : QWidget(parent) {
    setObjectName("civilPlanViewport");
    setMinimumSize(560, 360);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void CivilViewport::setDocument(const mydesk::civil::CivilDocument* document) {
    document_ = document;
    fitProject();
}

void CivilViewport::setSelection(EntityKind kind, std::uint64_t id) {
    selectionKind_ = kind;
    selectionId_ = id;
    update();
}

void CivilViewport::setContoursVisible(bool visible) { contoursVisible_ = visible; update(); }
void CivilViewport::setContourInterval(double interval) {
    contourInterval_ = std::max(interval, 1e-6);
    update();
}

void CivilViewport::fitProject() {
    if (!document_ || document_->points().empty()) {
        centerEasting_ = 0.0;
        centerNorthing_ = 0.0;
        pixelsPerUnit_ = 1.0;
        update();
        return;
    }
    double minE = std::numeric_limits<double>::max();
    double maxE = -minE;
    double minN = minE;
    double maxN = -minE;
    for (const auto& point : document_->points()) {
        minE = std::min(minE, point.easting); maxE = std::max(maxE, point.easting);
        minN = std::min(minN, point.northing); maxN = std::max(maxN, point.northing);
    }
    for (const auto& alignment : document_->alignments()) {
        for (const auto& segment : alignment.segments) {
            if (segment.kind == mydesk::civil::AlignmentSegment::Kind::Tangent) {
                minE = std::min({minE, segment.startX, segment.endX});
                maxE = std::max({maxE, segment.startX, segment.endX});
                minN = std::min({minN, segment.startY, segment.endY});
                maxN = std::max({maxN, segment.startY, segment.endY});
            } else {
                minE = std::min(minE, segment.centerX - segment.radius);
                maxE = std::max(maxE, segment.centerX + segment.radius);
                minN = std::min(minN, segment.centerY - segment.radius);
                maxN = std::max(maxN, segment.centerY + segment.radius);
            }
        }
    }
    centerEasting_ = (minE + maxE) * 0.5;
    centerNorthing_ = (minN + maxN) * 0.5;
    const double extentE = std::max(maxE - minE, 1.0);
    const double extentN = std::max(maxN - minN, 1.0);
    pixelsPerUnit_ = std::max(0.02, 0.82 * std::min(static_cast<double>(width()) / extentE,
                                                     static_cast<double>(height()) / extentN));
    update();
}

QPointF CivilViewport::toScreen(double easting, double northing) const {
    return {width() * 0.5 + (easting - centerEasting_) * pixelsPerUnit_,
            height() * 0.5 - (northing - centerNorthing_) * pixelsPerUnit_};
}

std::pair<double, double> CivilViewport::toWorld(const QPointF& screen) const {
    return {centerEasting_ + (screen.x() - width() * 0.5) / pixelsPerUnit_,
            centerNorthing_ - (screen.y() - height() * 0.5) / pixelsPerUnit_};
}

void CivilViewport::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(20, 27, 34));

    const double worldStep = std::pow(10.0, std::floor(std::log10(80.0 / pixelsPerUnit_)));
    const double gridStep = worldStep * (worldStep * pixelsPerUnit_ < 35.0 ? 5.0 : 1.0);
    const auto topLeft = toWorld({0.0, 0.0});
    const auto bottomRight = toWorld({static_cast<double>(width()), static_cast<double>(height())});
    painter.setPen(QPen(QColor(48, 59, 68), 1));
    for (double e = std::floor(topLeft.first / gridStep) * gridStep;
         e <= bottomRight.first; e += gridStep) {
        const double x = toScreen(e, 0.0).x();
        painter.drawLine(QPointF(x, 0.0), QPointF(x, height()));
    }
    for (double n = std::floor(bottomRight.second / gridStep) * gridStep;
         n <= topLeft.second; n += gridStep) {
        const double y = toScreen(0.0, n).y();
        painter.drawLine(QPointF(0.0, y), QPointF(width(), y));
    }
    if (!document_) return;

    for (const auto& surface : document_->surfaces()) {
        std::unordered_map<mydesk::civil::PointId, const mydesk::civil::CogoPoint*> pointMap;
        double minZ = std::numeric_limits<double>::max();
        double maxZ = -minZ;
        for (const auto& point : surface.points) {
            pointMap[point.id] = &point;
            minZ = std::min(minZ, point.elevation); maxZ = std::max(maxZ, point.elevation);
        }
        for (const auto& triangle : surface.triangles) {
            const auto* a = pointMap[triangle.p0]; const auto* b = pointMap[triangle.p1];
            const auto* c = pointMap[triangle.p2];
            if (!a || !b || !c) continue;
            const double z = (a->elevation + b->elevation + c->elevation) / 3.0;
            const double ratio = maxZ > minZ ? (z - minZ) / (maxZ - minZ) : 0.5;
            const int hue = static_cast<int>(std::lround((0.37 - 0.18 * ratio) * 359.0));
            QColor fill = QColor::fromHsv(hue, 107, 133, 122);
            if (selectionKind_ == EntityKind::Surface && selectionId_ == surface.id) {
                fill = QColor(45, 178, 205, 125);
            }
            painter.setBrush(fill);
            painter.setPen(QPen(QColor(104, 131, 138), 1));
            painter.drawPolygon(QPolygonF{toScreen(a->easting, a->northing),
                                           toScreen(b->easting, b->northing),
                                           toScreen(c->easting, c->northing)});
        }
        if (contoursVisible_ && !surface.points.empty()) {
            const double first = std::ceil(minZ / contourInterval_) * contourInterval_;
            const auto contours = surface.generateContours(contourInterval_, first, maxZ);
            painter.setPen(QPen(QColor(205, 163, 74), 1.2));
            for (const auto& contour : contours) {
                for (std::size_t i = 1; i < contour.points.size(); i += 2) {
                    painter.drawLine(toScreen(contour.points[i - 1].first, contour.points[i - 1].second),
                                     toScreen(contour.points[i].first, contour.points[i].second));
                }
            }
        }
    }

    for (const auto& alignment : document_->alignments()) {
        QPainterPath path;
        const int samples = std::max(20, static_cast<int>(alignment.segments.size()) * 24);
        for (int i = 0; i <= samples; ++i) {
            const double station = alignment.startStation + alignment.totalLength() *
                static_cast<double>(i) / static_cast<double>(samples);
            const auto point = alignment.getPointAtStation(station);
            const QPointF screen = toScreen(point.first, point.second);
            if (i == 0) path.moveTo(screen); else path.lineTo(screen);
        }
        const bool selected = selectionKind_ == EntityKind::Alignment && selectionId_ == alignment.id;
        painter.setPen(QPen(selected ? QColor(255, 210, 74) : QColor(241, 110, 62),
                            selected ? 4.0 : 2.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

    for (const auto& point : document_->points()) {
        const QPointF screen = toScreen(point.easting, point.northing);
        const bool selected = selectionKind_ == EntityKind::Point && selectionId_ == point.id;
        painter.setPen(QPen(QColor(8, 16, 22), 1));
        painter.setBrush(selected ? QColor(255, 218, 84) : QColor(106, 210, 239));
        painter.drawEllipse(screen, selected ? 6.0 : 4.5, selected ? 6.0 : 4.5);
        painter.setPen(QColor(219, 230, 236));
        painter.drawText(screen + QPointF(7.0, -7.0), QString::fromStdString(point.pointNumber));
    }

    painter.setPen(QColor(205, 216, 224));
    painter.drawText(QRect(12, 9, width() - 24, 40), Qt::AlignLeft | Qt::AlignTop,
        QStringLiteral("Plan View  |  LMB select  MMB pan  Wheel zoom  |  E/N survey coordinates"));
}

void CivilViewport::pick(const QPointF& position) {
    if (!document_) return;
    double best = 13.0;
    EntityKind kind = EntityKind::None;
    std::uint64_t id = 0;
    for (const auto& point : document_->points()) {
        const QPointF delta = toScreen(point.easting, point.northing) - position;
        const double distance = std::hypot(delta.x(), delta.y());
        if (distance < best) { best = distance; kind = EntityKind::Point; id = point.id; }
    }
    for (const auto& alignment : document_->alignments()) {
        for (int i = 0; i <= 100; ++i) {
            const auto point = alignment.getPointAtStation(alignment.startStation +
                alignment.totalLength() * static_cast<double>(i) / 100.0);
            const QPointF delta = toScreen(point.first, point.second) - position;
            const double distance = std::hypot(delta.x(), delta.y());
            if (distance < best) { best = distance; kind = EntityKind::Alignment; id = alignment.id; }
        }
    }
    emit entitySelected(kind, id);
}

void CivilViewport::mousePressEvent(QMouseEvent* event) {
    lastMouse_ = event->position().toPoint();
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) panning_ = true;
    else if (event->button() == Qt::LeftButton) pick(event->position());
}

void CivilViewport::mouseMoveEvent(QMouseEvent* event) {
    const auto world = toWorld(event->position());
    emit coordinateHovered(world.first, world.second);
    if (!panning_) return;
    const QPoint current = event->position().toPoint();
    const QPoint delta = current - lastMouse_;
    lastMouse_ = current;
    centerEasting_ -= delta.x() / pixelsPerUnit_;
    centerNorthing_ += delta.y() / pixelsPerUnit_;
    update();
}

void CivilViewport::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) panning_ = false;
}

void CivilViewport::wheelEvent(QWheelEvent* event) {
    const auto before = toWorld(event->position());
    pixelsPerUnit_ = std::clamp(pixelsPerUnit_ * std::pow(1.0015, event->angleDelta().y()),
                                0.001, 100000.0);
    const auto after = toWorld(event->position());
    centerEasting_ += before.first - after.first;
    centerNorthing_ += before.second - after.second;
    update();
}
