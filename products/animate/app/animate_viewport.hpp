#pragma once

#include "mydesk/animate/animate_document.hpp"

#include <QPoint>
#include <QWidget>

#include <functional>

namespace mydesk::animate::ui {

class AnimateViewport final : public QWidget {
public:
    explicit AnimateViewport(QWidget* parent = nullptr);
    void setDocument(AnimateDocument* document);
    void setFrame(double frame);
    void setSelectedNode(NodeId id);
    [[nodiscard]] NodeId selectedNode() const { return selected_; }
    std::function<void(NodeId)> selectionChanged;

public slots:
    void frameAll();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    struct Projection { QPointF point; double depth{}; };
    [[nodiscard]] Projection project(const Vec3& point) const;
    [[nodiscard]] Vec3 transformed(const Mat4& matrix, const Vec3& point) const;
    [[nodiscard]] NodeId hitTest(const QPointF& point) const;

    AnimateDocument* document_{nullptr};
    NodeId selected_{kInvalidNode};
    double frame_{0.0};
    double yaw_{-0.65};
    double pitch_{0.45};
    double zoom_{70.0};
    QPointF pan_{};
    QPoint lastMouse_{};
    QPoint pressMouse_{};
    Qt::MouseButton dragButton_{Qt::NoButton};
};

} // namespace mydesk::animate::ui
