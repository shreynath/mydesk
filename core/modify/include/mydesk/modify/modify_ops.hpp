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

} // namespace mydesk::modify
