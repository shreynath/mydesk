#include "mydesk/modify/modify_ops.hpp"

#include "mydesk/geometry/segment.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace mydesk::modify {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1e-9;

struct LinearSeg {
    document::EntityId id{document::kInvalidEntityId};
    geometry::Segment seg;
    bool fromLine{true};
};

std::vector<LinearSeg> collectSegments(const document::Document& doc,
                                       const std::vector<document::EntityId>& onlyIds) {
    std::vector<LinearSeg> out;
    auto want = [&](document::EntityId id) {
        if (onlyIds.empty()) {
            return true;
        }
        return std::find(onlyIds.begin(), onlyIds.end(), id) != onlyIds.end();
    };
    for (const auto& entity : doc.entities()) {
        if (!want(entity.id)) {
            continue;
        }
        if (const auto* line = std::get_if<document::LineEntity>(&entity.geometry)) {
            out.push_back({entity.id, {line->start, line->end}, true});
        } else if (const auto* pl = std::get_if<document::PolylineEntity>(&entity.geometry)) {
            const std::size_t n = document::polylineSegmentCount(*pl);
            for (std::size_t i = 0; i < n; ++i) {
                out.push_back({entity.id,
                               {document::polylineSegmentStart(*pl, i),
                                document::polylineSegmentEnd(*pl, i)},
                               false});
            }
        }
    }
    return out;
}

std::optional<LinearSeg> pickClosestSegment(const document::Document& doc, geometry::Vec2 pick,
                                            document::EntityId* outEntity) {
    double best = std::numeric_limits<double>::infinity();
    std::optional<LinearSeg> bestSeg;
    document::EntityId bestId = document::kInvalidEntityId;
    for (const auto& entity : doc.entities()) {
        if (const auto* line = std::get_if<document::LineEntity>(&entity.geometry)) {
            geometry::Segment seg{line->start, line->end};
            const double d = seg.distanceToPoint(pick);
            if (d < best) {
                best = d;
                bestSeg = LinearSeg{entity.id, seg, true};
                bestId = entity.id;
            }
        } else if (const auto* pl = std::get_if<document::PolylineEntity>(&entity.geometry)) {
            const std::size_t n = document::polylineSegmentCount(*pl);
            for (std::size_t i = 0; i < n; ++i) {
                geometry::Segment seg{document::polylineSegmentStart(*pl, i),
                                      document::polylineSegmentEnd(*pl, i)};
                const double d = seg.distanceToPoint(pick);
                if (d < best) {
                    best = d;
                    bestSeg = LinearSeg{entity.id, seg, false};
                    bestId = entity.id;
                }
            }
        }
    }
    if (outEntity) {
        *outEntity = bestId;
    }
    return bestSeg;
}

geometry::Vec2 unitDirFromCorner(geometry::Segment s, geometry::Vec2 corner) {
    geometry::Vec2 d = s.direction();
    const double len = d.length();
    if (len <= kEps) {
        return {1.0, 0.0};
    }
    d = d / len;
    const double ta = s.projectParam(corner);
    if (std::abs(ta - 0.0) <= std::abs(ta - 1.0)) {
        return d;
    }
    return d * -1.0;
}

document::LineEntity trimLineKeepRay(const document::LineEntity& line, geometry::Vec2 corner,
                                     geometry::Vec2 dir, geometry::Vec2 trimTo) {
    geometry::Vec2 far = line.start;
    const double scoreA =
        (line.start.x - corner.x) * dir.x + (line.start.y - corner.y) * dir.y;
    const double scoreB = (line.end.x - corner.x) * dir.x + (line.end.y - corner.y) * dir.y;
    far = scoreA >= scoreB ? line.start : line.end;
    if (far.distanceTo(trimTo) <= kEps) {
        far = corner + dir * std::max(1.0, line.start.distanceTo(line.end));
    }
    return {trimTo, far};
}

double normAngle(double a) {
    const double twopi = 2.0 * kPi;
    a = std::fmod(a, twopi);
    if (a < 0.0) {
        a += twopi;
    }
    return a;
}

} // namespace

