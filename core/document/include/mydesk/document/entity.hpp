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
    Insert,
    Text,
    Hatch,
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

struct TextEntity {
    geometry::Vec2 position;
    double height{2.5};
    double rotationDeg{0.0};
    std::string contents;
};

// Solid fill hatch bounded by a closed polyline (model-space vertices).
struct HatchEntity {
    std::vector<geometry::Vec2> boundary; // >= 3, treated as closed
    std::uint32_t fillArgb{0x6688AACC};
};

// Block reference. localBounds is the definition AABB (block space) for fast bounds.
struct InsertEntity {
    std::string blockName;
    geometry::Vec2 insertion;
    double scale{1.0};
    double rotationDeg{0.0};
    geometry::BBox localBounds;
};

using EntityGeometry =
    std::variant<LineEntity, CircleEntity, ArcEntity, PolylineEntity, DimensionLinearEntity,
                 InsertEntity, TextEntity, HatchEntity>;

// Geometry stored in block-definition coordinates (relative to block origin).
// Must not nest InsertEntity (enforced at BLOCK time).
struct BlockPrimitive {
    EntityGeometry geometry;
    std::uint32_t colorArgb{kColorByLayer};
};

struct BlockDefinition {
    std::string name;
    std::vector<BlockPrimitive> primitives;
};

enum class Linetype : std::uint8_t {
    ByLayer = 0,
    Continuous,
    Dashed,
    Hidden,
    Center,
    Phantom,
    Dot,
};

struct Entity {
    EntityId id{kInvalidEntityId};
    LayerId layerId{kInvalidLayerId};
    std::uint32_t colorArgb{kColorByLayer};
    Linetype linetype{Linetype::ByLayer};
    bool lineweightByLayer{true};
    double lineweightMm{0.25};
    float transparency{1.0f};
    bool transparencyByLayer{true};
    double thickness{0.0};
    bool thicknessByLayer{true};
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
    // Frozen is independent of visible: a thawed layer that is off stays off.
    bool frozen{false};
    // Stored for plot configuration. Display does not depend on this flag.
    bool plottable{true};
    std::uint32_t colorArgb{0xFFE0E0E0};
    Linetype linetype{Linetype::Continuous};
    double lineweightMm{0.25};
    float transparency{1.0f};
    double thickness{0.0};
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
[[nodiscard]] geometry::Vec2 transformInsertPoint(const InsertEntity& insert, geometry::Vec2 local);
[[nodiscard]] EntityGeometry transformInsertGeometry(const InsertEntity& insert,
                                                     const EntityGeometry& local);
[[nodiscard]] geometry::BBox boundsOfGeometry(const EntityGeometry& geometry);
[[nodiscard]] std::string_view entityKindName(EntityKind kind);
[[nodiscard]] std::string_view linetypeName(Linetype type);
[[nodiscard]] bool parseLinetype(std::string_view text, Linetype& out);
// AutoCAD-style pattern in drawing units. Positive = dash, negative = gap. Empty means continuous.
[[nodiscard]] std::vector<double> linetypeDashPattern(Linetype type);
[[nodiscard]] bool validLineweightMm(double millimeters);
[[nodiscard]] std::size_t polylineSegmentCount(const PolylineEntity& pl);
[[nodiscard]] geometry::Vec2 polylineSegmentStart(const PolylineEntity& pl, std::size_t i);
[[nodiscard]] geometry::Vec2 polylineSegmentEnd(const PolylineEntity& pl, std::size_t i);
[[nodiscard]] geometry::Vec2 arcPointAt(const ArcEntity& arc, double angle);
[[nodiscard]] double normalizeAngle(double radians);
[[nodiscard]] bool angleOnArc(const ArcEntity& arc, double angle, double eps = 1e-9);

} // namespace mydesk::document
