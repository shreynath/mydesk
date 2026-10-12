// SPDX-License-Identifier: MPL-2.0
#include "infra_viewport.hpp"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
mydesk::infra::RoadVertex pointAt(const mydesk::infra::ComponentRoad& road, double station) {
    double cursor = 0.0;
    for (std::size_t i = 1; i < road.centerline.size(); ++i) {
        const auto& a = road.centerline[i - 1]; const auto& b = road.centerline[i];
        const double length = std::hypot(b.easting - a.easting, b.northing - a.northing);
        if (station <= cursor + length || i + 1 == road.centerline.size()) {
            const double t = length > 0.0 ? std::clamp((station - cursor) / length, 0.0, 1.0) : 0.0;
            return {a.easting + t * (b.easting - a.easting),
                    a.northing + t * (b.northing - a.northing),
                    a.elevation + t * (b.elevation - a.elevation)};
        }
        cursor += length;
    }
    return {};
}
}

InfraViewport::InfraViewport(QWidget* parent) : QWidget(parent) {
    setObjectName("infraViewport"); setMinimumSize(560, 360); setMouseTracking(true);
}
void InfraViewport::setDocument(const mydesk::infra::InfraDocument* document) { document_ = document; fitModel(); }
void InfraViewport::setSelection(Kind kind, std::uint64_t id) { kind_ = kind; id_ = id; update(); }

void InfraViewport::fitModel() {
    if (!document_) return;
    const auto& dem = document_->dem();
    if (dem.cols >= 2 && dem.rows >= 2) {
        centerE_ = dem.originEasting + static_cast<double>(dem.cols - 1) * dem.cellSizeMeters * 0.5;
        centerN_ = dem.originNorthing + static_cast<double>(dem.rows - 1) * dem.cellSizeMeters * 0.5;
        if (!dem.elevations.empty()) {
            const auto range = std::minmax_element(dem.elevations.begin(), dem.elevations.end());
            centerZ_ = (*range.first + *range.second) * 0.5;
        }
        const double extent = std::max(static_cast<double>(dem.cols - 1) * dem.cellSizeMeters,
                                       static_cast<double>(dem.rows - 1) * dem.cellSizeMeters);
        zoom_ = 0.7 * static_cast<double>(std::min(width(), height())) / std::max(extent, 1.0);
    } else { centerE_ = centerN_ = centerZ_ = 0.0; zoom_ = 1.0; }
    update();
}

InfraViewport::Projected InfraViewport::project(double e, double n, double z) const {
    const double x=e-centerE_, y=n-centerN_, v=z-centerZ_;
    const double cy=std::cos(yaw_), sy=std::sin(yaw_), cp=std::cos(pitch_), sp=std::sin(pitch_);
    const double rx=cy*x-sy*y, ry=sy*x+cy*y;
    return {{width()*0.5+rx*zoom_, height()*0.5-(cp*v-sp*ry)*zoom_}, sp*v+cp*ry};
}

