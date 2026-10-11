#include "coordination_viewport.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {

QColor disciplineColor(const std::string& discipline) {
    const auto hash = qHash(QString::fromStdString(discipline));
    return QColor::fromHsv(static_cast<int>(hash % 360U), 145, 215, 155);
}

} // namespace

CoordinationViewport::CoordinationViewport(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("coordinationViewport"));
    setMinimumSize(480, 360);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

void CoordinationViewport::setDocument(
    const mydesk::coordinate::CoordinateDocument* document) {
    document_ = document;
    selectedElement_ = 0;
    fitAll();
}

void CoordinationViewport::setSelectedElement(mydesk::coordinate::ElementId id) {
    selectedElement_ = id;
    update();
}

void CoordinationViewport::fitAll() {
    recomputeSceneBounds();
    zoom_ = 1.0;
    pan_ = {};
    update();
    emit viewChanged(tr("Fit federated model"));
}

void CoordinationViewport::focusPoint(const mydesk::coordinate::Vector3D& point) {
    viewCenter_ = point;
    zoom_ = std::max(zoom_, 2.0);
    pan_ = {};
    update();
    emit viewChanged(tr("Focused clash viewpoint at %1, %2, %3")
                         .arg(point.x, 0, 'f', 3)
                         .arg(point.y, 0, 'f', 3)
                         .arg(point.z, 0, 'f', 3));
}

QPointF CoordinationViewport::project(const mydesk::coordinate::Vector3D& point,
                                      double* depth) const {
    const double x = point.x - viewCenter_.x;
    const double y = point.y - viewCenter_.y;
    const double z = point.z - viewCenter_.z;
    const double cosineYaw = std::cos(yaw_);
    const double sineYaw = std::sin(yaw_);
    const double cameraX = cosineYaw * x - sineYaw * y;
    const double yawY = sineYaw * x + cosineYaw * y;
    const double cosinePitch = std::cos(pitch_);
    const double sinePitch = std::sin(pitch_);
    const double cameraY = cosinePitch * z - sinePitch * yawY;
    const double cameraDepth = sinePitch * z + cosinePitch * yawY;
    if (depth) {
        *depth = cameraDepth;
    }
    const double base = 0.82 * static_cast<double>(std::min(width(), height()));
    const double scale = base * zoom_ / std::max(sceneSpan_, 1.0e-6);
    return {static_cast<double>(width()) * 0.5 + pan_.x() + cameraX * scale,
            static_cast<double>(height()) * 0.5 + pan_.y() - cameraY * scale};
}

std::array<mydesk::coordinate::Vector3D, 8> CoordinationViewport::elementVertices(
    const mydesk::coordinate::CoordinationElement& element) const {
    std::array<mydesk::coordinate::Vector3D, 8> vertices{};
    if (element.orientedBounds) {
        const auto& box = *element.orientedBounds;
        std::size_t index = 0;
        for (const double sx : {-1.0, 1.0}) {
            for (const double sy : {-1.0, 1.0}) {
                for (const double sz : {-1.0, 1.0}) {
                    vertices[index++] = {
                        box.center.x + sx * box.halfExtents.x * box.axes[0].x +
                            sy * box.halfExtents.y * box.axes[1].x +
                            sz * box.halfExtents.z * box.axes[2].x,
                        box.center.y + sx * box.halfExtents.x * box.axes[0].y +
                            sy * box.halfExtents.y * box.axes[1].y +
                            sz * box.halfExtents.z * box.axes[2].y,
                        box.center.z + sx * box.halfExtents.x * box.axes[0].z +
                            sy * box.halfExtents.y * box.axes[1].z +
                            sz * box.halfExtents.z * box.axes[2].z};
                }
            }
        }
        return vertices;
    }
    const auto& bounds = element.bounds;
    vertices = {{{bounds.minX, bounds.minY, bounds.minZ},
                 {bounds.minX, bounds.minY, bounds.maxZ},
                 {bounds.minX, bounds.maxY, bounds.minZ},
                 {bounds.minX, bounds.maxY, bounds.maxZ},
                 {bounds.maxX, bounds.minY, bounds.minZ},
                 {bounds.maxX, bounds.minY, bounds.maxZ},
                 {bounds.maxX, bounds.maxY, bounds.minZ},
                 {bounds.maxX, bounds.maxY, bounds.maxZ}}};
    return vertices;
}

void CoordinationViewport::recomputeSceneBounds() {
    if (!document_ || document_->models().empty()) {
        viewCenter_ = {};
        sceneSpan_ = 10.0;
        return;
    }
    double minimumX = std::numeric_limits<double>::infinity();
    double minimumY = minimumX;
    double minimumZ = minimumX;
    double maximumX = -minimumX;
    double maximumY = -minimumX;
    double maximumZ = -minimumX;
    bool hasElements = false;
    for (const auto& model : document_->models()) {
        for (const auto& element : model.elements) {
            hasElements = true;
            minimumX = std::min(minimumX, element.bounds.minX);
            minimumY = std::min(minimumY, element.bounds.minY);
            minimumZ = std::min(minimumZ, element.bounds.minZ);
            maximumX = std::max(maximumX, element.bounds.maxX);
            maximumY = std::max(maximumY, element.bounds.maxY);
            maximumZ = std::max(maximumZ, element.bounds.maxZ);
        }
    }
    if (!hasElements) {
        viewCenter_ = {};
        sceneSpan_ = 10.0;
        return;
    }
    viewCenter_ = {(minimumX + maximumX) * 0.5, (minimumY + maximumY) * 0.5,
                   (minimumZ + maximumZ) * 0.5};
    sceneSpan_ = std::max({maximumX - minimumX, maximumY - minimumY,
                           maximumZ - minimumZ, 1.0});
}

