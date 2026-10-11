// SPDX-License-Identifier: MPL-2.0
#include "simulation_viewport.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace mydesk::simulation;

namespace {
QColor contour(double value) {
    value = std::clamp(value, 0.0, 1.0);
    const double r = std::clamp(1.5 - std::abs(4.0 * value - 3.0), 0.0, 1.0);
    const double g = std::clamp(1.5 - std::abs(4.0 * value - 2.0), 0.0, 1.0);
    const double b = std::clamp(1.5 - std::abs(4.0 * value - 1.0), 0.0, 1.0);
    return QColor::fromRgbF(static_cast<float>(r), static_cast<float>(g),
                            static_cast<float>(b), 0.88F);
}

double distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b) {
    const QPointF ab = b - a;
    const double lengthSquared = ab.x()*ab.x() + ab.y()*ab.y();
    if (lengthSquared <= 1e-12) return QLineF(p, a).length();
    const QPointF ap = p - a;
    const double t = std::clamp((ap.x()*ab.x() + ap.y()*ab.y()) / lengthSquared, 0.0, 1.0);
    return QLineF(p, a + ab*t).length();
}
}

SimulationViewport::SimulationViewport(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("simulationViewport"));
    setMinimumSize(520, 420);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
}

void SimulationViewport::setDocument(const SimulationDocument* document) {
    document_ = document;
    staticResult_ = nullptr;
    modalResult_ = nullptr;
    fitView();
}

void SimulationViewport::setStaticResult(const SimulationResult* result) {
    staticResult_ = result;
    if (result && result->converged && document_ && result->max_displacement > 0.0) {
        double extent = 1.0;
        for (std::size_t i=0;i<document_->node_count();++i) {
            const auto& n=document_->node(static_cast<std::uint32_t>(i));
            extent=std::max(extent,std::max(std::abs(n.x-center_.x()),std::abs(n.y-center_.y())));
        }
        deformationScale_ = 0.12 * extent / result->max_displacement;
    }
    update();
}

void SimulationViewport::setModalResult(const ModalResult* result, int mode) {
    modalResult_ = result;
    modalMode_ = mode;
    update();
}

void SimulationViewport::setDisplayMode(DisplayMode mode) { mode_=mode; update(); }
void SimulationViewport::setSelection(bool element, std::uint32_t id) { selectedElement_=element; selectedId_=id; update(); }

void SimulationViewport::fitView() {
    if (!document_ || document_->node_count()==0) { center_={0,0}; zoom_=600.0; update(); return; }
    double minX=std::numeric_limits<double>::max(), minY=minX, maxX=-minX, maxY=-minX;
    for (std::size_t i=0;i<document_->node_count();++i) {
        const auto& n=document_->node(static_cast<std::uint32_t>(i));
        minX=std::min(minX,n.x); minY=std::min(minY,n.y); maxX=std::max(maxX,n.x); maxY=std::max(maxY,n.y);
    }
    center_={(minX+maxX)*0.5,(minY+maxY)*0.5};
    const double extent=std::max({maxX-minX,maxY-minY,1e-3});
    zoom_=0.72*static_cast<double>(std::min(width(),height()))/extent;
    update();
}

QPointF SimulationViewport::project(double x,double y) const {
    return {width()*0.5+(x-center_.x())*zoom_,height()*0.5-(y-center_.y())*zoom_};
}

QPointF SimulationViewport::displaced(std::uint32_t nodeId) const {
    const auto& node=document_->node(nodeId);
    double x=node.x,y=node.y;
    if ((mode_==DisplayMode::Displacement || mode_==DisplayMode::VonMises) && staticResult_ && staticResult_->converged && nodeId<staticResult_->node_results.size()) {
        x+=staticResult_->node_results[nodeId].ux*deformationScale_;
        y+=staticResult_->node_results[nodeId].uy*deformationScale_;
    } else if (mode_==DisplayMode::ModeShape && modalResult_ && modalResult_->converged && modalMode_>=0 && modalMode_<static_cast<int>(modalResult_->modes.size())) {
        const auto& shape=modalResult_->modes[static_cast<std::size_t>(modalMode_)].shape;
        double maximum=0.0; for(const auto& n:shape) maximum=std::max(maximum,n.resultant_displacement);
        if(nodeId<shape.size() && maximum>0.0) { x+=shape[nodeId].ux*0.12/maximum; y+=shape[nodeId].uy*0.12/maximum; }
    }
    return project(x,y);
}

