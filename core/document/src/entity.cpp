#include "mydesk/document/entity.hpp"

#include "mydesk/geometry/segment.hpp"

#include <algorithm>
#include <cctype>
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

geometry::Vec2 transformInsertPoint(const InsertEntity& insert, geometry::Vec2 local) {
    const double rad = insert.rotationDeg * kPi / 180.0;
    const double c = std::cos(rad);
    const double s = std::sin(rad);
    const geometry::Vec2 scaled{local.x * insert.scale, local.y * insert.scale};
    const geometry::Vec2 rotated{scaled.x * c - scaled.y * s, scaled.x * s + scaled.y * c};
    return insert.insertion + rotated;
}

geometry::BBox boundsOfGeometry(const EntityGeometry& geometry) {
    Entity temp;
    temp.geometry = geometry;
    return temp.bounds();
}

EntityGeometry transformInsertGeometry(const InsertEntity& insert, const EntityGeometry& local) {
    auto map = [&](geometry::Vec2 p) { return transformInsertPoint(insert, p); };
    if (const auto* line = std::get_if<LineEntity>(&local)) {
        return LineEntity{map(line->start), map(line->end)};
    }
    if (const auto* circle = std::get_if<CircleEntity>(&local)) {
        return CircleEntity{map(circle->center), circle->radius * std::abs(insert.scale)};
    }
    if (const auto* arc = std::get_if<ArcEntity>(&local)) {
        const double rad = insert.rotationDeg * kPi / 180.0;
        return ArcEntity{map(arc->center), arc->radius * std::abs(insert.scale),
                         arc->startAngle + rad, arc->endAngle + rad};
    }
    if (const auto* pl = std::get_if<PolylineEntity>(&local)) {
        PolylineEntity out;
        out.closed = pl->closed;
        out.vertices.reserve(pl->vertices.size());
        for (const auto& v : pl->vertices) {
            out.vertices.push_back(map(v));
        }
        return out;
    }
    if (const auto* dim = std::get_if<DimensionLinearEntity>(&local)) {
        return DimensionLinearEntity{map(dim->start), map(dim->end), map(dim->dimLine)};
    }
    if (const auto* text = std::get_if<TextEntity>(&local)) {
        TextEntity out = *text;
        out.position = map(text->position);
        out.height *= std::abs(insert.scale);
        out.rotationDeg += insert.rotationDeg;
        return out;
    }
    if (const auto* hatch = std::get_if<HatchEntity>(&local)) {
        HatchEntity out = *hatch;
        out.boundary.clear();
        out.boundary.reserve(hatch->boundary.size());
        for (const auto& v : hatch->boundary) {
            out.boundary.push_back(map(v));
        }
        return out;
    }
    return local;
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
    if (std::holds_alternative<InsertEntity>(geometry)) {
        return EntityKind::Insert;
    }
    if (std::holds_alternative<TextEntity>(geometry)) {
        return EntityKind::Text;
    }
    if (std::holds_alternative<HatchEntity>(geometry)) {
        return EntityKind::Hatch;
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
    if (const auto* insert = std::get_if<InsertEntity>(&geometry)) {
        geometry::BBox box;
        if (!insert->localBounds.isValid()) {
            box.expand(insert->insertion);
            return box;
        }
        const geometry::Vec2 corners[] = {
            {insert->localBounds.minX, insert->localBounds.minY},
            {insert->localBounds.maxX, insert->localBounds.minY},
            {insert->localBounds.maxX, insert->localBounds.maxY},
            {insert->localBounds.minX, insert->localBounds.maxY},
        };
        for (const auto& c : corners) {
            box.expand(transformInsertPoint(*insert, c));
        }
        return box;
    }
    if (const auto* text = std::get_if<TextEntity>(&geometry)) {
        geometry::BBox box;
        box.expand(text->position);
        box.expand(geometry::Vec2{
            text->position.x + text->height * static_cast<double>(text->contents.size()) * 0.6,
            text->position.y + text->height});
        return box;
    }
    if (const auto* hatch = std::get_if<HatchEntity>(&geometry)) {
        geometry::BBox box;
        for (const auto& v : hatch->boundary) {
            box.expand(v);
        }
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
    if (std::holds_alternative<InsertEntity>(geometry)) {
        // Precise insert hit-testing is done via Document::hitTestTop using block defs.
        const auto box = bounds();
        return point.x >= box.minX - tol && point.x <= box.maxX + tol &&
               point.y >= box.minY - tol && point.y <= box.maxY + tol;
    }
    if (const auto* text = std::get_if<TextEntity>(&geometry)) {
        return bounds().contains(point) || text->position.distanceTo(point) <= tol;
    }
    if (const auto* hatch = std::get_if<HatchEntity>(&geometry)) {
        // Point-in-polygon (ray cast) or near boundary.
        const auto& b = hatch->boundary;
        if (b.size() < 3) {
            return false;
        }
        bool inside = false;
        for (std::size_t i = 0, j = b.size() - 1; i < b.size(); j = i++) {
            geometry::Segment seg{b[j], b[i]};
            if (seg.distanceToPoint(point) <= tol) {
                return true;
            }
            const bool intersect = ((b[i].y > point.y) != (b[j].y > point.y)) &&
                                   (point.x < (b[j].x - b[i].x) * (point.y - b[i].y) /
                                                     (b[j].y - b[i].y + 1e-30) +
                                                 b[i].x);
            if (intersect) {
                inside = !inside;
            }
        }
        return inside;
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
    if (std::holds_alternative<InsertEntity>(geometry) ||
        std::holds_alternative<TextEntity>(geometry) ||
        std::holds_alternative<HatchEntity>(geometry)) {
        const auto box = bounds();
        if (box.maxX < rect.minX || box.minX > rect.maxX || box.maxY < rect.minY ||
            box.minY > rect.maxY) {
            return false;
        }
        return true;
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
    case EntityKind::Insert:
        return "INSERT";
    case EntityKind::Text:
        return "TEXT";
    case EntityKind::Hatch:
        return "HATCH";
    }
    return "UNKNOWN";
}

std::string_view linetypeName(Linetype type) {
    switch (type) {
    case Linetype::ByLayer:
        return "BYLAYER";
    case Linetype::Continuous:
        return "CONTINUOUS";
    case Linetype::Dashed:
        return "DASHED";
    case Linetype::Hidden:
        return "HIDDEN";
    case Linetype::Center:
        return "CENTER";
    case Linetype::Phantom:
        return "PHANTOM";
    case Linetype::Dot:
        return "DOT";
    }
    return "CONTINUOUS";
}

bool parseLinetype(std::string_view text, Linetype& out) {
    std::string upper;
    upper.reserve(text.size());
    for (char c : text) {
        upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    if (upper == "BYLAYER") {
        out = Linetype::ByLayer;
        return true;
    }
    if (upper == "CONTINUOUS") {
        out = Linetype::Continuous;
        return true;
    }
    if (upper == "DASHED" || upper == "DASH") {
        out = Linetype::Dashed;
        return true;
    }
    if (upper == "HIDDEN") {
        out = Linetype::Hidden;
        return true;
    }
    if (upper == "CENTER") {
        out = Linetype::Center;
        return true;
    }
    if (upper == "PHANTOM") {
        out = Linetype::Phantom;
        return true;
    }
    if (upper == "DOT" || upper == "DOTTED") {
        out = Linetype::Dot;
        return true;
    }
    return false;
}

std::vector<double> linetypeDashPattern(Linetype type) {
    switch (type) {
    case Linetype::Dashed:
        return {8.0, -4.0};
    case Linetype::Hidden:
        return {4.0, -2.0};
    case Linetype::Center:
        return {16.0, -4.0, 2.0, -4.0};
    case Linetype::Phantom:
        return {16.0, -4.0, 2.0, -4.0, 2.0, -4.0};
    case Linetype::Dot:
        return {0.5, -2.0};
    case Linetype::ByLayer:
    case Linetype::Continuous:
        break;
    }
    return {};
}

bool validLineweightMm(double millimeters) {
    return std::isfinite(millimeters) && millimeters >= 0.0 && millimeters <= 2.11;
}

} // namespace mydesk::document
