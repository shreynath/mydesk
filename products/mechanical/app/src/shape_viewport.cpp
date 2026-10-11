#include "shape_viewport.hpp"

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopAbs_Orientation.hxx>

#include <QMatrix4x4>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace mydesk::mechanical::desktop {

ShapeViewport::ShapeViewport(QWidget* parent) : QWidget(parent) {
    setMinimumSize(480, 360);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::OpenHandCursor);
}

void ShapeViewport::setShape(const TopoDS_Shape& shape) {
    shape_ = shape;
    rebuildMesh();
    fitAll();
}

void ShapeViewport::setBodySelected(bool selected) {
    bodySelected_ = selected;
    update();
}

void ShapeViewport::fitAll() {
    zoom_ = 1.0F;
    update();
}

void ShapeViewport::rebuildMesh() {
    triangles_.clear();
    if (shape_.IsNull()) return;
    BRepMesh_IncrementalMesh mesh(shape_, 0.35, Standard_False, 0.35, Standard_True);
    mesh.Perform();
    QVector3D minimum;
    QVector3D maximum;
    bool first = true;
    for (TopExp_Explorer explorer(shape_, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const auto face = TopoDS::Face(explorer.Current());
        TopLoc_Location location;
        const auto triangulation = BRep_Tool::Triangulation(face, location);
        if (triangulation.IsNull()) continue;
        for (Standard_Integer index = 1; index <= triangulation->NbTriangles(); ++index) {
            Standard_Integer i1 = 0, i2 = 0, i3 = 0;
            triangulation->Triangle(index).Get(i1, i2, i3);
            if (face.Orientation() == TopAbs_REVERSED) std::swap(i2, i3);
            const auto point = [&](Standard_Integer node) {
                const gp_Pnt p = triangulation->Node(node).Transformed(location.Transformation());
                return QVector3D(static_cast<float>(p.X()), static_cast<float>(p.Y()),
                                 static_cast<float>(p.Z()));
            };
            Triangle triangle{point(i1), point(i2), point(i3)};
            triangles_.push_back(triangle);
            for (const auto& vertex : {triangle.a, triangle.b, triangle.c}) {
                if (first) { minimum = maximum = vertex; first = false; }
                minimum.setX(std::min(minimum.x(), vertex.x()));
                minimum.setY(std::min(minimum.y(), vertex.y()));
                minimum.setZ(std::min(minimum.z(), vertex.z()));
                maximum.setX(std::max(maximum.x(), vertex.x()));
                maximum.setY(std::max(maximum.y(), vertex.y()));
                maximum.setZ(std::max(maximum.z(), vertex.z()));
            }
        }
    }
    if (!first) {
        center_ = (minimum + maximum) * 0.5F;
        modelRadius_ = std::max(0.001F, (maximum - minimum).length() * 0.5F);
    }
}

QVector3D ShapeViewport::rotated(const QVector3D& value) const {
    QMatrix4x4 matrix;
    matrix.rotate(pitch_, 1.0F, 0.0F, 0.0F);
    matrix.rotate(yaw_, 0.0F, 0.0F, 1.0F);
    return matrix.map(value - center_);
}

void ShapeViewport::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(31, 36, 43));

    painter.setPen(QPen(QColor(68, 76, 86), 1));
    const int spacing = 32;
    for (int x = width() / 2 % spacing; x < width(); x += spacing) painter.drawLine(x, 0, x, height());
    for (int y = height() / 2 % spacing; y < height(); y += spacing) painter.drawLine(0, y, width(), y);

    if (triangles_.empty()) {
        painter.setPen(QColor(170, 180, 190));
        painter.drawText(rect(), Qt::AlignCenter,
                         "No evaluated solid\nCreate a Box feature to begin");
        return;
    }

    struct Painted { QPolygonF polygon; float depth; float light; };
    std::vector<Painted> painted;
    painted.reserve(triangles_.size());
    const float scale = 0.82F * static_cast<float>(std::min(width(), height())) /
                        (2.0F * modelRadius_) * zoom_;
    const QPointF origin(width() * 0.5, height() * 0.5);
    for (const auto& triangle : triangles_) {
        const QVector3D a = rotated(triangle.a);
        const QVector3D b = rotated(triangle.b);
        const QVector3D c = rotated(triangle.c);
        const QVector3D normal = QVector3D::crossProduct(b - a, c - a).normalized();
        const float light = std::clamp(QVector3D::dotProduct(normal,
                                        QVector3D(-0.25F, -0.45F, 0.86F)), -1.0F, 1.0F);
        const auto project = [&](const QVector3D& p) {
            return origin + QPointF(static_cast<double>(p.x() * scale),
                                    static_cast<double>(-p.y() * scale));
        };
        painted.push_back({QPolygonF{project(a), project(b), project(c)},
                           (a.z() + b.z() + c.z()) / 3.0F, light});
    }
    std::sort(painted.begin(), painted.end(),
              [](const Painted& left, const Painted& right) { return left.depth < right.depth; });
    const QColor base = bodySelected_ ? QColor(255, 178, 72) : QColor(82, 154, 220);
    for (const auto& triangle : painted) {
        const int factor = static_cast<int>(94.0F + 35.0F * triangle.light);
        painter.setBrush(base.lighter(std::max(60, factor)));
        painter.setPen(QPen(bodySelected_ ? QColor(255, 221, 157) : QColor(22, 57, 83), 0.7));
        painter.drawPolygon(triangle.polygon);
    }
    painter.setPen(QColor(210, 218, 226));
    painter.drawText(12, 22, QString("OCCT B-rep • %1 display triangles").arg(triangles_.size()));
}

void ShapeViewport::mousePressEvent(QMouseEvent* event) {
    lastMouse_ = event->position().toPoint();
    if (event->button() == Qt::LeftButton && !triangles_.empty()) emit bodyPicked();
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)
        setCursor(Qt::ClosedHandCursor);
}

void ShapeViewport::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & (Qt::MiddleButton | Qt::RightButton)) {
        const QPoint delta = event->position().toPoint() - lastMouse_;
        yaw_ += static_cast<float>(delta.x()) * 0.55F;
        pitch_ += static_cast<float>(delta.y()) * 0.55F;
        pitch_ = std::clamp(pitch_, -89.0F, 89.0F);
        lastMouse_ = event->position().toPoint();
        update();
    }
}

void ShapeViewport::wheelEvent(QWheelEvent* event) {
    zoom_ *= std::pow(1.0015F, static_cast<float>(event->angleDelta().y()));
    zoom_ = std::clamp(zoom_, 0.08F, 20.0F);
    update();
}

} // namespace mydesk::mechanical::desktop