ModifyResult trimAtPoint(document::Document& doc, geometry::Vec2 pick,
                         const std::vector<document::EntityId>& cutters) {
    document::EntityId targetId = document::kInvalidEntityId;
    auto target = pickClosestSegment(doc, pick, &targetId);
    if (!target || targetId == document::kInvalidEntityId) {
        return {false, "TRIM: no object near pick"};
    }

    const auto cutSegs = collectSegments(doc, cutters);
    std::vector<double> hits;
    for (const auto& cutter : cutSegs) {
        if (cutter.id == targetId) {
            continue;
        }
        if (auto isect = geometry::intersectSegments(target->seg, cutter.seg)) {
            hits.push_back(isect->tOnA);
        }
    }
    if (hits.empty()) {
        return {false, "TRIM: no intersection with cutting edge"};
    }
    std::sort(hits.begin(), hits.end());
    hits.erase(std::unique(hits.begin(), hits.end(),
                           [](double a, double b) { return std::abs(a - b) < 1e-8; }),
               hits.end());

    const double tPick = std::clamp(target->seg.projectParam(pick), 0.0, 1.0);
    double left = 0.0;
    double right = 1.0;
    for (double t : hits) {
        if (t < tPick) {
            left = t;
        }
    }
    for (double t : hits) {
        if (t > tPick) {
            right = t;
            break;
        }
    }

    const document::Entity* before = doc.findEntity(targetId);
    if (!before) {
        return {false, "TRIM: target vanished"};
    }

    const bool simpleLine = target->fromLine;
    const auto* pl = std::get_if<document::PolylineEntity>(&before->geometry);
    const bool simplePline = pl && !pl->closed && pl->vertices.size() == 2;
    if (!simpleLine && !simplePline) {
        return {false, "TRIM: multi-segment polyline trim not yet supported"};
    }

    const geometry::Vec2 a = target->seg.a;
    const geometry::Vec2 b = target->seg.b;
    const document::LayerId layerId = before->layerId;
    doc.beginComposite("TRIM");
    doc.compositeRemove(targetId);
    if (left > kEps) {
        document::Entity keep;
        keep.id = doc.allocateEntityIdPublic();
        keep.layerId = layerId;
        keep.geometry = document::LineEntity{a, target->seg.pointAt(left)};
        doc.compositeAdd(keep);
    }
    if (right < 1.0 - kEps) {
        document::Entity keep;
        keep.id = doc.allocateEntityIdPublic();
        keep.layerId = layerId;
        keep.geometry = document::LineEntity{target->seg.pointAt(right), b};
        doc.compositeAdd(keep);
    }
    if (!doc.commitComposite()) {
        return {false, "TRIM: object fully removed"};
    }
    return {true, "TRIM complete"};
}

ModifyResult extendAtPoint(document::Document& doc, geometry::Vec2 pick,
                           const std::vector<document::EntityId>& boundaries) {
    document::EntityId targetId = document::kInvalidEntityId;
    auto target = pickClosestSegment(doc, pick, &targetId);
    if (!target || !target->fromLine) {
        return {false, "EXTEND: pick a LINE near the end to extend"};
    }

    const double tPick = target->seg.projectParam(pick);
    const bool extendStart = tPick < 0.5;
    const auto bounds = collectSegments(doc, boundaries);

    double bestT = extendStart ? std::numeric_limits<double>::infinity()
                               : -std::numeric_limits<double>::infinity();
    bool found = false;
    geometry::Vec2 bestPoint;

    for (const auto& bseg : bounds) {
        if (bseg.id == targetId) {
            continue;
        }
        auto hit = geometry::intersectLines(target->seg, bseg.seg);
        if (!hit) {
            continue;
        }
        if (hit->tOnB < -kEps || hit->tOnB > 1.0 + kEps) {
            continue;
        }
        if (extendStart) {
            if (hit->tOnA < -kEps && hit->tOnA < bestT) {
                bestT = hit->tOnA;
                bestPoint = hit->point;
                found = true;
            }
        } else if (hit->tOnA > 1.0 + kEps && hit->tOnA > bestT) {
            bestT = hit->tOnA;
            bestPoint = hit->point;
            found = true;
        }
    }
    if (!found) {
        return {false, "EXTEND: no boundary intersection"};
    }

    const document::Entity* before = doc.findEntity(targetId);
    if (!before) {
        return {false, "EXTEND: target missing"};
    }
    document::LineEntity line = *std::get_if<document::LineEntity>(&before->geometry);
    if (extendStart) {
        line.start = bestPoint;
    } else {
        line.end = bestPoint;
    }
    if (!doc.replaceEntity(targetId, line, "EXTEND")) {
        return {false, "EXTEND failed"};
    }
    return {true, "EXTEND complete"};
}

