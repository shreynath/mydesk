#pragma once

#include "mydesk/coordinate/coordinate_document.hpp"

#include <QPoint>
#include <QRectF>
#include <QWidget>

#include <vector>

class CoordinationViewport final : public QWidget {
    Q_OBJECT

public:
    explicit CoordinationViewport(QWidget* parent = nullptr);

    void setDocument(const mydesk::coordinate::CoordinateDocument* document);
    void setSelectedElement(mydesk::coordinate::ElementId id);
    void fitAll();
    void focusPoint(const mydesk::coordinate::Vector3D& point);

signals:
    void elementSelected(qulonglong id);
    void viewChanged(const QString& description);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct HitRegion {
        QRectF rect;
        mydesk::coordinate::ElementId id{0};
        double depth{0.0};
    };

    [[nodiscard]] QPointF project(const mydesk::coordinate::Vector3D& point,
                                  double* depth = nullptr) const;
    [[nodiscard]] std::array<mydesk::coordinate::Vector3D, 8>
    elementVertices(const mydesk::coordinate::CoordinationElement& element) const;
    void recomputeSceneBounds();

    const mydesk::coordinate::CoordinateDocument* document_{nullptr};
    mydesk::coordinate::ElementId selectedElement_{0};
    mydesk::coordinate::Vector3D viewCenter_{};
    double sceneSpan_{10.0};
    double zoom_{1.0};
    double yaw_{-0.65};
    double pitch_{0.55};
    QPointF pan_{};
    QPoint lastMousePosition_{};
    Qt::MouseButton dragButton_{Qt::NoButton};
    std::vector<HitRegion> hitRegions_;
};
