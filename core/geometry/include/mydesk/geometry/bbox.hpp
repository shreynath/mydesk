#pragma once

#include "mydesk/geometry/vec2.hpp"

#include <algorithm>
#include <limits>

namespace mydesk::geometry {

struct BBox {
    double minX{std::numeric_limits<double>::infinity()};
    double minY{std::numeric_limits<double>::infinity()};
    double maxX{-std::numeric_limits<double>::infinity()};
    double maxY{-std::numeric_limits<double>::infinity()};

    [[nodiscard]] bool isValid() const { return minX <= maxX && minY <= maxY; }

    void expand(Vec2 p) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }

    void expand(const BBox& o) {
        if (!o.isValid()) {
            return;
        }
        expand(Vec2{o.minX, o.minY});
        expand(Vec2{o.maxX, o.maxY});
    }

    [[nodiscard]] bool contains(Vec2 p, double tol = 0.0) const {
        return p.x >= minX - tol && p.x <= maxX + tol && p.y >= minY - tol && p.y <= maxY + tol;
    }

    [[nodiscard]] Vec2 center() const { return {(minX + maxX) * 0.5, (minY + maxY) * 0.5}; }
};

[[nodiscard]] BBox lineBBox(Vec2 a, Vec2 b);
[[nodiscard]] BBox circleBBox(Vec2 center, double radius);

} // namespace mydesk::geometry
