#include "mydesk/draft/snaps.hpp"

#include "mydesk/geometry/segment.hpp"

#include <cmath>
#include <utility>
#include <vector>

namespace mydesk::draft {

namespace {

struct SegRef {
    geometry::Segment seg;
    document::EntityId id{document::kInvalidEntityId};
};

void collectSegments(const document::Document& doc, std::vector<SegRef>& out) {
    for (const auto& entity : doc.entities()) {
        const auto* layer = doc.findLayer(entity.layerId);
        if (layer && !layer->visible) {
            continue;
        }
        if (const auto* line = std::get_if<document::LineEntity>(&entity.geometry)) {
            out.push_back({{line->start, line->end}, entity.id});
        } else if (const auto* pl = std::get_if<document::PolylineEntity>(&entity.geometry)) {
            const std::size_t n = document::polylineSegmentCount(*pl);
            for (std::size_t i = 0; i < n; ++i) {
                out.push_back({{document::polylineSegmentStart(*pl, i),
                                document::polylineSegmentEnd(*pl, i)},
                               entity.id});
            }
        }
    }
}

} // namespace

std::optional<SnapResult> findSnap(const document::Document& doc, geometry::Vec2 cursor,
                                   const SnapSettings& settings) {
    SnapResult best;
    double bestDist = settings.aperture;
    bool found = false;

    auto consider = [&](geometry::Vec2 p, std::string kind, document::EntityId id) {
        const double d = cursor.distanceTo(p);
        if (d <= bestDist) {
            bestDist = d;
            best = SnapResult{p, std::move(kind), id};
            found = true;
        }
    };

    for (const auto& entity : doc.entities()) {
        const auto* layer = doc.findLayer(entity.layerId);
        if (layer && !layer->visible) {
            continue;
        }
        if (const auto* line = std::get_if<document::LineEntity>(&entity.geometry)) {
            if (settings.endpoint) {
                consider(line->start, "endpoint", entity.id);
                consider(line->end, "endpoint", entity.id);
            }
            if (settings.midpoint) {
                consider((line->start + line->end) * 0.5, "midpoint", entity.id);
            }
        } else if (const auto* circle = std::get_if<document::CircleEntity>(&entity.geometry)) {
            if (settings.center) {
                consider(circle->center, "center", entity.id);
            }
        } else if (const auto* arc = std::get_if<document::ArcEntity>(&entity.geometry)) {
            if (settings.center) {
                consider(arc->center, "center", entity.id);
            }
            if (settings.endpoint) {
                consider(document::arcPointAt(*arc, arc->startAngle), "endpoint", entity.id);
                consider(document::arcPointAt(*arc, arc->endAngle), "endpoint", entity.id);
            }
        } else if (const auto* pl = std::get_if<document::PolylineEntity>(&entity.geometry)) {
            if (settings.endpoint) {
                for (const auto& v : pl->vertices) {
                    consider(v, "endpoint", entity.id);
                }
            }
            if (settings.midpoint) {
                const std::size_t n = document::polylineSegmentCount(*pl);
                for (std::size_t i = 0; i < n; ++i) {
                    const auto a = document::polylineSegmentStart(*pl, i);
                    const auto b = document::polylineSegmentEnd(*pl, i);
                    consider((a + b) * 0.5, "midpoint", entity.id);
                }
            }
        }
    }

    std::vector<SegRef> segs;
    if (settings.intersection || settings.perpendicular) {
        collectSegments(doc, segs);
    }

    if (settings.intersection) {
        for (std::size_t i = 0; i < segs.size(); ++i) {
            for (std::size_t j = i + 1; j < segs.size(); ++j) {
                if (segs[i].id == segs[j].id) {
                    continue;
                }
                if (auto hit = geometry::intersectSegments(segs[i].seg, segs[j].seg)) {
                    consider(hit->point, "intersection", segs[i].id);
                }
            }
        }
    }

    // Perpendicular requires a prior point (command in progress); otherwise it steals endpoint.
    if (settings.perpendicular && settings.fromPoint) {
        const geometry::Vec2 from = *settings.fromPoint;
        for (const auto& ref : segs) {
            const double t = ref.seg.projectParam(from);
            if (t < -1e-6 || t > 1.0 + 1e-6) {
                continue;
            }
            consider(ref.seg.closestPoint(from), "perpendicular", ref.id);
        }
    }

    if (settings.grid) {
        const double g = settings.gridSpacing;
        if (g > 0.0) {
            const geometry::Vec2 gp{std::round(cursor.x / g) * g, std::round(cursor.y / g) * g};
            consider(gp, "grid", document::kInvalidEntityId);
        }
    }

    if (!found) {
        return std::nullopt;
    }
    return best;
}

} // namespace mydesk::draft
