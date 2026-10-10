#include "mydesk/geometry/bbox.hpp"

namespace mydesk::geometry {

BBox lineBBox(Vec2 a, Vec2 b) {
    BBox box;
    box.expand(a);
    box.expand(b);
    return box;
}

BBox circleBBox(Vec2 center, double radius) {
    BBox box;
    const double r = std::abs(radius);
    box.expand(Vec2{center.x - r, center.y - r});
    box.expand(Vec2{center.x + r, center.y + r});
    return box;
}

} // namespace mydesk::geometry