ModifyResult offsetEntity(document::Document& doc, document::EntityId id, double distance,
                          geometry::Vec2 sidePoint) {
    if (!(std::abs(distance) > kEps) || !std::isfinite(distance)) {
        return {false, "OFFSET: invalid distance"};
    }
    const document::Entity* entity = doc.findEntity(id);
    if (!entity) {
        return {false, "OFFSET: object not found"};
    }

    if (const auto* line = std::get_if<document::LineEntity>(&entity->geometry)) {
        geometry::Segment seg{line->start, line->end};
        const geometry::Vec2 mid = seg.pointAt(0.5);
        const bool leftSide =
            sidePoint.distanceTo(geometry::offsetPoint(mid, seg.direction(), 1.0)) <=
            sidePoint.distanceTo(geometry::offsetPoint(mid, seg.direction(), -1.0));
        const auto off = geometry::offsetSegment(seg, std::abs(distance), leftSide);
        doc.addLine(off.a, off.b, "OFFSET");
        return {true, "OFFSET line created"};
    }
    if (const auto* circle = std::get_if<document::CircleEntity>(&entity->geometry)) {
        const double side = sidePoint.distanceTo(circle->center) > circle->radius ? 1.0 : -1.0;
        const double r = circle->radius + side * std::abs(distance);
        if (!(r > kEps)) {
            return {false, "OFFSET: resulting radius non-positive"};
        }
        doc.addCircle(circle->center, r, "OFFSET");
        return {true, "OFFSET circle created"};
    }
    if (const auto* pl = std::get_if<document::PolylineEntity>(&entity->geometry)) {
        if (pl->vertices.size() < 2) {
            return {false, "OFFSET: empty polyline"};
        }
        geometry::Segment s0{document::polylineSegmentStart(*pl, 0),
                             document::polylineSegmentEnd(*pl, 0)};
        const geometry::Vec2 mid = s0.pointAt(0.5);
        const bool leftSide =
            sidePoint.distanceTo(geometry::offsetPoint(mid, s0.direction(), 1.0)) <=
            sidePoint.distanceTo(geometry::offsetPoint(mid, s0.direction(), -1.0));
        std::vector<geometry::Vec2> verts;
        const std::size_t n = document::polylineSegmentCount(*pl);
        for (std::size_t i = 0; i < n; ++i) {
            geometry::Segment seg{document::polylineSegmentStart(*pl, i),
                                  document::polylineSegmentEnd(*pl, i)};
            auto off = geometry::offsetSegment(seg, std::abs(distance), leftSide);
            if (verts.empty()) {
                verts.push_back(off.a);
            }
            verts.push_back(off.b);
        }
        doc.addPolyline(std::move(verts), pl->closed, "OFFSET");
        return {true, "OFFSET polyline created"};
    }
    return {false, "OFFSET: unsupported entity type"};
}

