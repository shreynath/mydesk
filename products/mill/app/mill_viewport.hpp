// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "mydesk/mill/mill_document.hpp"

#include <QPoint>
#include <QWidget>

#include <optional>

class MillViewport final : public QWidget {
    Q_OBJECT
public:
    explicit MillViewport(QWidget* parent = nullptr);

    void setDocument(const mydesk::mill::MillDocument* document);
    void setSelectedToolpath(std::optional<std::size_t> index);
    void setTopView(bool topView);
    [[nodiscard]] bool topView() const noexcept { return topView_; }
    void fitView();

signals:
    void toolpathSelected(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QPointF project(const mydesk::mill::Vec3& point) const;
    std::optional<std::size_t> hitToolpath(const QPointF& point) const;

    const mydesk::mill::MillDocument* document_{nullptr};
    std::optional<std::size_t> selectedToolpath_;
    bool topView_{false};
    double yaw_{-35.0};
    double pitch_{28.0};
    double zoom_{1.0};
    QPointF pan_{};
    QPoint pressPos_;
    QPoint lastPos_;
    bool orbiting_{false};
    bool panning_{false};
};
