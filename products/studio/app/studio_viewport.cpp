// SPDX-License-Identifier: MPL-2.0
#include "studio_viewport.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QSizeF>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace mydesk::studio::ui {

namespace {

QColor objectColor(ObjectId id) {
    static const QColor palette[] = {
        QColor("#38BDF8"), QColor("#A78BFA"), QColor("#34D399"),
        QColor("#F59E0B"), QColor("#FB7185"), QColor("#22D3EE")};
    return palette[id % (sizeof(palette) / sizeof(palette[0]))];
}

} // namespace

StudioViewport::StudioViewport(QWidget* parent) : QWidget(parent) {
    setObjectName("studioViewport");
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(480, 320);
    setMouseTracking(true);
    setToolTip("Left drag: orbit · Middle/right drag: pan · Wheel: zoom · Click: select");
}

void StudioViewport::setDocument(StudioDocument* document) {
    document_ = document;
    if (!document_ || !document_->findObject(selectedObject_)) selectedObject_ = 0;
    frameAll();
}

void StudioViewport::setSelectedObject(ObjectId id) {
    selectedObject_ = id;
    update();
}

Vec3 StudioViewport::sceneCenter() const {
    if (!document_ || document_->objects().empty()) return {};
    Vec3 minimum{std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
                 std::numeric_limits<double>::max()};
    Vec3 maximum{-minimum.x, -minimum.y, -minimum.z};
    bool found = false;
    for (const StudioObject& object : document_->objects()) {
        for (const Vec3& vertex : object.evaluatedMesh.vertices) {
            found = true;
            minimum.x = std::min(minimum.x, vertex.x);
            minimum.y = std::min(minimum.y, vertex.y);
            minimum.z = std::min(minimum.z, vertex.z);
            maximum.x = std::max(maximum.x, vertex.x);
            maximum.y = std::max(maximum.y, vertex.y);
            maximum.z = std::max(maximum.z, vertex.z);
        }
    }
    return found ? Vec3{(minimum.x + maximum.x) * 0.5, (minimum.y + maximum.y) * 0.5,
                        (minimum.z + maximum.z) * 0.5} : Vec3{};
}

double StudioViewport::sceneExtent() const {
    if (!document_ || document_->objects().empty()) return 10.0;
    double extent = 0.0;
    const Vec3 center = sceneCenter();
    for (const StudioObject& object : document_->objects()) {
        for (const Vec3& vertex : object.evaluatedMesh.vertices) {
            extent = std::max({extent, std::abs(vertex.x - center.x),
                               std::abs(vertex.y - center.y), std::abs(vertex.z - center.z)});
        }
    }
    return std::max(extent, 1.0);
}

StudioViewport::ProjectedPoint StudioViewport::project(const Vec3& point) const {
    const Vec3 center = sceneCenter();
    const double dx = point.x - center.x;
    const double dy = point.y - center.y;
    const double dz = point.z - center.z;
    const double cy = std::cos(yaw_), sy = std::sin(yaw_);
    const double cp = std::cos(pitch_), sp = std::sin(pitch_);
    const double horizontal = cy * dx - sy * dy;
    const double orbitDepth = sy * dx + cy * dy;
    const double vertical = cp * dz - sp * orbitDepth;
    const double depth = sp * dz + cp * orbitDepth;
    return {{width() * 0.5 + pan_.x() + horizontal * zoom_,
             height() * 0.5 + pan_.y() - vertical * zoom_}, depth};
}

void StudioViewport::frameAll() {
    pan_ = {};
    const double extent = sceneExtent();
    const int shortest = std::max(200, std::min(width(), height()));
    zoom_ = std::clamp(static_cast<double>(shortest) * 0.38 / extent, 0.02, 5000.0);
    update();
    emit cameraChanged(QStringLiteral("Perspective · %1 px/unit").arg(zoom_, 0, 'f', 2));
}

