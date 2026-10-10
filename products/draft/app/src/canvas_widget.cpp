#include "canvas_widget.hpp"

#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVector>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace mydesk::desktop {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

CanvasWidget::CanvasWidget(commands::Session& session, QWidget* parent)
    : QWidget(parent), session_(session) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(400, 300);
    snaps_.aperture = 12.0;
}

void CanvasWidget::fitToContent() {
    const auto& entities = session_.document().entities();
    if (entities.empty()) {
        scale_ = 1.0;
        pan_ = {width() * 0.5, height() * 0.5};
        update();
        return;
    }
    geometry::BBox box;
    for (const auto& entity : entities) {
        box.expand(entity.bounds());
    }
    const double pad = 40.0;
    const double w = std::max(1.0, box.maxX - box.minX);
    const double h = std::max(1.0, box.maxY - box.minY);
    scale_ = std::min((width() - pad * 2) / w, (height() - pad * 2) / h);
    scale_ = std::clamp(scale_, 0.05, 200.0);
    const auto center = box.center();
    pan_ = {width() * 0.5 - center.x * scale_, height() * 0.5 + center.y * scale_};
    update();
}

void CanvasWidget::refresh() { update(); }

QPointF CanvasWidget::worldToScreen(geometry::Vec2 world) const {
    return {pan_.x + world.x * scale_, pan_.y - world.y * scale_};
}

geometry::Vec2 CanvasWidget::screenToWorld(QPointF screen) const {
    return {(screen.x() - pan_.x) / scale_, (pan_.y - screen.y()) / scale_};
}

geometry::Vec2 CanvasWidget::resolveWorld(QPointF screen, bool* snapped) {
    auto world = screenToWorld(screen);
    if (snapped) {
        *snapped = false;
    }
    snaps_.grid = session_.modes().gridSnap;
    snaps_.gridSpacing = session_.modes().gridSpacing;
    if (session_.modes().objectSnap || session_.modes().gridSnap) {
        snaps_.aperture = 12.0 / scale_;
        const bool os = session_.modes().objectSnap;
        snaps_.endpoint = os && session_.modes().snapEndpoint;
        snaps_.midpoint = os && session_.modes().snapMidpoint;
        snaps_.center = os && session_.modes().snapCenter;
        snaps_.intersection = os && session_.modes().snapIntersection;
        snaps_.perpendicular = os && session_.modes().snapPerpendicular;
        if (session_.hasActiveCommand() && !session_.context().points().empty()) {
            snaps_.fromPoint = session_.context().points().back();
        } else {
            snaps_.fromPoint.reset();
        }
        hoverSnap_ = draft::findSnap(session_.document(), world, snaps_);
        if (hoverSnap_) {
            world = hoverSnap_->point;
            if (snapped) {
                *snapped = true;
            }
        } else {
            hoverSnap_.reset();
        }
    } else {
        hoverSnap_.reset();
    }
    if (session_.hasActiveCommand()) {
        world = session_.applyOrtho(world);
    }
    return world;
}

