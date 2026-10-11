#pragma once

#include <QPoint>
#include <QVector3D>
#include <QWidget>
#include <TopoDS_Shape.hxx>
#include <vector>

namespace mydesk::mechanical::desktop {

class ShapeViewport final : public QWidget {
    Q_OBJECT
public:
    explicit ShapeViewport(QWidget* parent = nullptr);
    void setShape(const TopoDS_Shape& shape);
    void fitAll();
    void setBodySelected(bool selected);

signals:
    void bodyPicked();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct Triangle { QVector3D a, b, c; };
    void rebuildMesh();
    [[nodiscard]] QVector3D rotated(const QVector3D& value) const;

    TopoDS_Shape shape_;
    std::vector<Triangle> triangles_;
    QPoint lastMouse_;
    QVector3D center_;
    float modelRadius_{1.0F};
    float yaw_{-35.0F};
    float pitch_{25.0F};
    float zoom_{1.0F};
    bool bodySelected_{false};
};

} // namespace mydesk::mechanical::desktop