void StudioViewport::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#0B1220"));

    // World axes and ground grid give scale/orientation without decorative chrome.
    const double extent = sceneExtent();
    const double gridStep = std::pow(10.0, std::floor(std::log10(extent))) * 0.5;
    painter.setPen(QPen(QColor(51, 65, 85, 115), 1.0));
    for (int i = -10; i <= 10; ++i) {
        const double value = static_cast<double>(i) * gridStep;
        painter.drawLine(project({-10.0 * gridStep, value, 0.0}).point,
                         project({10.0 * gridStep, value, 0.0}).point);
        painter.drawLine(project({value, -10.0 * gridStep, 0.0}).point,
                         project({value, 10.0 * gridStep, 0.0}).point);
    }
    painter.setPen(QPen(QColor("#EF4444"), 1.6));
    painter.drawLine(project({0, 0, 0}).point, project({extent, 0, 0}).point);
    painter.setPen(QPen(QColor("#22C55E"), 1.6));
    painter.drawLine(project({0, 0, 0}).point, project({0, extent, 0}).point);
    painter.setPen(QPen(QColor("#3B82F6"), 1.6));
    painter.drawLine(project({0, 0, 0}).point, project({0, 0, extent}).point);

    if (!document_) return;
    struct Face { QPolygonF polygon; double depth; ObjectId object; };
    std::vector<Face> faces;
    for (const StudioObject& object : document_->objects()) {
        for (const Triangle& triangle : object.evaluatedMesh.triangles) {
            if (triangle.v0 >= object.evaluatedMesh.vertices.size() ||
                triangle.v1 >= object.evaluatedMesh.vertices.size() ||
                triangle.v2 >= object.evaluatedMesh.vertices.size()) continue;
            const auto a = project(object.evaluatedMesh.vertices[triangle.v0]);
            const auto b = project(object.evaluatedMesh.vertices[triangle.v1]);
            const auto c = project(object.evaluatedMesh.vertices[triangle.v2]);
            faces.push_back({QPolygonF{a.point, b.point, c.point},
                             (a.depth + b.depth + c.depth) / 3.0, object.id});
        }
    }
    std::sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) {
        return a.depth < b.depth;
    });
    for (const Face& face : faces) {
        QColor fill = objectColor(face.object);
        fill.setAlpha(face.object == selectedObject_ ? 110 : 42);
        QColor edge = objectColor(face.object).lighter(face.object == selectedObject_ ? 150 : 105);
        painter.setBrush(fill);
        painter.setPen(QPen(edge, face.object == selectedObject_ ? 1.8 : 0.8));
        painter.drawPolygon(face.polygon);
    }

    painter.setPen(QColor("#CBD5E1"));
    painter.drawText(QRect(14, 12, width() - 28, 28), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("PERSPECTIVE  |  SHADED + EDGES"));
    if (document_->objects().empty()) {
        painter.setPen(QColor("#94A3B8"));
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("Create a primitive from the Command Panel"));
    }
}

ObjectId StudioViewport::hitObject(const QPointF& point) const {
    if (!document_) return 0;
    ObjectId best = 0;
    double bestArea = std::numeric_limits<double>::max();
    for (const StudioObject& object : document_->objects()) {
        QRectF bounds;
        bool first = true;
        for (const Vec3& vertex : object.evaluatedMesh.vertices) {
            const QPointF projected = project(vertex).point;
            if (first) {
                bounds = QRectF(projected, QSizeF(1.0, 1.0));
                first = false;
            } else {
                bounds = bounds.united(QRectF(projected, QSizeF(1.0, 1.0)));
            }
        }
        bounds.adjust(-5.0, -5.0, 5.0, 5.0);
        const double area = bounds.width() * bounds.height();
        if (!first && bounds.contains(point) && area < bestArea) {
            best = object.id;
            bestArea = area;
        }
    }
    return best;
}

void StudioViewport::mousePressEvent(QMouseEvent* event) {
    setFocus();
    lastMouse_ = event->position().toPoint();
    pressMouse_ = lastMouse_;
    dragButton_ = event->button();
    event->accept();
}

void StudioViewport::mouseMoveEvent(QMouseEvent* event) {
    if (dragButton_ == Qt::NoButton) return;
    const QPoint current = event->position().toPoint();
    const QPoint delta = current - lastMouse_;
    lastMouse_ = current;
    if (dragButton_ == Qt::LeftButton) {
        yaw_ += static_cast<double>(delta.x()) * 0.008;
        pitch_ = std::clamp(pitch_ + static_cast<double>(delta.y()) * 0.008, -1.45, 1.45);
    } else {
        pan_ += QPointF(delta);
    }
    update();
    emit cameraChanged(QStringLiteral("Perspective · orbit %1°, %2°")
                       .arg(yaw_ * 180.0 / 3.14159265358979323846, 0, 'f', 1)
                       .arg(pitch_ * 180.0 / 3.14159265358979323846, 0, 'f', 1));
}

void StudioViewport::mouseReleaseEvent(QMouseEvent* event) {
    if ((event->position().toPoint() - pressMouse_).manhattanLength() < 4 &&
        dragButton_ == Qt::LeftButton) {
        selectedObject_ = hitObject(event->position());
        emit objectSelected(selectedObject_);
        update();
    }
    dragButton_ = Qt::NoButton;
    event->accept();
}

void StudioViewport::wheelEvent(QWheelEvent* event) {
    const double factor = std::pow(1.0015, static_cast<double>(event->angleDelta().y()));
    zoom_ = std::clamp(zoom_ * factor, 0.02, 5000.0);
    update();
    emit cameraChanged(QStringLiteral("Perspective · %1 px/unit").arg(zoom_, 0, 'f', 2));
    event->accept();
}

} // namespace mydesk::studio::ui
