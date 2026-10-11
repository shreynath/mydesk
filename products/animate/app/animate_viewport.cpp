#include "animate_viewport.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QSizeF>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace mydesk::animate::ui {

AnimateViewport::AnimateViewport(QWidget* parent) : QWidget(parent) {
    setObjectName("animateViewport");
    setMinimumSize(520, 340);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip("Left drag: orbit · Middle/right drag: pan · Wheel: zoom · Click: select");
}

void AnimateViewport::setDocument(AnimateDocument* document) {
    document_ = document;
    selected_ = kInvalidNode;
    frameAll();
}

void AnimateViewport::setFrame(double frame) { frame_ = frame; update(); }
void AnimateViewport::setSelectedNode(NodeId id) { selected_ = id; update(); }

Vec3 AnimateViewport::transformed(const Mat4& m, const Vec3& p) const {
    return {m.m[0]*p.x + m.m[4]*p.y + m.m[8]*p.z + m.m[12],
            m.m[1]*p.x + m.m[5]*p.y + m.m[9]*p.z + m.m[13],
            m.m[2]*p.x + m.m[6]*p.y + m.m[10]*p.z + m.m[14]};
}

AnimateViewport::Projection AnimateViewport::project(const Vec3& p) const {
    const double cy = std::cos(yaw_), sy = std::sin(yaw_);
    const double cp = std::cos(pitch_), sp = std::sin(pitch_);
    const double horizontal = cy*p.x - sy*p.z;
    const double depth0 = sy*p.x + cy*p.z;
    const double vertical = cp*p.y - sp*depth0;
    const double depth = sp*p.y + cp*depth0;
    return {{width()*0.5 + pan_.x() + horizontal*zoom_,
             height()*0.5 + pan_.y() - vertical*zoom_}, depth};
}

void AnimateViewport::frameAll() { pan_ = {}; zoom_ = 70.0; update(); }

void AnimateViewport::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#10151d"));
    painter.setPen(QPen(QColor("#293241"), 1));
    for (int i = -10; i <= 10; ++i) {
        painter.drawLine(project({-10.0, 0.0, static_cast<double>(i)}).point,
                         project({10.0, 0.0, static_cast<double>(i)}).point);
        painter.drawLine(project({static_cast<double>(i), 0.0, -10.0}).point,
                         project({static_cast<double>(i), 0.0, 10.0}).point);
    }
    painter.setPen(QPen(QColor("#e74c3c"), 2));
    painter.drawLine(project({0,0,0}).point, project({2,0,0}).point);
    painter.setPen(QPen(QColor("#2ecc71"), 2));
    painter.drawLine(project({0,0,0}).point, project({0,2,0}).point);
    painter.setPen(QPen(QColor("#3498db"), 2));
    painter.drawLine(project({0,0,0}).point, project({0,0,2}).point);
    if (!document_) return;
    const double seconds = document_->timeline().frame_to_time(frame_);
    const auto matrices = document_->world_matrices(seconds);
    struct Face { QPolygonF polygon; double depth; NodeId node; };
    std::vector<Face> faces;
    for (std::size_t meshIndex = 0; meshIndex < document_->mesh_count(); ++meshIndex) {
        const Mesh& mesh = document_->mesh(meshIndex);
        if (mesh.node_id == kInvalidNode || !matrices.contains(mesh.node_id)) continue;
        const Mat4& matrix = matrices.at(mesh.node_id);
        for (const auto& triangle : mesh.triangles) {
            if (triangle[0] >= mesh.positions.size() || triangle[1] >= mesh.positions.size() ||
                triangle[2] >= mesh.positions.size()) continue;
            const auto a = project(transformed(matrix, mesh.positions[triangle[0]]));
            const auto b = project(transformed(matrix, mesh.positions[triangle[1]]));
            const auto c = project(transformed(matrix, mesh.positions[triangle[2]]));
            faces.push_back({QPolygonF{a.point,b.point,c.point},
                             (a.depth+b.depth+c.depth)/3.0, mesh.node_id});
        }
    }
    std::sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) {
        return a.depth < b.depth;
    });
    for (const Face& face : faces) {
        const bool selected = face.node == selected_;
        painter.setBrush(selected ? QColor(255,174,66,110) : QColor(83,162,255,65));
        painter.setPen(QPen(selected ? QColor("#ffae42") : QColor("#74b9ff"),
                            selected ? 1.8 : 0.8));
        painter.drawPolygon(face.polygon);
    }
    painter.setPen(QColor("#d8dee9"));
    painter.drawText(14, 24, QString("PERSPECTIVE  |  Frame %1").arg(frame_, 0, 'f', 1));
    if (document_->node_count() == 0)
        painter.drawText(rect(), Qt::AlignCenter, "Create a mesh from the Modeling toolbar");
}

NodeId AnimateViewport::hitTest(const QPointF& point) const {
    if (!document_) return kInvalidNode;
    const auto matrices = document_->world_matrices(document_->timeline().frame_to_time(frame_));
    NodeId best = kInvalidNode;
    double bestArea = std::numeric_limits<double>::max();
    for (std::size_t index = 0; index < document_->mesh_count(); ++index) {
        const Mesh& mesh = document_->mesh(index);
        if (!matrices.contains(mesh.node_id)) continue;
        QRectF bounds;
        bool first = true;
        for (const Vec3& p : mesh.positions) {
            const QPointF projected = project(transformed(matrices.at(mesh.node_id), p)).point;
            if (first) { bounds = QRectF(projected, QSizeF(1,1)); first = false; }
            else bounds = bounds.united(QRectF(projected, QSizeF(1,1)));
        }
        bounds.adjust(-5,-5,5,5);
        const double area = bounds.width()*bounds.height();
        if (!first && bounds.contains(point) && area < bestArea) { best = mesh.node_id; bestArea = area; }
    }
    return best;
}

void AnimateViewport::mousePressEvent(QMouseEvent* event) {
    lastMouse_ = pressMouse_ = event->position().toPoint();
    dragButton_ = event->button();
}
void AnimateViewport::mouseMoveEvent(QMouseEvent* event) {
    if (dragButton_ == Qt::NoButton) return;
    const QPoint current = event->position().toPoint();
    const QPoint delta = current-lastMouse_; lastMouse_ = current;
    if (dragButton_ == Qt::LeftButton) {
        yaw_ += delta.x()*0.008;
        pitch_ = std::clamp(pitch_ + delta.y()*0.008, -1.45, 1.45);
    } else pan_ += QPointF(delta);
    update();
}
void AnimateViewport::mouseReleaseEvent(QMouseEvent* event) {
    if (dragButton_ == Qt::LeftButton &&
        (event->position().toPoint()-pressMouse_).manhattanLength() < 4) {
        selected_ = hitTest(event->position());
        if (selectionChanged) selectionChanged(selected_);
    }
    dragButton_ = Qt::NoButton;
    update();
}
void AnimateViewport::wheelEvent(QWheelEvent* event) {
    zoom_ = std::clamp(zoom_*std::pow(1.0015, event->angleDelta().y()), 5.0, 2000.0);
    update();
}

} // namespace mydesk::animate::ui
