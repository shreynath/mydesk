#pragma once

#include "mydesk/geometry/bbox.hpp"
#include "mydesk/geometry/vec2.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mydesk::document {

using EntityId = std::uint64_t;
using LayerId = std::uint64_t;

inline constexpr EntityId kInvalidEntityId = 0;
inline constexpr LayerId kInvalidLayerId = 0;
// colorArgb == 0 means ByLayer (use owning layer color).
inline constexpr std::uint32_t kColorByLayer = 0;

enum class EntityKind {
    Line,
    Circle,
    Arc,
    Polyline,
    DimensionLinear,
};

struct LineEntity {
    geometry::Vec2 start;
    geometry::Vec2 end;
};

struct CircleEntity {
    geometry::Vec2 center;
    double radius{0.0};
};

// Circular arc, CCW from startAngle to endAngle (radians).
struct ArcEntity {
    geometry::Vec2 center;
    double radius{0.0};
    double startAngle{0.0};
    double endAngle{0.0};
};

struct PolylineEntity {
    std::vector<geometry::Vec2> vertices; // >= 2
    bool closed{false};
};

// Aligned linear dimension: measurement between start/end; dimLine locates the dimension line.
struct DimensionLinearEntity {
    geometry::Vec2 start;
    geometry::Vec2 end;
    geometry::Vec2 dimLine;
};

using EntityGeometry =
    std::variant<LineEntity, CircleEntity, ArcEntity, PolylineEntity, DimensionLinearEntity>;

struct Entity {
    EntityId id{kInvalidEntityId};
    LayerId layerId{kInvalidLayerId};
    std::uint32_t colorArgb{kColorByLayer};
    EntityGeometry geometry;

    [[nodiscard]] EntityKind kind() const;
    [[nodiscard]] geometry::BBox bounds() const;
    [[nodiscard]] bool hitTest(geometry::Vec2 point, double tol) const;
    [[nodiscard]] bool intersectsRect(const geometry::BBox& rect, bool crossing) const;
    [[nodiscard]] bool containedInRect(const geometry::BBox& rect) const;
};

struct Layer {
    LayerId id{kInvalidLayerId};
    std::string name{"0"};
    bool visible{true};
    bool locked{false};
    std::uint32_t colorArgb{0xFFE0E0E0};
};

struct DimensionLayout {
    geometry::Vec2 start;
    geometry::Vec2 end;
    geometry::Vec2 dimStart;
    geometry::Vec2 dimEnd;
    geometry::Vec2 textPos;
    double measurement{0.0};
};

[[nodiscard]] DimensionLayout layoutDimension(const DimensionLinearEntity& dim);
[[nodiscard]] std::string_view entityKindName(EntityKind kind);
[[nodiscard]] std::size_t polylineSegmentCount(const PolylineEntity& pl);
[[nodiscard]] geometry::Vec2 polylineSegmentStart(const PolylineEntity& pl, std::size_t i);
[[nodiscard]] geometry::Vec2 polylineSegmentEnd(const PolylineEntity& pl, std::size_t i);
[[nodiscard]] geometry::Vec2 arcPointAt(const ArcEntity& arc, double angle);
[[nodiscard]] double normalizeAngle(double radians);
[[nodiscard]] bool angleOnArc(const ArcEntity& arc, double angle, double eps = 1e-9);

} // namespace mydesk::document
