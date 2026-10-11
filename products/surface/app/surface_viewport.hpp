// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "mydesk/surface/surface_document.hpp"

#include <QPoint>
#include <QWidget>

class SurfaceViewport final : public QWidget {
    Q_OBJECT
public:
    enum class EntityKind { None, Curve, Surface };

    explicit SurfaceViewport(QWidget* parent = nullptr);
    void setDocument(const mydesk::surface::SurfaceDocument* document);
    void setSelection(EntityKind kind, std::uint64_t id, int controlPoint = -1);
    void setControlNetVisible(bool visible);
    void setZebraVisible(bool visible);
    [[nodiscard]] EntityKind selectionKind() const { return kind_; }
    [[nodiscard]] std::uint64_t selectionId() const { return id_; }

signals:
    void entitySelected(SurfaceViewport::EntityKind kind, std::uint64_t id, int controlPoint);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct ScreenPoint { QPointF point; double depth{0.0}; };
    [[nodiscard]] ScreenPoint project(const mydesk::surface::Vec3& point) const;
    void frameDocument();
    void pick(const QPoint& position);

    const mydesk::surface::SurfaceDocument* document_{nullptr};
    EntityKind kind_{EntityKind::None};
    std::uint64_t id_{0};
    int controlPoint_{-1};
    bool controlNet_{true};
    bool zebra_{false};
    double yaw_{-0.72};
    double pitch_{0.56};
    double zoom_{5.0};
    mydesk::surface::Vec3 center_{};
    QPoint lastMouse_;
    Qt::MouseButton navigationButton_{Qt::NoButton};
};

Q_DECLARE_METATYPE(SurfaceViewport::EntityKind)
