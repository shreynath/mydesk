// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "mydesk/fusion/fusion_document.hpp"

#include <QPoint>
#include <QWidget>
#include <functional>

class FusionViewport final : public QWidget {
public:
    explicit FusionViewport(QWidget* parent = nullptr);

    void setDocument(const mydesk::fusion::FusionDocument* document);
    void setSelectedFeature(mydesk::fusion::FeatureId id);
    void fitAll();
    std::function<void(mydesk::fusion::FeatureId)> featurePicked;

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    QPointF project(double x, double y, double z) const;

    const mydesk::fusion::FusionDocument* document_{nullptr};
    mydesk::fusion::FeatureId selected_{0};
    QPoint lastMouse_;
    QPointF pan_;
    double zoom_{1.0};
};