void CoordinationViewport::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(28, 32, 38));

    painter.setPen(QPen(QColor(62, 68, 76), 1.0));
    const double gridExtent = sceneSpan_ * 0.75;
    for (int line = -5; line <= 5; ++line) {
        const double coordinate = gridExtent * static_cast<double>(line) / 5.0;
        painter.drawLine(project({-gridExtent + viewCenter_.x, coordinate + viewCenter_.y, 0.0}),
                         project({gridExtent + viewCenter_.x, coordinate + viewCenter_.y, 0.0}));
        painter.drawLine(project({coordinate + viewCenter_.x, -gridExtent + viewCenter_.y, 0.0}),
                         project({coordinate + viewCenter_.x, gridExtent + viewCenter_.y, 0.0}));
    }

    hitRegions_.clear();
    if (!document_) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, tr("No coordination session"));
        return;
    }

    constexpr std::array<std::array<int, 4>, 6> faces{{{{0, 1, 3, 2}},
                                                        {{4, 6, 7, 5}},
                                                        {{0, 4, 5, 1}},
                                                        {{2, 3, 7, 6}},
                                                        {{0, 2, 6, 4}},
                                                        {{1, 5, 7, 3}}}};
    for (const auto& model : document_->models()) {
        const QColor baseColor = disciplineColor(model.discipline);
        for (const auto& element : model.elements) {
            const auto vertices = elementVertices(element);
            std::array<QPointF, 8> points{};
            double averageDepth = 0.0;
            QRectF bounds;
            for (std::size_t index = 0; index < vertices.size(); ++index) {
                double depth = 0.0;
                points[index] = project(vertices[index], &depth);
                averageDepth += depth;
                bounds = index == 0 ? QRectF(points[index], QSizeF{}) : bounds.united(
                    QRectF(points[index], QSizeF{}));
            }
            averageDepth /= static_cast<double>(vertices.size());
            for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
                QPolygonF polygon;
                for (const int vertexIndex : faces[faceIndex]) {
                    polygon << points[static_cast<std::size_t>(vertexIndex)];
                }
                QColor faceColor = baseColor.lighter(90 + static_cast<int>(faceIndex) * 6);
                faceColor.setAlpha(element.id == selectedElement_ ? 185 : 105);
                painter.setBrush(faceColor);
                painter.setPen(QPen(element.id == selectedElement_ ? QColor(255, 196, 66)
                                                                    : baseColor.lighter(125),
                                    element.id == selectedElement_ ? 2.5 : 1.0));
                painter.drawPolygon(polygon);
            }
            hitRegions_.push_back({bounds.adjusted(-4.0, -4.0, 4.0, 4.0), element.id,
                                   averageDepth});
        }
    }
    std::sort(hitRegions_.begin(), hitRegions_.end(),
              [](const HitRegion& left, const HitRegion& right) {
                  return left.depth > right.depth;
              });

    painter.setPen(QColor(215, 220, 226));
    painter.drawText(QRect(12, 10, width() - 24, 24), Qt::AlignLeft,
                     tr("Orbit: right drag   Pan: middle drag   Zoom: wheel   Select: left click"));
}

void CoordinationViewport::mousePressEvent(QMouseEvent* event) {
    lastMousePosition_ = event->position().toPoint();
    dragButton_ = event->button();
    if (event->button() == Qt::LeftButton) {
        for (const auto& hit : hitRegions_) {
            if (hit.rect.contains(event->position())) {
                selectedElement_ = hit.id;
                emit elementSelected(static_cast<qulonglong>(hit.id));
                update();
                return;
            }
        }
        selectedElement_ = 0;
        emit elementSelected(0);
        update();
    }
}

void CoordinationViewport::mouseMoveEvent(QMouseEvent* event) {
    const QPoint current = event->position().toPoint();
    const QPoint delta = current - lastMousePosition_;
    lastMousePosition_ = current;
    if (dragButton_ == Qt::RightButton) {
        yaw_ += static_cast<double>(delta.x()) * 0.008;
        pitch_ = std::clamp(pitch_ + static_cast<double>(delta.y()) * 0.008, -1.4, 1.4);
        update();
        emit viewChanged(tr("Orbit %1°, %2°")
                             .arg(yaw_ * 180.0 / 3.14159265358979323846, 0, 'f', 1)
                             .arg(pitch_ * 180.0 / 3.14159265358979323846, 0, 'f', 1));
    } else if (dragButton_ == Qt::MiddleButton) {
        pan_ += QPointF(delta);
        update();
        emit viewChanged(tr("Pan view"));
    }
}

void CoordinationViewport::mouseReleaseEvent(QMouseEvent*) { dragButton_ = Qt::NoButton; }

void CoordinationViewport::wheelEvent(QWheelEvent* event) {
    zoom_ = std::clamp(zoom_ * std::pow(1.0015, event->angleDelta().y()), 0.08, 40.0);
    update();
    emit viewChanged(tr("Zoom %1%").arg(zoom_ * 100.0, 0, 'f', 0));
}
