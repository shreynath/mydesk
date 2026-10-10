#pragma once

#include "mydesk/document/document.hpp"
#include "mydesk/geometry/vec2.hpp"

#include <optional>
#include <string>

namespace mydesk::draft {

struct SnapSettings {
    bool endpoint{true};
    bool midpoint{true};
    bool center{true};
    bool intersection{true};
    bool perpendicular{true};
    bool grid{false};
    double gridSpacing{10.0};
    double aperture{8.0}; // model units
    // When set, perpendicular snaps from this point onto nearby geometry.
    std::optional<geometry::Vec2> fromPoint;
};

struct SnapResult {
    geometry::Vec2 point;
    std::string kind;
    document::EntityId entityId{document::kInvalidEntityId};
};

[[nodiscard]] std::optional<SnapResult> findSnap(const document::Document& doc, geometry::Vec2 cursor,
                                                 const SnapSettings& settings);

} // namespace mydesk::draft