void CanvasWidget::drawEntity(QPainter& painter, const document::Entity& entity,
                              const QPen& pen) const {
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    if (const auto* line = std::get_if<document::LineEntity>(&entity.geometry)) {
        painter.drawLine(worldToScreen(line->start), worldToScreen(line->end));
    } else if (const auto* circle = std::get_if<document::CircleEntity>(&entity.geometry)) {
        const auto c = worldToScreen(circle->center);
        const double r = circle->radius * scale_;
        painter.drawEllipse(c, r, r);
    } else if (const auto* arc = std::get_if<document::ArcEntity>(&entity.geometry)) {
        const auto c = worldToScreen(arc->center);
        const double r = arc->radius * scale_;
        // Qt: angles in 1/16 degree, 0=3 o'clock, CCW positive; our angles: 0=+x CCW.
        double startDeg = -arc->startAngle * 180.0 / kPi;
        double endDeg = -arc->endAngle * 180.0 / kPi;
        double span = endDeg - startDeg;
        if (span <= 0.0) {
            span += 360.0;
        }
        painter.drawArc(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r),
                        static_cast<int>(startDeg * 16.0), static_cast<int>(span * 16.0));
    } else if (const auto* pl = std::get_if<document::PolylineEntity>(&entity.geometry)) {
        if (pl->vertices.size() < 2) {
            return;
        }
        QPainterPath path;
        path.moveTo(worldToScreen(pl->vertices.front()));
        for (std::size_t i = 1; i < pl->vertices.size(); ++i) {
            path.lineTo(worldToScreen(pl->vertices[i]));
        }
        if (pl->closed) {
            path.closeSubpath();
        }
        painter.drawPath(path);
    } else if (const auto* dim = std::get_if<document::DimensionLinearEntity>(&entity.geometry)) {
        const auto layout = document::layoutDimension(*dim);
        painter.drawLine(worldToScreen(layout.start), worldToScreen(layout.dimStart));
        painter.drawLine(worldToScreen(layout.end), worldToScreen(layout.dimEnd));
        painter.drawLine(worldToScreen(layout.dimStart), worldToScreen(layout.dimEnd));
        // Arrow ticks
        const auto ds = worldToScreen(layout.dimStart);
        const auto de = worldToScreen(layout.dimEnd);
        const QPointF dir = de - ds;
        const double dlen = std::hypot(dir.x(), dir.y());
        if (dlen > 1.0) {
            const QPointF u = dir / dlen;
            const QPointF n(-u.y(), u.x());
            constexpr double tick = 6.0;
            painter.drawLine(ds, ds + u * tick + n * tick);
            painter.drawLine(ds, ds + u * tick - n * tick);
            painter.drawLine(de, de - u * tick + n * tick);
            painter.drawLine(de, de - u * tick - n * tick);
        }
        const auto textPos = worldToScreen(layout.textPos);
        painter.drawText(textPos, QString::number(layout.measurement, 'f', 2));
    } else if (const auto* text = std::get_if<document::TextEntity>(&entity.geometry)) {
        painter.save();
        const auto pos = worldToScreen(text->position);
        painter.translate(pos);
        painter.rotate(-text->rotationDeg);
        QFont font = painter.font();
        font.setPointSizeF(std::max(6.0, text->height * scale_ * 0.75));
        painter.setFont(font);
        painter.drawText(QPointF(0, 0), QString::fromStdString(text->contents));
        painter.restore();
    } else if (const auto* hatch = std::get_if<document::HatchEntity>(&entity.geometry)) {
        if (hatch->boundary.size() >= 3) {
            QPainterPath path;
            path.moveTo(worldToScreen(hatch->boundary.front()));
            for (std::size_t i = 1; i < hatch->boundary.size(); ++i) {
                path.lineTo(worldToScreen(hatch->boundary[i]));
            }
            path.closeSubpath();
            const auto argb = hatch->fillArgb;
            QColor fill(static_cast<int>((argb >> 16) & 0xFF),
                        static_cast<int>((argb >> 8) & 0xFF), static_cast<int>(argb & 0xFF),
                        static_cast<int>((argb >> 24) & 0xFF));
            painter.fillPath(path, fill);
            painter.strokePath(path, pen);
        }
    } else if (const auto* attdef = std::get_if<document::AttdefEntity>(&entity.geometry)) {
        painter.save();
        const auto pos = worldToScreen(attdef->position);
        painter.translate(pos);
        painter.rotate(-attdef->rotationDeg);
        QFont font = painter.font();
        font.setPointSizeF(std::max(6.0, attdef->height * scale_ * 0.75));
        painter.setFont(font);
        painter.drawText(QPointF(0, 0), QString::fromStdString(attdef->tag));
        painter.restore();
    } else if (const auto* insert = std::get_if<document::InsertEntity>(&entity.geometry)) {
        const auto* block = session_.document().findBlock(insert->blockName);
        if (!block) {
            return;
        }
        for (const auto& prim : block->primitives) {
            document::Entity proxy = entity;
            proxy.colorArgb = prim.colorArgb;
            proxy.geometry = document::transformInsertGeometry(*insert, prim.geometry);
            const auto argb = session_.document().resolvedEntityColor(proxy);
            QColor primColor(static_cast<int>((argb >> 16) & 0xFF),
                             static_cast<int>((argb >> 8) & 0xFF), static_cast<int>(argb & 0xFF),
                             static_cast<int>((argb >> 24) & 0xFF));
            if (pen.color() == QColor(255, 196, 72) || pen.color() == QColor(140, 210, 255)) {
                primColor = pen.color();
            }
            drawEntity(painter, proxy, QPen(primColor, pen.widthF()));
        }
        const auto mode = session_.document().attributeDisplayMode();
        if (mode != document::AttributeDisplayMode::Off) {
            for (const auto& attr : insert->attributes) {
                if (mode == document::AttributeDisplayMode::Normal && attr.invisible) {
                    continue;
                }
                painter.save();
                const auto pos = worldToScreen(attr.position);
                painter.translate(pos);
                painter.rotate(-attr.rotationDeg);
                QFont font = painter.font();
                font.setPointSizeF(std::max(6.0, attr.height * scale_ * 0.75));
                painter.setFont(font);
                painter.setPen(pen);
                painter.drawText(QPointF(0, 0), QString::fromStdString(attr.value));
                painter.restore();
            }
        }
    }
}

void CanvasWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient bg(0, 0, width(), height());
    bg.setColorAt(0.0, QColor(18, 24, 32));
    bg.setColorAt(0.55, QColor(24, 32, 42));
    bg.setColorAt(1.0, QColor(14, 18, 24));
    painter.fillRect(rect(), bg);

    if (session_.modes().gridDisplay) {
        const double grid = session_.modes().gridSpacing;
        painter.setPen(QPen(QColor(55, 68, 82, 90), 1));
        const auto topLeft = screenToWorld(QPointF(0, 0));
        const auto bottomRight = screenToWorld(QPointF(width(), height()));
        const double x0 = std::floor(std::min(topLeft.x, bottomRight.x) / grid) * grid;
        const double x1 = std::ceil(std::max(topLeft.x, bottomRight.x) / grid) * grid;
        const double y0 = std::floor(std::min(topLeft.y, bottomRight.y) / grid) * grid;
        const double y1 = std::ceil(std::max(topLeft.y, bottomRight.y) / grid) * grid;
        for (double x = x0; x <= x1; x += grid) {
            painter.drawLine(worldToScreen({x, y0}), worldToScreen({x, y1}));
        }
        for (double y = y0; y <= y1; y += grid) {
            painter.drawLine(worldToScreen({x0, y}), worldToScreen({x1, y}));
        }
    }

    painter.setPen(QPen(QColor(90, 140, 170, 160), 1.5));
    {
        const auto topLeft = screenToWorld(QPointF(0, 0));
        const auto bottomRight = screenToWorld(QPointF(width(), height()));
        painter.drawLine(worldToScreen({std::min(topLeft.x, bottomRight.x), 0}),
                         worldToScreen({std::max(topLeft.x, bottomRight.x), 0}));
        painter.drawLine(worldToScreen({0, std::min(topLeft.y, bottomRight.y)}),
                         worldToScreen({0, std::max(topLeft.y, bottomRight.y)}));
    }

    const auto& selection = session_.document().selection();
    for (const auto& entity : session_.document().entities()) {
        const auto* layer = session_.document().findLayer(entity.layerId);
        if (layer && !session_.document().isEntityVisible(entity)) {
            continue;
        }
        const bool selected = selection.count(entity.id) != 0U;
        const bool hovered = entity.id == hoverEntity_;
        const auto argb = session_.document().resolvedEntityColor(entity);
        QColor color = QColor(static_cast<int>((argb >> 16) & 0xFF),
                              static_cast<int>((argb >> 8) & 0xFF), static_cast<int>(argb & 0xFF),
                              static_cast<int>((argb >> 24) & 0xFF));
        double width = std::max(1.0, session_.document().resolvedLineweightMm(entity) * 4.0);
        if (selected) {
            color = QColor(255, 196, 72);
            width = std::max(width, 2.2);
        } else if (hovered) {
            color = QColor(140, 210, 255);
            width = std::max(width, 2.0);
        } else if (layer && layer->locked) {
            color = color.darker(125);
        }
        QPen pen(color, width);
        const auto pattern =
            document::linetypeDashPattern(session_.document().resolvedLinetype(entity));
        if (!pattern.empty()) {
            QVector<qreal> dashes;
            const double ltScale = session_.document().linetypeScale();
            for (double segment : pattern) {
                const double pixels = std::abs(segment) * ltScale * scale_ / width;
                dashes.push_back(std::max(0.4, pixels));
            }
            pen.setDashPattern(dashes);
        }
        drawEntity(painter, entity, pen);
    }

    // Command preview rubber-band
    if (session_.hasActiveCommand() && !session_.context().points().empty()) {
        painter.setPen(QPen(QColor(120, 200, 160), 1.2, Qt::DashLine));
        const auto& pts = session_.context().points();
        for (std::size_t i = 1; i < pts.size(); ++i) {
            painter.drawLine(worldToScreen(pts[i - 1]), worldToScreen(pts[i]));
        }
        painter.drawLine(worldToScreen(pts.back()), worldToScreen(cursorWorld_));
    }

    if (boxSelecting_) {
        const QRect rect = QRect(boxOrigin_, boxCurrent_).normalized();
        const bool crossing = boxCurrent_.x() < boxOrigin_.x();
        painter.setPen(QPen(crossing ? QColor(80, 200, 120) : QColor(80, 140, 220), 1.0,
                            crossing ? Qt::DashLine : Qt::SolidLine));
        painter.setBrush(crossing ? QColor(80, 200, 120, 40) : QColor(80, 140, 220, 40));
        painter.drawRect(rect);
    }

    if (hoverSnap_) {
        const auto p = worldToScreen(hoverSnap_->point);
        painter.setPen(QPen(QColor(120, 220, 180), 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(p.x() - 5, p.y() - 5, 10, 10));
        painter.drawText(p + QPointF(8, -8), QString::fromStdString(hoverSnap_->kind));
    }
}