void SimulationViewport::paintEvent(QPaintEvent*) {
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing); painter.fillRect(rect(),QColor(20,25,32));
    painter.setPen(QPen(QColor(45,54,65),1));
    for(int x=width()/2%40;x<width();x+=40)painter.drawLine(x,0,x,height());
    for(int y=height()/2%40;y<height();y+=40)painter.drawLine(0,y,width(),y);
    if(!document_)return;
    const double maxStress=staticResult_?std::max(staticResult_->max_von_mises,1e-20):1.0;
    for(std::size_t i=0;i<document_->element_count();++i){
        const auto& element=document_->element(static_cast<std::uint32_t>(i));
        QPolygonF polygon; for(auto id:element.node_ids) polygon<<displaced(id);
        QColor fill(55,120,155,150);
        if(mode_==DisplayMode::VonMises && staticResult_ && i<staticResult_->element_results.size()) fill=contour(staticResult_->element_results[i].von_mises/maxStress);
        else if(mode_==DisplayMode::Displacement && staticResult_){ double d=0;for(auto id:element.node_ids)d+=staticResult_->node_results[id].resultant_displacement;fill=contour(d/(3.0*std::max(staticResult_->max_displacement,1e-20))); }
        painter.setBrush(fill); painter.setPen(QPen(selectedElement_&&selectedId_==i?QColor(255,208,80):QColor(145,180,205),selectedElement_&&selectedId_==i?3:1)); painter.drawPolygon(polygon);
        painter.setPen(QColor(225,232,240)); painter.drawText(polygon.boundingRect().center(),QStringLiteral("E%1").arg(i));
    }
    for(std::size_t i=0;i<document_->node_count();++i){
        const QPointF p=displaced(static_cast<std::uint32_t>(i));
        painter.setBrush(!selectedElement_&&selectedId_==i?QColor(255,208,80):QColor(224,231,239)); painter.setPen(QColor(15,18,22)); painter.drawEllipse(p,4.5,4.5);
        painter.setPen(QColor(215,222,230)); painter.drawText(p+QPointF(7,-6),QStringLiteral("N%1").arg(i));
    }
    painter.setPen(QColor(190,202,214)); painter.drawText(12,24,QStringLiteral("LMB select · MMB pan · wheel zoom · deformed scale %1×").arg(deformationScale_,0,'g',3));
}

void SimulationViewport::pick(const QPointF& position){
    if(!document_)return; double best=13.0; bool element=false;std::uint32_t id=0;
    for(std::size_t i=0;i<document_->node_count();++i){double d=QLineF(position,displaced(static_cast<std::uint32_t>(i))).length();if(d<best){best=d;element=false;id=static_cast<std::uint32_t>(i);}}
    if(best>=13.0)for(std::size_t i=0;i<document_->element_count();++i){const auto&e=document_->element(static_cast<std::uint32_t>(i));QPolygonF p;for(auto n:e.node_ids)p<<displaced(n);if(p.containsPoint(position,Qt::OddEvenFill)){element=true;id=static_cast<std::uint32_t>(i);best=0;break;}for(int k=0;k<3;++k){double d=distanceToSegment(position,p[k],p[(k+1)%3]);if(d<best){best=d;element=true;id=static_cast<std::uint32_t>(i);}}}
    if(best<13.0){setSelection(element,id);emit entitySelected(element,id);}
}

void SimulationViewport::mousePressEvent(QMouseEvent* event){lastMouse_=event->position().toPoint();if(event->button()==Qt::MiddleButton||event->button()==Qt::RightButton)panning_=true;else if(event->button()==Qt::LeftButton)pick(event->position());}
void SimulationViewport::mouseMoveEvent(QMouseEvent* event){if(!panning_)return;const QPoint now=event->position().toPoint();const QPoint d=now-lastMouse_;lastMouse_=now;center_.rx()-=d.x()/zoom_;center_.ry()+=d.y()/zoom_;update();}
void SimulationViewport::mouseReleaseEvent(QMouseEvent* event){if(event->button()==Qt::MiddleButton||event->button()==Qt::RightButton)panning_=false;}
void SimulationViewport::wheelEvent(QWheelEvent* event){zoom_=std::clamp(zoom_*std::pow(1.0015,event->angleDelta().y()),10.0,100000.0);update();}