ModifyResult filletLines(document::Document& doc, document::EntityId idA, document::EntityId idB,
                         double radius) {
    if (!(radius >= 0.0) || !std::isfinite(radius)) {
        return {false, "FILLET: invalid radius"};
    }
    const document::Entity* ea = doc.findEntity(idA);
    const document::Entity* eb = doc.findEntity(idB);
    if (!ea || !eb) {
        return {false, "FILLET: select two lines"};
    }
    const auto* la = std::get_if<document::LineEntity>(&ea->geometry);
    const auto* lb = std::get_if<document::LineEntity>(&eb->geometry);
    if (!la || !lb) {
        return {false, "FILLET: both objects must be LINE"};
    }
    geometry::Segment sa{la->start, la->end};
    geometry::Segment sb{lb->start, lb->end};
    auto corner = geometry::intersectLines(sa, sb);
    if (!corner) {
        return {false, "FILLET: lines are parallel"};
    }

    const geometry::Vec2 d1 = unitDirFromCorner(sa, corner->point);
    const geometry::Vec2 d2 = unitDirFromCorner(sb, corner->point);

    if (radius <= kEps) {
        document::LineEntity na = trimLineKeepRay(*la, corner->point, d1, corner->point);
        document::LineEntity nb = trimLineKeepRay(*lb, corner->point, d2, corner->point);
    doc.beginComposite("FILLET");
    document::Entity afterA = *ea;
    afterA.geometry = na;
    doc.compositeReplace(*ea, afterA);
    document::Entity afterB = *eb;
    afterB.geometry = nb;
    doc.compositeReplace(*eb, afterB);
    doc.commitComposite();
    return {true, "FILLET radius 0"};
}

    double cosA = std::clamp(d1.x * d2.x + d1.y * d2.y, -1.0, 1.0);
    const double angle = std::acos(cosA);
    if (angle <= kEps || std::abs(angle - kPi) <= kEps) {
        return {false, "FILLET: lines collinear"};
    }
    const double dist = radius / std::tan(angle * 0.5);
    const geometry::Vec2 t1 = corner->point + d1 * dist;
    const geometry::Vec2 t2 = corner->point + d2 * dist;

    geometry::Vec2 n1{-d1.y, d1.x};
    if (n1.x * d2.x + n1.y * d2.y < 0.0) {
        n1 = n1 * -1.0;
    }
    const geometry::Vec2 center = t1 + n1 * radius;
    double start = normAngle(std::atan2(t1.y - center.y, t1.x - center.x));
    double end = normAngle(std::atan2(t2.y - center.y, t2.x - center.x));
    double ccw = end - start;
    if (ccw < 0.0) {
        ccw += 2.0 * kPi;
    }
    if (ccw > kPi) {
        std::swap(start, end);
    }

    document::LineEntity na = trimLineKeepRay(*la, corner->point, d1, t1);
    document::LineEntity nb = trimLineKeepRay(*lb, corner->point, d2, t2);

    const document::LayerId layerId = ea->layerId;
    doc.beginComposite("FILLET");
    document::Entity afterA = *ea;
    afterA.geometry = na;
    doc.compositeReplace(*ea, afterA);
    document::Entity afterB = *eb;
    afterB.geometry = nb;
    doc.compositeReplace(*eb, afterB);
    document::Entity arc;
    arc.id = doc.allocateEntityIdPublic();
    arc.layerId = layerId;
    arc.geometry = document::ArcEntity{center, radius, start, end};
    doc.compositeAdd(arc);
    doc.commitComposite();
    return {true, "FILLET complete"};
}

ModifyResult chamferLines(document::Document& doc, document::EntityId idA, document::EntityId idB,
                          double dist1, double dist2) {
    if (!(dist1 >= 0.0) || !(dist2 >= 0.0) || !std::isfinite(dist1) || !std::isfinite(dist2)) {
        return {false, "CHAMFER: invalid distances"};
    }
    const document::Entity* ea = doc.findEntity(idA);
    const document::Entity* eb = doc.findEntity(idB);
    if (!ea || !eb) {
        return {false, "CHAMFER: select two lines"};
    }
    const auto* la = std::get_if<document::LineEntity>(&ea->geometry);
    const auto* lb = std::get_if<document::LineEntity>(&eb->geometry);
    if (!la || !lb) {
        return {false, "CHAMFER: both objects must be LINE"};
    }
    geometry::Segment sa{la->start, la->end};
    geometry::Segment sb{lb->start, lb->end};
    auto corner = geometry::intersectLines(sa, sb);
    if (!corner) {
        return {false, "CHAMFER: lines are parallel"};
    }

    const geometry::Vec2 d1 = unitDirFromCorner(sa, corner->point);
    const geometry::Vec2 d2 = unitDirFromCorner(sb, corner->point);
    const geometry::Vec2 p1 = corner->point + d1 * dist1;
    const geometry::Vec2 p2 = corner->point + d2 * dist2;

    document::LineEntity na = trimLineKeepRay(*la, corner->point, d1, p1);
    document::LineEntity nb = trimLineKeepRay(*lb, corner->point, d2, p2);

    const document::LayerId layerId = ea->layerId;
    doc.beginComposite("CHAMFER");
    document::Entity afterA = *ea;
    afterA.geometry = na;
    doc.compositeReplace(*ea, afterA);
    document::Entity afterB = *eb;
    afterB.geometry = nb;
    doc.compositeReplace(*eb, afterB);
    document::Entity chamfer;
    chamfer.id = doc.allocateEntityIdPublic();
    chamfer.layerId = layerId;
    chamfer.geometry = document::LineEntity{p1, p2};
    doc.compositeAdd(chamfer);
    doc.commitComposite();
    return {true, "CHAMFER complete"};
}