void CanvasWidget::wheelEvent(QWheelEvent* event) {
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    const auto before = screenToWorld(event->position());
    scale_ = std::clamp(scale_ * factor, 0.05, 400.0);
    const auto afterScreen = worldToScreen(before);
    pan_.x += event->position().x() - afterScreen.x();
    pan_.y += event->position().y() - afterScreen.y();
    update();
}

void CanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning_ = true;
        lastMouse_ = event->pos();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        bool snapped = false;
        auto world = resolveWorld(event->position(), &snapped);
        cursorWorld_ = world;
        if (session_.hasActiveCommand()) {
            emit pointCommitted(world.x, world.y, snapped);
            return;
        }
        // Begin potential click or window/crossing drag.
        boxSelecting_ = true;
        boxOrigin_ = event->pos();
        boxCurrent_ = event->pos();
        lastMouse_ = event->pos();
    }
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (panning_) {
        const QPoint delta = event->pos() - lastMouse_;
        pan_.x += delta.x();
        pan_.y += delta.y();
        lastMouse_ = event->pos();
        update();
        return;
    }
    if (boxSelecting_) {
        boxCurrent_ = event->pos();
        update();
        return;
    }

    bool snapped = false;
    cursorWorld_ = resolveWorld(event->position(), &snapped);
    hoverEntity_ = document::kInvalidEntityId;
    if (!session_.hasActiveCommand()) {
        const double tol = 8.0 / scale_;
        hoverEntity_ = session_.document().hitTestTop(cursorWorld_, tol);
    }
    emit cursorMoved(cursorWorld_.x, cursorWorld_.y);
    update();
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning_ = false;
        return;
    }
    if (event->button() == Qt::LeftButton && boxSelecting_) {
        boxSelecting_ = false;
        const QPoint delta = event->pos() - boxOrigin_;
        const bool additive = event->modifiers() & Qt::ShiftModifier;
        const double tol = 8.0 / scale_;

        if (std::hypot(delta.x(), delta.y()) < 4.0) {
            // Click select
            bool snapped = false;
            auto world = resolveWorld(event->position(), &snapped);
            const auto id = session_.document().hitTestTop(world, tol);
            if (!additive) {
                session_.document().clearSelection();
            }
            if (id != document::kInvalidEntityId) {
                if (additive) {
                    session_.document().toggleSelect(id);
                } else {
                    session_.document().select(id);
                }
            }
        } else {
            // Window vs crossing by drag direction (AutoCAD convention).
            const auto a = screenToWorld(boxOrigin_);
            const auto b = screenToWorld(event->pos());
            geometry::BBox rect;
            rect.minX = std::min(a.x, b.x);
            rect.maxX = std::max(a.x, b.x);
            rect.minY = std::min(a.y, b.y);
            rect.maxY = std::max(a.y, b.y);
            const bool crossing = event->pos().x() < boxOrigin_.x();
            session_.document().selectInRect(rect, crossing, additive);
        }
        emit selectionChanged();
        update();
    }
}

void CanvasWidget::resizeEvent(QResizeEvent* event) {
    if (event->oldSize().width() <= 0 || event->oldSize().height() <= 0) {
        pan_ = {width() * 0.5, height() * 0.5};
    }
    QWidget::resizeEvent(event);
}

} // namespace mydesk::desktop
