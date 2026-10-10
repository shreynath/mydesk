#include "mydesk/geometry/segment.hpp"

#include <algorithm>
#include <cmath>

namespace mydesk::geometry {

double Segment::projectParam(Vec2 p) const {
    const Vec2 ab = b - a;
    const double len2 = ab.lengthSquared();
    if (len2 <= 1e-18) {
        return 0.0;
    }
    return ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / len2;
}

Vec2 Segment::closestPoint(Vec2 p) const {
    const double t = std::clamp(projectParam(p), 0.0, 1.0);
    return pointAt(t);
}

double Segment::distanceToPoint(Vec2 p) const { return p.distanceTo(closestPoint(p)); }

std::optional<Intersection> intersectLines(Segment s0, Segment s1, double eps) {
    const Vec2 r = s0.b - s0.a;
    const Vec2 s = s1.b - s1.a;
    const double rxs = r.x * s.y - r.y * s.x;
    if (std::abs(rxs) <= eps) {
        return std::nullopt; // parallel
    }
    const Vec2 q_p = s1.a - s0.a;
    const double t = (q_p.x * s.y - q_p.y * s.x) / rxs;
    const double u = (q_p.x * r.y - q_p.y * r.x) / rxs;
    return Intersection{s0.a + r * t, t, u};
}

std::optional<Intersection> intersectSegments(Segment s0, Segment s1, double eps) {
    auto hit = intersectLines(s0, s1, eps);
    if (!hit) {
        return std::nullopt;
    }
    if (hit->tOnA < -eps || hit->tOnA > 1.0 + eps || hit->tOnB < -eps || hit->tOnB > 1.0 + eps) {
        return std::nullopt;
    }
    hit->tOnA = std::clamp(hit->tOnA, 0.0, 1.0);
    hit->tOnB = std::clamp(hit->tOnB, 0.0, 1.0);
    hit->point = s0.pointAt(hit->tOnA);
    return hit;
}

Vec2 offsetPoint(Vec2 p, Vec2 along, double distance) {
    const double len = along.length();
    if (len <= 1e-18) {
        return p;
    }
    const Vec2 n{-along.y / len, along.x / len}; // left normal
    return p + n * distance;
}

Segment offsetSegment(Segment s, double distance, bool leftSide) {
    const double sign = leftSide ? 1.0 : -1.0;
    return {offsetPoint(s.a, s.direction(), distance * sign),
            offsetPoint(s.b, s.direction(), distance * sign)};
}

} // namespace mydesk::geometry