namespace {

geometry::Vec2 mirrorPt(geometry::Vec2 p, geometry::Vec2 p1, geometry::Vec2 p2) {
    geometry::Vec2 d = p2 - p1;
    double lenSq = d.x * d.x + d.y * d.y;
    if (lenSq < 1e-12) return p;
    geometry::Vec2 v = p - p1;
    double t = (v.x * d.x + v.y * d.y) / lenSq;
    geometry::Vec2 proj = p1 + d * t;
    return proj * 2.0 - p;
}

geometry::Vec2 rotatePt(geometry::Vec2 p, geometry::Vec2 center, double rad) {
    double cosA = std::cos(rad);
    double sinA = std::sin(rad);
    geometry::Vec2 v = p - center;
    return {center.x + v.x * cosA - v.y * sinA, center.y + v.x * sinA + v.y * cosA};
}

double normAngleDeg(double deg) {
    while (deg < 0.0) deg += 360.0;
    while (deg >= 360.0) deg -= 360.0;
    return deg;
}

document::Entity transformEntity(const document::Entity& src, document::EntityId newId,
                                 const std::function<geometry::Vec2(geometry::Vec2)>& xform) {
    document::Entity ent = src;
    ent.id = newId;
    if (const auto* l = std::get_if<document::LineEntity>(&src.geometry)) {
        ent.geometry = document::LineEntity{xform(l->start), xform(l->end)};
    } else if (const auto* c = std::get_if<document::CircleEntity>(&src.geometry)) {
        ent.geometry = document::CircleEntity{xform(c->center), c->radius};
    } else if (const auto* a = std::get_if<document::ArcEntity>(&src.geometry)) {
        ent.geometry = document::ArcEntity{xform(a->center), a->radius, a->startAngle, a->endAngle};
    } else if (const auto* pl = std::get_if<document::PolylineEntity>(&src.geometry)) {
        std::vector<geometry::Vec2> vts;
        vts.reserve(pl->vertices.size());
        for (const auto& pt : pl->vertices) vts.push_back(xform(pt));
        ent.geometry = document::PolylineEntity{std::move(vts), pl->closed};
    } else if (const auto* t = std::get_if<document::TextEntity>(&src.geometry)) {
        ent.geometry = document::TextEntity{xform(t->position), t->height, t->rotationDeg, t->contents};
    } else if (const auto* dim = std::get_if<document::DimensionLinearEntity>(&src.geometry)) {
        ent.geometry = document::DimensionLinearEntity{xform(dim->start), xform(dim->end),
                                                       xform(dim->dimLine)};
    } else if (const auto* ins = std::get_if<document::InsertEntity>(&src.geometry)) {
        document::InsertEntity ni = *ins;
        ni.insertion = xform(ins->insertion);
        ent.geometry = ni;
    }
    return ent;
}

} // namespace

ModifyResult mirrorEntities(document::Document& doc, const std::vector<document::EntityId>& ids,
                            geometry::Vec2 p1, geometry::Vec2 p2, bool eraseSource) {
    if (std::hypot(p2.x - p1.x, p2.y - p1.y) < 1e-9) {
        return {false, "MIRROR: mirror line points must be distinct"};
    }
    if (ids.empty()) {
        return {false, "MIRROR: no objects selected"};
    }

    doc.beginComposite("MIRROR");
    std::size_t count = 0;
    double axisAngleDeg = std::atan2(p2.y - p1.y, p2.x - p1.x) * 180.0 / kPi;

    for (auto id : ids) {
        const auto* ent = doc.findEntity(id);
        if (!ent) continue;

        auto xform = [&](geometry::Vec2 p) { return mirrorPt(p, p1, p2); };
        document::Entity mir = transformEntity(*ent, doc.allocateEntityIdPublic(), xform);

        if (const auto* a = std::get_if<document::ArcEntity>(&ent->geometry)) {
            double ns = normAngleDeg(2.0 * axisAngleDeg - a->endAngle);
            double ne = normAngleDeg(2.0 * axisAngleDeg - a->startAngle);
            mir.geometry = document::ArcEntity{mirrorPt(a->center, p1, p2), a->radius, ns, ne};
        }

        doc.compositeAdd(mir);
        ++count;

        if (eraseSource) {
            doc.compositeRemove(id);
        }
    }
    doc.commitComposite();
    return {true, "MIRROR: mirrored " + std::to_string(count) + " object(s)"};
}

