// SPDX-License-Identifier: MPL-2.0
// MyDesk Mill — PowerMill-class professional CAM engine
// Provides: tool library, stock model, toolpath strategies (raster/parallel/contour/pencil),
//           feed/speed calculation, G-code (RS-274) export.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mydesk::mill {

// ────────────────────────────────────────────────────────────────────────────
// Math
// ────────────────────────────────────────────────────────────────────────────
struct Vec2 { double x{}, y{}; };
struct Vec3 { double x{}, y{}, z{}; };

// ────────────────────────────────────────────────────────────────────────────
// Tool library
// ────────────────────────────────────────────────────────────────────────────
enum class ToolKind : std::uint8_t {
    FlatEndmill,
    BallEndmill,
    BullnosEndmill,
    DrillBit,
    ChamferMill,
    FormTool
};

struct Tool {
    std::string name;
    ToolKind    kind{ToolKind::FlatEndmill};
    double      diameter_mm{6.0};
    double      corner_radius_mm{0.0};   ///< for bullnose
    double      flutes{4};
    double      overall_length_mm{75.0};
    double      cutting_length_mm{30.0};
    double      shank_diameter_mm{6.0};
    // Default feeds/speeds for aluminium (overrideable per operation)
    double      surface_speed_m_min{200.0};
    double      feed_per_tooth_mm{0.05};
};

// ────────────────────────────────────────────────────────────────────────────
// Stock (bounding box block of material)
// ────────────────────────────────────────────────────────────────────────────
struct StockBox {
    Vec3 min_corner{};  ///< lower-left-bottom in mm
    Vec3 max_corner{};  ///< upper-right-top in mm
};

// ────────────────────────────────────────────────────────────────────────────
// Toolpath move types
// ────────────────────────────────────────────────────────────────────────────
enum class MoveKind : std::uint8_t {
    Rapid,       ///< G0
    Linear,      ///< G1
    ArcCW,       ///< G2
    ArcCCW,      ///< G3
    ToolChange,  ///< T__ M06
    PlungeEntry, ///< ramp-in linear
    Retract,     ///< pull-up to safe-Z
    Dwell        ///< G4 Pxxx
};

struct ToolMove {
    MoveKind kind{MoveKind::Rapid};
    Vec3     pos{};      ///< endpoint in WCS (mm)
    Vec3     arc_center{}; ///< only for Arc moves
    double   feed_mm_min{};
    double   spindle_rpm{};
};

// ────────────────────────────────────────────────────────────────────────────
// Toolpath operation descriptor (parameters for one strategy pass)
// ────────────────────────────────────────────────────────────────────────────
enum class Strategy : std::uint8_t {
    RasterZigZag,      ///< face milling — parallel lines, zig-zag
    ParallelOffset,    ///< contour parallel (equidistant offset paths)
    PencilMilling,     ///< pencil finishing in concave fillets
    Drilling,          ///< drill cycle
    Profile,           ///< outer/inner profile (conventional or climb)
};

struct OperationParams {
    Strategy strategy{Strategy::RasterZigZag};
    std::size_t tool_index{};   ///< index into MillDocument::tools
    double stepover_mm{};       ///< lateral stepover; defaults to 0.5*diameter
    double stepdown_mm{3.0};    ///< axial depth per pass
    double z_top_mm{0.0};       ///< top of cut in WCS
    double z_bottom_mm{-5.0};   ///< bottom of cut in WCS
    double safe_z_mm{10.0};     ///< retract height
    double tolerance_mm{0.01};  ///< chordal tolerance for arcs/curves
    bool   climb_cut{true};     ///< climb (up-cut) vs conventional
    double ramp_angle_deg{3.0}; ///< entry ramp angle
};

// ────────────────────────────────────────────────────────────────────────────
// Computed toolpath (one operation)
// ────────────────────────────────────────────────────────────────────────────
struct Toolpath {
    std::string       name;
    OperationParams   params;
    std::vector<ToolMove> moves;

    double total_cut_length_mm() const noexcept;
    double estimated_time_min()  const noexcept;
};

// ────────────────────────────────────────────────────────────────────────────
// G-code post-processor options
// ────────────────────────────────────────────────────────────────────────────
struct GCodeOptions {
    std::string machine_name{"generic_fanuc"};
    bool        use_tool_length_comp{true};   ///< G43
    bool        absolute_mode{true};          ///< G90
    bool        metric{true};                 ///< G21
    int         decimal_places{3};
};

// ────────────────────────────────────────────────────────────────────────────
// Document
// ────────────────────────────────────────────────────────────────────────────
class MillDocument {
public:
    MillDocument();

    // Tool library
    std::size_t add_tool(Tool t);
    Tool&       tool(std::size_t i) { return tools_.at(i); }
    std::size_t tool_count() const noexcept { return tools_.size(); }

    // Stock
    void       set_stock(StockBox s) { stock_ = s; }
    StockBox&  stock() noexcept { return stock_; }

    // Toolpath generation
    std::size_t generate(const OperationParams& params);
    const Toolpath& toolpath(std::size_t i) const { return toolpaths_.at(i); }
    std::size_t toolpath_count() const noexcept { return toolpaths_.size(); }

    // Feed/speed calculator
    // Returns recommended spindle RPM and feed-rate (mm/min) for a tool
    struct FeedSpeed { double rpm; double feed_mm_min; };
    FeedSpeed calc_feed_speed(std::size_t tool_idx) const noexcept;

    // G-code export — all toolpaths
    std::string export_gcode(const GCodeOptions& opts = {}) const;

private:
    std::vector<Tool>       tools_;
    std::vector<Toolpath>   toolpaths_;
    StockBox                stock_;

    Toolpath generate_raster(const OperationParams& p) const;
    Toolpath generate_parallel_offset(const OperationParams& p) const;
    Toolpath generate_pencil(const OperationParams& p) const;
    Toolpath generate_drilling(const OperationParams& p) const;
    Toolpath generate_profile(const OperationParams& p) const;
};

} // namespace mydesk::mill
