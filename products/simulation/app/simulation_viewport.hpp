// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "mydesk/simulation/simulation_document.hpp"

#include <QPoint>
#include <QWidget>

class SimulationViewport final : public QWidget {
    Q_OBJECT
public:
    enum class DisplayMode { Model, Displacement, VonMises, ModeShape };
    explicit SimulationViewport(QWidget* parent = nullptr);
    void setDocument(const mydesk::simulation::SimulationDocument* document);
    void setStaticResult(const mydesk::simulation::SimulationResult* result);
    void setModalResult(const mydesk::simulation::ModalResult* result, int mode = 0);
    void setDisplayMode(DisplayMode mode);
    void setSelection(bool element, std::uint32_t id);
    [[nodiscard]] DisplayMode displayMode() const { return mode_; }

signals:
    void entitySelected(bool element, std::uint32_t id);

public slots:
    void fitView();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    QPointF project(double x, double y) const;
    QPointF displaced(std::uint32_t node) const;
    void pick(const QPointF& position);
    const mydesk::simulation::SimulationDocument* document_{nullptr};
    const mydesk::simulation::SimulationResult* staticResult_{nullptr};
    const mydesk::simulation::ModalResult* modalResult_{nullptr};
    DisplayMode mode_{DisplayMode::Model};
    int modalMode_{0};
    bool selectedElement_{false};
    std::uint32_t selectedId_{0};
    QPoint lastMouse_;
    bool panning_{false};
    QPointF center_{0.0, 0.0};
    double zoom_{600.0};
    double deformationScale_{1.0};
};
