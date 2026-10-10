#include "../test_assert.hpp"

#include "mydesk/geometry/bbox.hpp"
#include "mydesk/geometry/vec2.hpp"

void test_vec2_distance() {
    using mydesk::geometry::Vec2;
    const Vec2 a{0, 0};
    const Vec2 b{3, 4};
    expect(a.distanceTo(b) == 5.0, "3-4-5 distance");
    expect(a.nearlyEqual(Vec2{0, 0}), "nearlyEqual");
}

void test_line_bbox() {
    using mydesk::geometry::Vec2;
    const auto box = mydesk::geometry::lineBBox({1, 2}, {4, -1});
    expect(box.minX == 1.0 && box.maxX == 4.0, "bbox x");
    expect(box.minY == -1.0 && box.maxY == 2.0, "bbox y");
}