void InfraViewport::paintEvent(QPaintEvent*) {
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(24,31,36));
    if (!document_) return;
    const auto& dem=document_->dem();
    if (dem.cols>=2 && dem.rows>=2 && dem.elevations.size()==dem.cols*dem.rows) {
        const auto zr=std::minmax_element(dem.elevations.begin(),dem.elevations.end());
        for(std::size_t r=0;r+1<dem.rows;++r) for(std::size_t c=0;c+1<dem.cols;++c){
            auto z=[&](std::size_t rr,std::size_t cc){return dem.elevations[rr*dem.cols+cc];};
            auto p=[&](std::size_t rr,std::size_t cc){return project(dem.originEasting+static_cast<double>(cc)*dem.cellSizeMeters,dem.originNorthing+static_cast<double>(rr)*dem.cellSizeMeters,z(rr,cc)).point;};
            const double avg=(z(r,c)+z(r,c+1)+z(r+1,c+1)+z(r+1,c))/4.0;
            const double t=*zr.second>*zr.first?(avg-*zr.first)/(*zr.second-*zr.first):0.5;
            painter.setBrush(QColor::fromHsv(static_cast<int>(105-45*t),125,static_cast<int>(92+70*t)));
            painter.setPen(QPen(QColor(67,86,80),0.7));
            painter.drawPolygon(QPolygonF{p(r,c),p(r,c+1),p(r+1,c+1),p(r+1,c)});
        }
    }
    for(const auto& building:document_->buildings()){
        QPolygonF base,top;
        for(const auto& q:building.footprint){base<<project(q.first,q.second,building.baseElevation).point;top<<project(q.first,q.second,building.baseElevation+building.roofHeightMeters).point;}
        const bool selected=kind_==Kind::Building&&id_==building.id;
        painter.setPen(QPen(selected?QColor(255,207,82):QColor(158,185,199),selected?3:1.3));
        painter.setBrush(selected?QColor(232,155,52,180):QColor(91,127,146,180)); painter.drawPolygon(top);
        for(int i=0;i<base.size();++i)painter.drawLine(base[i],top[i]);
    }
    for(const auto& road:document_->roads()){
        QPainterPath path;
        for(std::size_t i=0;i<road.centerline.size();++i){const auto& q=road.centerline[i];auto s=project(q.easting,q.northing,q.elevation+0.15).point;if(i==0)path.moveTo(s);else path.lineTo(s);}
        const bool selected=kind_==Kind::Road&&id_==road.id;
        painter.setPen(QPen(selected?QColor(255,210,72):QColor(66,70,73),std::max(3.0,road.numLanes*road.laneWidthMeters*zoom_),Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.setBrush(Qt::NoBrush);painter.drawPath(path);
        painter.setPen(QPen(QColor(244,204,92),1.2,Qt::DashLine));painter.drawPath(path);
    }
    for(const auto& bridge:document_->bridges()) if(const auto* road=document_->findRoad(bridge.roadId)){
        const auto a=pointAt(*road,bridge.startStation),b=pointAt(*road,bridge.endStation);
        painter.setPen(QPen(kind_==Kind::Bridge&&id_==bridge.id?QColor(255,122,72):QColor(205,96,61),6.0));
        painter.drawLine(project(a.easting,a.northing,a.elevation+bridge.deckThicknessMeters).point,project(b.easting,b.northing,b.elevation+bridge.deckThicknessMeters).point);
    }
    painter.setPen(QColor(225,232,235));painter.drawText(12,22,QStringLiteral("Conceptual Model  |  LMB select  MMB orbit  Shift+MMB pan  Wheel zoom"));
}

void InfraViewport::pick(const QPointF& pos){
    if(!document_)return;double best=15;Kind result=Kind::None;std::uint64_t rid=0;
    auto consider=[&](Kind k,std::uint64_t id,double e,double n,double z){auto d=project(e,n,z).point-pos;double distance=std::hypot(d.x(),d.y());if(distance<best){best=distance;result=k;rid=id;}};
    for(const auto& road:document_->roads())for(const auto& p:road.centerline)consider(Kind::Road,road.id,p.easting,p.northing,p.elevation);
    for(const auto& b:document_->buildings())if(!b.footprint.empty()){double e=0,n=0;for(auto q:b.footprint){e+=q.first;n+=q.second;}const double count=static_cast<double>(b.footprint.size());consider(Kind::Building,b.id,e/count,n/count,b.baseElevation+b.roofHeightMeters);}
    for(const auto& bridge:document_->bridges())if(const auto* road=document_->findRoad(bridge.roadId)){auto p=pointAt(*road,(bridge.startStation+bridge.endStation)*0.5);consider(Kind::Bridge,bridge.id,p.easting,p.northing,p.elevation);}
    emit selected(result,rid);
}
void InfraViewport::mousePressEvent(QMouseEvent* e){last_=e->position().toPoint();if(e->button()==Qt::MiddleButton||e->button()==Qt::RightButton)navigating_=true;else if(e->button()==Qt::LeftButton)pick(e->position());}
void InfraViewport::mouseMoveEvent(QMouseEvent* e){if(!navigating_)return;QPoint now=e->position().toPoint(),d=now-last_;last_=now;if(e->modifiers().testFlag(Qt::ShiftModifier)){centerE_-=d.x()/zoom_;centerZ_+=d.y()/zoom_;}else{yaw_+=d.x()*0.008;pitch_=std::clamp(pitch_+d.y()*0.008,0.05,1.5);}update();}
void InfraViewport::mouseReleaseEvent(QMouseEvent* e){if(e->button()==Qt::MiddleButton||e->button()==Qt::RightButton)navigating_=false;}
void InfraViewport::wheelEvent(QWheelEvent* e){zoom_=std::clamp(zoom_*std::pow(1.0015,e->angleDelta().y()),0.002,1000.0);update();}
