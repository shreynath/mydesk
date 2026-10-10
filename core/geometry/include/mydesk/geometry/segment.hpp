#pragma once

#include "mydesk/geometry/bbox.hpp"
#include "mydesk/geometry/vec2.hpp"

#include <optional>
#include <vector>

namespace mydesk::geometry {

struct Segment {
    Vec2 a;
    Vec2 b;

    [[nodiscard]] Vec2 direction() const { return b - a; }
    [[nodiscard]] double length() const { return a.distanceTo(b); }
    [[nodiscard]] Vec2 pointAt(double t) const { return a + (b - a) * t; }
    [[nodiscard]] BBox bounds() const { return lineBBox(a, b); }

    // Project point onto infinite line; returns parameter t where a=0, b=1.
    [[nodiscard]] double projectParam(Vec2 p) const;
    [[nodiscard]] Vec2 closestPoint(Vec2 p) const;
    [[nodiscard]] double distanceToPoint(Vec2 p) const;
};

struct Intersection {
    Vec2 point;
    double tOnA{0.0}; // param on first segment
    double tOnB{0.0}; // param on second segment
};

// Segment-segment intersection. Includes endpoints within eps.
[[nodiscard]] std::optional<Intersection> intersectSegments(Segment s0, Segment s1,
                                                            double eps = 1e-9);

// Infinite lines through each segment.
[[nodiscard]] std::optional<Intersection> intersectLines(Segment s0, Segment s1, double eps = 1e-9);

[[nodiscard]] Vec2 offsetPoint(Vec2 p, Vec2 along, double distance);
[[nodiscard]] Segment offsetSegment(Segment s, double distance, bool leftSide);

} // namespace mydesk::geometry
