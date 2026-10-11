#pragma once

#include "mydesk/document/document.hpp"
#include "mydesk/geometry/vec2.hpp"

#include <string>
#include <vector>

namespace mydesk::modify {

struct ModifyResult {
    bool ok{false};
    std::string message;
};

[[nodiscard]] ModifyResult trimAtPoint(document::Document& doc, geometry::Vec2 pick,
                                       const std::vector<document::EntityId>& cutters);

[[nodiscard]] ModifyResult extendAtPoint(document::Document& doc, geometry::Vec2 pick,
                                         const std::vector<document::EntityId>& boundaries);

[[nodiscard]] ModifyResult offsetEntity(document::Document& doc, document::EntityId id,
                                        double distance, geometry::Vec2 sidePoint);

[[nodiscard]] ModifyResult filletLines(document::Document& doc, document::EntityId a,
                                       document::EntityId b, double radius);

[[nodiscard]] ModifyResult chamferLines(document::Document& doc, document::EntityId a,
                                        document::EntityId b, double dist1, double dist2);

[[nodiscard]] ModifyResult mirrorEntities(document::Document& doc,
                                         const std::vector<document::EntityId>& ids,
                                         geometry::Vec2 p1, geometry::Vec2 p2,
                                         bool eraseSource = false);

[[nodiscard]] ModifyResult arrayRectangular(document::Document& doc,
                                            const std::vector<document::EntityId>& ids,
                                            int rows, int cols, double rowSpacing,
                                            double colSpacing);

[[nodiscard]] ModifyResult arrayPolar(document::Document& doc,
                                      const std::vector<document::EntityId>& ids,
                                      geometry::Vec2 center, int itemCount,
                                      double totalAngleDeg = 360.0);

[[nodiscard]] ModifyResult stretchEntities(document::Document& doc,
                                           const std::vector<document::EntityId>& ids,
                                           geometry::Vec2 winMin, geometry::Vec2 winMax,
                                           geometry::Vec2 displacement);

} // namespace mydesk::modify
