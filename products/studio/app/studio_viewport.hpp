// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "mydesk/studio/studio_document.hpp"

#include <QPoint>
#include <QPointF>
#include <QString>
#include <QWidget>

namespace mydesk::studio::ui {

class StudioViewport final : public QWidget {
    Q_OBJECT

public:
    explicit StudioViewport(QWidget* parent = nullptr);

    void setDocument(StudioDocument* document);
    void setSelectedObject(ObjectId id);
    [[nodiscard]] ObjectId selectedObject() const noexcept { return selectedObject_; }

public slots:
    void frameAll();

signals:
    void objectSelected(mydesk::studio::ObjectId id);
    void cameraChanged(const QString& description);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct ProjectedPoint {
        QPointF point;
        double depth{};
    };

    [[nodiscard]] ProjectedPoint project(const Vec3& point) const;
    [[nodiscard]] ObjectId hitObject(const QPointF& point) const;
    [[nodiscard]] Vec3 sceneCenter() const;
    [[nodiscard]] double sceneExtent() const;

    StudioDocument* document_{nullptr};
    ObjectId selectedObject_{0};
    double yaw_{-0.65};
    double pitch_{0.48};
    double zoom_{24.0};
    QPointF pan_{};
    QPoint lastMouse_{};
    QPoint pressMouse_{};
    Qt::MouseButton dragButton_{Qt::NoButton};
};

} // namespace mydesk::studio::ui
