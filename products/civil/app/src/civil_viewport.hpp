// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "mydesk/civil/civil_document.hpp"

#include <QPoint>
#include <QWidget>

class CivilViewport final : public QWidget {
    Q_OBJECT
public:
    enum class EntityKind { None, Point, Surface, Alignment, Profile };

    explicit CivilViewport(QWidget* parent = nullptr);
    void setDocument(const mydesk::civil::CivilDocument* document);
    void setSelection(EntityKind kind, std::uint64_t id);
    void setContoursVisible(bool visible);
    void setContourInterval(double interval);
    void fitProject();

signals:
    void entitySelected(CivilViewport::EntityKind kind, std::uint64_t id);
    void coordinateHovered(double easting, double northing);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    [[nodiscard]] QPointF toScreen(double easting, double northing) const;
    [[nodiscard]] std::pair<double, double> toWorld(const QPointF& screen) const;
    void pick(const QPointF& position);

    const mydesk::civil::CivilDocument* document_{nullptr};
    EntityKind selectionKind_{EntityKind::None};
    std::uint64_t selectionId_{0};
    double centerEasting_{0.0};
    double centerNorthing_{0.0};
    double pixelsPerUnit_{1.0};
    bool contoursVisible_{true};
    double contourInterval_{1.0};
    QPoint lastMouse_;
    bool panning_{false};
};

Q_DECLARE_METATYPE(CivilViewport::EntityKind)
