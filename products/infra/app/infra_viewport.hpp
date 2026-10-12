// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "mydesk/infra/infra_document.hpp"
#include <QPoint>
#include <QWidget>

class InfraViewport final : public QWidget {
    Q_OBJECT
public:
    enum class Kind { None, Road, Bridge, Building };
    explicit InfraViewport(QWidget* parent = nullptr);
    void setDocument(const mydesk::infra::InfraDocument* document);
    void setSelection(Kind kind, std::uint64_t id);
    void fitModel();
signals:
    void selected(InfraViewport::Kind kind, std::uint64_t id);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
private:
    struct Projected { QPointF point; double depth; };
    Projected project(double e, double n, double z) const;
    void pick(const QPointF& point);
    const mydesk::infra::InfraDocument* document_{nullptr};
    Kind kind_{Kind::None}; std::uint64_t id_{0};
    double centerE_{0}, centerN_{0}, centerZ_{0}, zoom_{1}, yaw_{-0.65}, pitch_{0.75};
    QPoint last_; bool navigating_{false};
};
Q_DECLARE_METATYPE(InfraViewport::Kind)
