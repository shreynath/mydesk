// SPDX-License-Identifier: MPL-2.0
#include "fusion_viewport.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <algorithm>
#include <array>

using mydesk::fusion::SolidBody;

FusionViewport::FusionViewport(QWidget* parent) : QWidget(parent) {
    setMinimumSize(520, 360);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
}

void FusionViewport::setDocument(const mydesk::fusion::FusionDocument* document) {
    document_ = document;
    update();
}

void FusionViewport::setSelectedFeature(mydesk::fusion::FeatureId id) {
    selected_ = id;
    update();
}

void FusionViewport::fitAll() {
    zoom_ = 1.0;
    pan_ = {};
    update();
}

QPointF FusionViewport::project(double x, double y, double z) const {
    double extent = 100.0;
    if (document_ && !document_->bodies().empty()) {
        const auto& b = document_->bodies().front();
        extent = std::max({b.bboxMax[0] - b.bboxMin[0], b.bboxMax[1] - b.bboxMin[1],
                           b.bboxMax[2] - b.bboxMin[2], 1.0});
        x -= (b.bboxMin[0] + b.bboxMax[0]) * 0.5;
        y -= (b.bboxMin[1] + b.bboxMax[1]) * 0.5;
        z -= (b.bboxMin[2] + b.bboxMax[2]) * 0.5;
    }
    const double scale = std::min(width(), height()) * 0.62 * zoom_ / extent;
    return {width() * 0.5 + pan_.x() + (x - y) * 0.78 * scale,
            height() * 0.5 + pan_.y() + (x + y) * 0.34 * scale - z * scale};
}

void FusionViewport::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(31, 37, 43));

    painter.setPen(QPen(QColor(54, 62, 69), 1));
    for (int i = -10; i <= 10; ++i) {
        painter.drawLine(project(-100, i * 10, 0), project(100, i * 10, 0));
        painter.drawLine(project(i * 10, -100, 0), project(i * 10, 100, 0));
    }
    painter.setPen(QPen(QColor(221, 82, 75), 2));
    painter.drawLine(project(0, 0, 0), project(25, 0, 0));
    painter.setPen(QPen(QColor(95, 190, 112), 2));
    painter.drawLine(project(0, 0, 0), project(0, 25, 0));
    painter.setPen(QPen(QColor(87, 147, 224), 2));
    painter.drawLine(project(0, 0, 0), project(0, 0, 25));

    if (!document_ || document_->bodies().empty()) {
        painter.setPen(QColor(170, 180, 188));
        painter.drawText(rect(), Qt::AlignCenter, "Create a sketch, then extrude a solid");
        return;
    }

    for (const SolidBody& body : document_->bodies()) {
        const auto& a = body.bboxMin;
        const auto& b = body.bboxMax;
        std::array<QPointF, 8> p{{project(a[0], a[1], a[2]), project(b[0], a[1], a[2]),
            project(b[0], b[1], a[2]), project(a[0], b[1], a[2]),
            project(a[0], a[1], b[2]), project(b[0], a[1], b[2]),
            project(b[0], b[1], b[2]), project(a[0], b[1], b[2])}};
        QPainterPath top; top.moveTo(p[4]); top.lineTo(p[5]); top.lineTo(p[6]); top.lineTo(p[7]); top.closeSubpath();
        painter.fillPath(top, body.featureId == selected_ ? QColor(255, 181, 71, 155) : QColor(67, 151, 193, 155));
        QPainterPath side; side.moveTo(p[1]); side.lineTo(p[2]); side.lineTo(p[6]); side.lineTo(p[5]); side.closeSubpath();
        painter.fillPath(side, QColor(45, 104, 139, 180));
        painter.setPen(QPen(body.featureId == selected_ ? QColor(255, 198, 92) : QColor(185, 215, 232),
                            body.featureId == selected_ ? 3.0 : 1.5));
        const std::size_t edges[][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        for (const auto& edge : edges) painter.drawLine(p[edge[0]], p[edge[1]]);
    }

    for (const auto& setup : document_->camSetups()) {
        if (setup.toolpath.size() < 2) continue;
        for (std::size_t i = 1; i < setup.toolpath.size(); ++i) {
            const auto& previous = setup.toolpath[i - 1];
            const auto& point = setup.toolpath[i];
            painter.setPen(QPen(point.rapid ? QColor(244, 101, 101, 180) : QColor(85, 231, 168),
                                point.rapid ? 1.0 : 2.0, point.rapid ? Qt::DashLine : Qt::SolidLine));
            painter.drawLine(project(previous.xMm, previous.yMm, previous.zMm),
                             project(point.xMm, point.yMm, point.zMm));
        }
    }

    painter.setPen(QColor(215, 222, 228));
    painter.drawText(14, 24, QString("Bodies %1   CAM paths %2   Timeline %3/%4")
        .arg(document_->bodies().size()).arg(document_->camSetups().size())
        .arg(document_->timelineCursor()).arg(document_->timeline().size()));
}

void FusionViewport::mousePressEvent(QMouseEvent* event) {
    lastMouse_ = event->position().toPoint();
    if (event->button() == Qt::LeftButton && document_ && !document_->bodies().empty()) {
        selected_ = document_->bodies().front().featureId;
        if (featurePicked) featurePicked(selected_);
        update();
    }
}

void FusionViewport::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & (Qt::MiddleButton | Qt::RightButton)) {
        const QPoint delta = event->position().toPoint() - lastMouse_;
        pan_ += QPointF(delta);
        lastMouse_ = event->position().toPoint();
        update();
    }
}

void FusionViewport::wheelEvent(QWheelEvent* event) {
    zoom_ = std::clamp(zoom_ * (event->angleDelta().y() > 0 ? 1.12 : 0.89), 0.15, 12.0);
    update();
}
