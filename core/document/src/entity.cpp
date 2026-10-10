#include "mydesk/document/entity.hpp"

#include "mydesk/geometry/segment.hpp"

#include <algorithm>
#include <cmath>

namespace mydesk::document {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

double normalizeAngle(double radians) {
    double a = std::fmod(radians, 2.0 * kPi);
    if (a < 0.0) {
        a += 2.0 * kPi;
    }
    return a;
}

geometry::Vec2 arcPointAt(const ArcEntity& arc, double angle) {
    return {arc.center.x + arc.radius * std::cos(angle),
            arc.center.y + arc.radius * std::sin(angle)};
}

bool angleOnArc(const ArcEntity& arc, double angle, double eps) {
    const double a0 = normalizeAngle(arc.startAngle);
    const double a1 = normalizeAngle(arc.endAngle);
    const double a = normalizeAngle(angle);
    if (std::abs(a0 - a1) <= eps) {
        return true; // full circle treated as on-arc
    }
    if (a0 <= a1) {
        return a + eps >= a0 && a - eps <= a1;
    }
    return a + eps >= a0 || a - eps <= a1;
}

std::size_t polylineSegmentCount(const PolylineEntity& pl) {
    if (pl.vertices.size() < 2) {
        return 0;
    }
    return pl.closed ? pl.vertices.size() : pl.vertices.size() - 1;
}

geometry::Vec2 polylineSegmentStart(const PolylineEntity& pl, std::size_t i) {
    return pl.vertices[i % pl.vertices.size()];
}

geometry::Vec2 polylineSegmentEnd(const PolylineEntity& pl, std::size_t i) {
    return pl.vertices[(i + 1) % pl.vertices.size()];
}

DimensionLayout layoutDimension(const DimensionLinearEntity& dim) {
    DimensionLayout layout;
    layout.start = dim.start;
    layout.end = dim.end;
    layout.measurement = dim.start.distanceTo(dim.end);
    geometry::Vec2 dir = dim.end - dim.start;
    const double len = dir.length();
    if (len < 1e-12) {
        layout.dimStart = dim.start;
        layout.dimEnd = dim.end;
        layout.textPos = dim.dimLine;
        return layout;
    }
    dir = dir / len;
    const geometry::Vec2 perp{-dir.y, dir.x};
    const geometry::Vec2 mid = (dim.start + dim.end) * 0.5;
    const geometry::Vec2 toDim = dim.dimLine - mid;
    const double offset = toDim.x * perp.x + toDim.y * perp.y;
    layout.dimStart = dim.start + perp * offset;
    layout.dimEnd = dim.end + perp * offset;
    layout.textPos = (layout.dimStart + layout.dimEnd) * 0.5 + perp * (offset >= 0 ? 2.0 : -2.0);
    return layout;
}

EntityKind Entity::kind() const {
    if (std::holds_alternative<LineEntity>(geometry)) {
        return EntityKind::Line;
    }
    if (std::holds_alternative<CircleEntity>(geometry)) {
        return EntityKind::Circle;
    }
    if (std::holds_alternative<ArcEntity>(geometry)) {
        return EntityKind::Arc;
    }
    if (std::holds_alternative<DimensionLinearEntity>(geometry)) {
        return EntityKind::DimensionLinear;
    }
    return EntityKind::Polyline;
}

geometry::BBox Entity::bounds() const {
    if (const auto* line = std::get_if<LineEntity>(&geometry)) {
        return geometry::lineBBox(line->start, line->end);
    }
    if (const auto* circle = std::get_if<CircleEntity>(&geometry)) {
        return geometry::circleBBox(circle->center, circle->radius);
    }
    if (const auto* arc = std::get_if<ArcEntity>(&geometry)) {
        geometry::BBox box;
        box.expand(arcPointAt(*arc, arc->startAngle));
        box.expand(arcPointAt(*arc, arc->endAngle));
        for (double a : {0.0, kPi * 0.5, kPi, kPi * 1.5}) {
            if (angleOnArc(*arc, a)) {
                box.expand(arcPointAt(*arc, a));
            }
        }
        return box;
    }
    if (const auto* dim = std::get_if<DimensionLinearEntity>(&geometry)) {
        const auto layout = layoutDimension(*dim);
        geometry::BBox box;
        box.expand(layout.start);
        box.expand(layout.end);
        box.expand(layout.dimStart);
        box.expand(layout.dimEnd);
        box.expand(layout.textPos);
        return box;
    }
    const auto& pl = std::get<PolylineEntity>(geometry);
    geometry::BBox box;
    for (const auto& v : pl.vertices) {
        box.expand(v);
    }
    return box;
}

bool Entity::hitTest(geometry::Vec2 point, double tol) const {
    if (const auto* line = std::get_if<LineEntity>(&geometry)) {
        return geometry::Segment{line->start, line->end}.distanceToPoint(point) <= tol;
    }
    if (const auto* circle = std::get_if<CircleEntity>(&geometry)) {
        return std::abs(point.distanceTo(circle->center) - std::abs(circle->radius)) <= tol;
    }
    if (const auto* arc = std::get_if<ArcEntity>(&geometry)) {
        const double dist = point.distanceTo(arc->center);
        if (std::abs(dist - std::abs(arc->radius)) > tol) {
            return false;
        }
        const double ang = std::atan2(point.y - arc->center.y, point.x - arc->center.x);
        return angleOnArc(*arc, ang, 1e-6);
    }
    if (const auto* dim = std::get_if<DimensionLinearEntity>(&geometry)) {
        const auto layout = layoutDimension(*dim);
        if (geometry::Segment{layout.dimStart, layout.dimEnd}.distanceToPoint(point) <= tol) {
            return true;
        }
        if (geometry::Segment{layout.start, layout.dimStart}.distanceToPoint(point) <= tol) {
            return true;
        }
        if (geometry::Segment{layout.end, layout.dimEnd}.distanceToPoint(point) <= tol) {
            return true;
        }
        return layout.textPos.distanceTo(point) <= tol * 2.0;
    }
    const auto& pl = std::get<PolylineEntity>(geometry);
    const std::size_t n = polylineSegmentCount(pl);
    for (std::size_t i = 0; i < n; ++i) {
        geometry::Segment seg{polylineSegmentStart(pl, i), polylineSegmentEnd(pl, i)};
        if (seg.distanceToPoint(point) <= tol) {
            return true;
        }
    }
    return false;
}

bool Entity::containedInRect(const geometry::BBox& rect) const {
    const auto box = bounds();
    if (!box.isValid()) {
        return false;
    }
    return rect.contains({box.minX, box.minY}) && rect.contains({box.maxX, box.minY}) &&
           rect.contains({box.minX, box.maxY}) && rect.contains({box.maxX, box.maxY});
}

namespace {

bool segmentCrossesRect(geometry::Segment seg, const geometry::BBox& rect) {
    if (rect.contains(seg.a) || rect.contains(seg.b)) {
        return true;
    }
    const geometry::Segment edges[] = {
        {{rect.minX, rect.minY}, {rect.maxX, rect.minY}},
        {{rect.maxX, rect.minY}, {rect.maxX, rect.maxY}},
        {{rect.maxX, rect.maxY}, {rect.minX, rect.maxY}},
        {{rect.minX, rect.maxY}, {rect.minX, rect.minY}},
    };
    for (const auto& e : edges) {
        if (geometry::intersectSegments(seg, e)) {
            return true;
        }
    }
    return false;
}

} // namespace

bool Entity::intersectsRect(const geometry::BBox& rect, bool crossing) const {
    if (!crossing) {
        return containedInRect(rect);
    }
    if (const auto* line = std::get_if<LineEntity>(&geometry)) {
        return segmentCrossesRect({line->start, line->end}, rect);
    }
    if (std::holds_alternative<CircleEntity>(geometry) ||
        std::holds_alternative<ArcEntity>(geometry)) {
        const auto box = bounds();
        if (box.maxX < rect.minX || box.minX > rect.maxX || box.maxY < rect.minY ||
            box.minY > rect.maxY) {
            return false;
        }
        return true;
    }
    if (const auto* dim = std::get_if<DimensionLinearEntity>(&geometry)) {
        const auto layout = layoutDimension(*dim);
        return segmentCrossesRect({layout.dimStart, layout.dimEnd}, rect) ||
               segmentCrossesRect({layout.start, layout.dimStart}, rect) ||
               segmentCrossesRect({layout.end, layout.dimEnd}, rect) ||
               rect.contains(layout.textPos);
    }
    const auto& pl = std::get<PolylineEntity>(geometry);
    const std::size_t n = polylineSegmentCount(pl);
    for (std::size_t i = 0; i < n; ++i) {
        if (segmentCrossesRect({polylineSegmentStart(pl, i), polylineSegmentEnd(pl, i)}, rect)) {
            return true;
        }
    }
    return false;
}

std::string_view entityKindName(EntityKind kind) {
    switch (kind) {
    case EntityKind::Line:
        return "LINE";
    case EntityKind::Circle:
        return "CIRCLE";
    case EntityKind::Arc:
        return "ARC";
    case EntityKind::Polyline:
        return "PLINE";
    case EntityKind::DimensionLinear:
        return "DIMLINEAR";
    }
    return "UNKNOWN";
}

} // namespace mydesk::document