ModifyResult arrayRectangular(document::Document& doc, const std::vector<document::EntityId>& ids,
                             int rows, int cols, double rowSpacing, double colSpacing) {
    if (rows <= 0 || cols <= 0) {
        return {false, "ARRAY: rows and cols must be positive"};
    }
    if (ids.empty()) {
        return {false, "ARRAY: no objects selected"};
    }

    doc.beginComposite("ARRAYRECT");
    std::size_t created = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (r == 0 && c == 0) continue;
            geometry::Vec2 offset{c * colSpacing, r * rowSpacing};
            auto xform = [offset](geometry::Vec2 p) { return p + offset; };
            for (auto id : ids) {
                const auto* ent = doc.findEntity(id);
                if (!ent) continue;
                document::Entity copy = transformEntity(*ent, doc.allocateEntityIdPublic(), xform);
                doc.compositeAdd(copy);
                ++created;
            }
        }
    }
    doc.commitComposite();
    return {true, "ARRAYRECT: created " + std::to_string(created) + " objects"};
}

ModifyResult arrayPolar(document::Document& doc, const std::vector<document::EntityId>& ids,
                        geometry::Vec2 center, int itemCount, double totalAngleDeg) {
    if (itemCount <= 1) {
        return {false, "ARRAYPOLAR: item count must be at least 2"};
    }
    if (ids.empty()) {
        return {false, "ARRAYPOLAR: no objects selected"};
    }

    doc.beginComposite("ARRAYPOLAR");
    std::size_t created = 0;
    double stepDeg = totalAngleDeg / static_cast<double>(itemCount);
    for (int i = 1; i < itemCount; ++i) {
        double rad = (stepDeg * static_cast<double>(i)) * kPi / 180.0;
        auto xform = [center, rad](geometry::Vec2 p) { return rotatePt(p, center, rad); };
        for (auto id : ids) {
            const auto* ent = doc.findEntity(id);
            if (!ent) continue;
            document::Entity copy = transformEntity(*ent, doc.allocateEntityIdPublic(), xform);
            if (const auto* a = std::get_if<document::ArcEntity>(&copy.geometry)) {
                double ns = normAngleDeg(a->startAngle + stepDeg * static_cast<double>(i));
                double ne = normAngleDeg(a->endAngle + stepDeg * static_cast<double>(i));
                copy.geometry = document::ArcEntity{a->center, a->radius, ns, ne};
            }
            doc.compositeAdd(copy);
            ++created;
        }
    }
    doc.commitComposite();
    return {true, "ARRAYPOLAR: created " + std::to_string(created) + " objects"};
}

ModifyResult stretchEntities(document::Document& doc, const std::vector<document::EntityId>& ids,
                             geometry::Vec2 winMin, geometry::Vec2 winMax,
                             geometry::Vec2 displacement) {
    double minX = std::min(winMin.x, winMax.x);
    double maxX = std::max(winMin.x, winMax.x);
    double minY = std::min(winMin.y, winMax.y);
    double maxY = std::max(winMin.y, winMax.y);

    auto inWin = [minX, maxX, minY, maxY](geometry::Vec2 p) {
        return p.x >= minX - 1e-6 && p.x <= maxX + 1e-6 &&
               p.y >= minY - 1e-6 && p.y <= maxY + 1e-6;
    };

    doc.beginComposite("STRETCH");
    std::size_t modified = 0;
    for (auto id : ids) {
        const auto* ent = doc.findEntity(id);
        if (!ent) continue;

        if (const auto* l = std::get_if<document::LineEntity>(&ent->geometry)) {
            geometry::Vec2 s = l->start;
            geometry::Vec2 e = l->end;
            bool changed = false;
            if (inWin(s)) { s = s + displacement; changed = true; }
            if (inWin(e)) { e = e + displacement; changed = true; }
            if (changed) {
                document::Entity repl = *ent;
                repl.geometry = document::LineEntity{s, e};
                doc.compositeReplace(*ent, repl);
                ++modified;
            }
        } else if (const auto* pl = std::get_if<document::PolylineEntity>(&ent->geometry)) {
            std::vector<geometry::Vec2> vts = pl->vertices;
            bool changed = false;
            for (auto& v : vts) {
                if (inWin(v)) { v = v + displacement; changed = true; }
            }
            if (changed) {
                document::Entity repl = *ent;
                repl.geometry = document::PolylineEntity{vts, pl->closed};
                doc.compositeReplace(*ent, repl);
                ++modified;
            }
        }
    }
    doc.commitComposite();
    return {true, "STRETCH: modified " + std::to_string(modified) + " objects"};
}

} // namespace mydesk::modify

