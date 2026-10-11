// SPDX-License-Identifier: MPL-2.0
// MyDesk Mill — CAM engine implementation
#include "mydesk/mill/mill_document.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace mydesk::mill {

namespace {

constexpr double epsilon = 1e-9;

bool finite(double value) noexcept { return std::isfinite(value); }

double cross(const Vec2& a, const Vec2& b, const Vec2& c) noexcept {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

double signed_area(const std::vector<Vec2>& polygon) noexcept {
    double twice_area = 0.0;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const Vec2& a = polygon[i];
        const Vec2& b = polygon[(i + 1) % polygon.size()];
        twice_area += a.x * b.y - b.x * a.y;
    }
    return twice_area * 0.5;
}

std::vector<Vec2> normalize_convex(std::vector<Vec2> polygon) {
    if (polygon.size() < 3) {
        throw std::invalid_argument("machining boundary needs at least three vertices");
    }
    for (const Vec2& point : polygon) {
        if (!finite(point.x) || !finite(point.y)) {
            throw std::invalid_argument("machining boundary coordinates must be finite");
        }
    }
    const double area = signed_area(polygon);
    if (std::abs(area) <= epsilon) throw std::invalid_argument("machining boundary has zero area");
    if (area < 0.0) std::reverse(polygon.begin(), polygon.end());
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        if (cross(polygon[i], polygon[(i + 1) % polygon.size()],
                  polygon[(i + 2) % polygon.size()]) <= epsilon) {
            throw std::invalid_argument("machining boundary must be strictly convex");
        }
    }
    return polygon;
}

bool point_in_convex(const std::vector<Vec2>& polygon, const Vec2& point) noexcept {
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        if (cross(polygon[i], polygon[(i + 1) % polygon.size()], point) < -epsilon) return false;
    }
    return true;
}

std::vector<Vec2> offset_convex(const std::vector<Vec2>& polygon, double offset) {
    if (!finite(offset) || offset < -epsilon) {
        throw std::invalid_argument("cutter offset must be finite and non-negative");
    }
    if (offset <= epsilon) return polygon;

    struct Line { Vec2 point; Vec2 direction; };
    std::vector<Line> lines;
    lines.reserve(polygon.size());
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const Vec2 a = polygon[i];
        const Vec2 b = polygon[(i + 1) % polygon.size()];
        const Vec2 direction{b.x - a.x, b.y - a.y};
        const double length = std::hypot(direction.x, direction.y);
        if (length <= epsilon) throw std::invalid_argument("machining boundary has a zero-length edge");
        const Vec2 inward{-direction.y / length, direction.x / length};
        lines.push_back({{a.x + inward.x * offset, a.y + inward.y * offset}, direction});
    }

    std::vector<Vec2> result;
    result.reserve(lines.size());
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const Line& previous = lines[(i + lines.size() - 1) % lines.size()];
        const Line& current = lines[i];
        const double denominator = previous.direction.x * current.direction.y -
                                   previous.direction.y * current.direction.x;
        if (std::abs(denominator) <= epsilon) {
            throw std::invalid_argument("cutter does not fit the selected boundary");
        }
        const Vec2 delta{current.point.x - previous.point.x,
                         current.point.y - previous.point.y};
        const double parameter = (delta.x * current.direction.y -
                                  delta.y * current.direction.x) / denominator;
        result.push_back({previous.point.x + parameter * previous.direction.x,
                          previous.point.y + parameter * previous.direction.y});
    }
    if (signed_area(result) <= epsilon) {
        throw std::invalid_argument("cutter does not fit the selected boundary");
    }
    for (const Vec2& point : result) {
        if (!point_in_convex(polygon, point)) {
            throw std::invalid_argument("cutter does not fit the selected boundary");
        }
        for (const Line& line : lines) {
            const Vec2 line_end{line.point.x + line.direction.x,
                                line.point.y + line.direction.y};
            if (cross(line.point, line_end, point) < -epsilon) {
                throw std::invalid_argument("cutter does not fit the selected boundary");
            }
        }
    }
    return result;
}

std::vector<double> depth_levels(const OperationParams& params) {
    std::vector<double> levels;
    for (double z = params.z_top_mm - params.stepdown_mm;
         z > params.z_bottom_mm + epsilon; z -= params.stepdown_mm) {
        levels.push_back(z);
    }
    levels.push_back(params.z_bottom_mm);
    return levels;
}

std::pair<double, double> scanline_span(const std::vector<Vec2>& polygon, double y) {
    std::vector<double> intersections;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const Vec2& a = polygon[i];
        const Vec2& b = polygon[(i + 1) % polygon.size()];
        if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
            const double parameter = (y - a.y) / (b.y - a.y);
            intersections.push_back(a.x + parameter * (b.x - a.x));
        }
    }
    if (intersections.size() < 2) {
        throw std::runtime_error("scanline does not intersect selected machining boundary");
    }
    const auto bounds = std::minmax_element(intersections.begin(), intersections.end());
    return {*bounds.first, *bounds.second};
}

void append_closed_path(Toolpath& toolpath, const std::vector<Vec2>& polygon, double z,
                        double safe_z, double feed, double rpm) {
    const Vec2 start = polygon.front();
    toolpath.moves.push_back({MoveKind::Rapid, {start.x, start.y, safe_z}, {}, 0.0, rpm});
    toolpath.moves.push_back({MoveKind::PlungeEntry, {start.x, start.y, z}, {}, feed * 0.3, rpm});
    for (std::size_t i = 1; i <= polygon.size(); ++i) {
        const Vec2& point = polygon[i % polygon.size()];
        toolpath.moves.push_back({MoveKind::Linear, {point.x, point.y, z}, {}, feed, rpm});
    }
    toolpath.moves.push_back({MoveKind::Retract, {start.x, start.y, safe_z}, {}, 0.0, rpm});
}

} // namespace

double Toolpath::total_cut_length_mm() const noexcept {
    double length = 0.0;
    for (std::size_t i = 1; i < moves.size(); ++i) {
        if (moves[i].kind == MoveKind::Linear || moves[i].kind == MoveKind::PlungeEntry) {
            const double dx = moves[i].pos.x - moves[i - 1].pos.x;
            const double dy = moves[i].pos.y - moves[i - 1].pos.y;
            const double dz = moves[i].pos.z - moves[i - 1].pos.z;
            length += std::sqrt(dx * dx + dy * dy + dz * dz);
        }
    }
    return length;
}

double Toolpath::estimated_time_min() const noexcept {
    double minutes = 0.0;
    for (std::size_t i = 1; i < moves.size(); ++i) {
        const ToolMove& move = moves[i];
        const double dx = move.pos.x - moves[i - 1].pos.x;
        const double dy = move.pos.y - moves[i - 1].pos.y;
        const double dz = move.pos.z - moves[i - 1].pos.z;
        const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        const double feed = move.feed_mm_min > 0.0 ? move.feed_mm_min : 10000.0;
        minutes += distance / feed;
    }
    return minutes;
}

MillDocument::MillDocument() {
    stock_.min_corner = {0.0, 0.0, -10.0};
    stock_.max_corner = {100.0, 100.0, 0.0};
}

std::size_t MillDocument::add_tool(Tool tool) {
    if (tool.name.empty() || !finite(tool.diameter_mm) || tool.diameter_mm <= 0.0 ||
        !finite(tool.flutes) || tool.flutes <= 0.0 || !finite(tool.cutting_length_mm) ||
        tool.cutting_length_mm <= 0.0 || !finite(tool.surface_speed_m_min) ||
        tool.surface_speed_m_min <= 0.0 || !finite(tool.feed_per_tooth_mm) ||
        tool.feed_per_tooth_mm <= 0.0) {
        throw std::invalid_argument("tool geometry and cutting data must be positive and finite");
    }
    tools_.push_back(std::move(tool));
    return tools_.size() - 1;
}

void MillDocument::set_stock(StockBox stock) {
    const bool valid = finite(stock.min_corner.x) && finite(stock.min_corner.y) &&
                       finite(stock.min_corner.z) && finite(stock.max_corner.x) &&
                       finite(stock.max_corner.y) && finite(stock.max_corner.z) &&
                       stock.min_corner.x < stock.max_corner.x &&
                       stock.min_corner.y < stock.max_corner.y &&
                       stock.min_corner.z < stock.max_corner.z;
    if (!valid) throw std::invalid_argument("stock must be a finite, non-empty box");
    stock_ = stock;
}

std::size_t MillDocument::add_boundary(MachiningBoundary boundary) {
    boundary.polygon = normalize_convex(std::move(boundary.polygon));
    for (const Vec2& point : boundary.polygon) {
        if (point.x < stock_.min_corner.x - epsilon || point.x > stock_.max_corner.x + epsilon ||
            point.y < stock_.min_corner.y - epsilon || point.y > stock_.max_corner.y + epsilon) {
            throw std::invalid_argument("machining boundary must lie within stock XY extents");
        }
    }
    boundaries_.push_back(std::move(boundary));
    return boundaries_.size() - 1;
}

MillDocument::FeedSpeed MillDocument::calc_feed_speed(std::size_t tool_index) const {
    const Tool& tool = tools_.at(tool_index);
    constexpr double pi = 3.141592653589793238462643383279502884;
    const double rpm = (tool.surface_speed_m_min * 1000.0) / (pi * tool.diameter_mm);
    return {rpm, rpm * tool.flutes * tool.feed_per_tooth_mm};
}

std::vector<Vec2> MillDocument::selected_boundary(const OperationParams& params,
                                                   double cutter_offset_mm) const {
    std::vector<Vec2> polygon;
    if (params.boundary_index) {
        polygon = boundaries_.at(*params.boundary_index).polygon;
    } else {
        polygon = {{stock_.min_corner.x, stock_.min_corner.y},
                   {stock_.max_corner.x, stock_.min_corner.y},
                   {stock_.max_corner.x, stock_.max_corner.y},
                   {stock_.min_corner.x, stock_.max_corner.y}};
    }
    return offset_convex(polygon, cutter_offset_mm);
}

void MillDocument::validate_operation(const OperationParams& params) const {
    if (params.tool_index >= tools_.size()) throw std::out_of_range("operation tool index is invalid");
    const Tool& tool = tools_[params.tool_index];
    if (!finite(params.z_top_mm) || !finite(params.z_bottom_mm) || !finite(params.safe_z_mm) ||
        !finite(params.stepdown_mm) || params.stepdown_mm <= 0.0 ||
        params.z_top_mm <= params.z_bottom_mm ||
        params.z_top_mm > stock_.max_corner.z + epsilon ||
        params.z_bottom_mm < stock_.min_corner.z - epsilon ||
        params.safe_z_mm <= params.z_top_mm || !finite(params.stepover_mm) ||
        params.stepover_mm < 0.0 || !finite(params.tolerance_mm) ||
        params.tolerance_mm <= 0.0) {
        throw std::invalid_argument("operation depths, step sizes, tolerance, or safe Z are invalid");
    }
    if (params.z_top_mm - params.z_bottom_mm > tool.cutting_length_mm + epsilon) {
        throw std::invalid_argument("cut depth exceeds the tool cutting length");
    }
    if (params.boundary_index && *params.boundary_index >= boundaries_.size()) {
        throw std::out_of_range("operation boundary index is invalid");
    }
    if (params.strategy == Strategy::Drilling) {
        if (params.drill_points.empty()) {
            throw std::invalid_argument("drilling requires selected hole centres");
        }
        const std::vector<Vec2> permitted = selected_boundary(params, tool.diameter_mm * 0.5);
        for (const Vec2& point : params.drill_points) {
            if (!finite(point.x) || !finite(point.y) || !point_in_convex(permitted, point)) {
                throw std::invalid_argument("a selected drill centre does not provide cutter clearance");
            }
        }
    } else {
        (void)selected_boundary(params, tool.diameter_mm * 0.5);
    }
}

Toolpath MillDocument::generate_raster(const OperationParams& params) const {
    const Tool& tool = tools_.at(params.tool_index);
    const FeedSpeed cutting = calc_feed_speed(params.tool_index);
    const double stepover = params.stepover_mm > 0.0 ? params.stepover_mm : tool.diameter_mm * 0.5;
    const std::vector<Vec2> boundary = selected_boundary(params, tool.diameter_mm * 0.5);
    const auto y_bounds = std::minmax_element(boundary.begin(), boundary.end(),
        [](const Vec2& a, const Vec2& b) { return a.y < b.y; });

    Toolpath toolpath{"Raster", params, {}};
    for (double z : depth_levels(params)) {
        bool forward = true;
        for (double y = y_bounds.first->y + epsilon; y < y_bounds.second->y - epsilon;
             y += stepover) {
            const auto span = scanline_span(boundary, y);
            const double start_x = forward ? span.first : span.second;
            const double end_x = forward ? span.second : span.first;
            toolpath.moves.push_back({MoveKind::Rapid, {start_x, y, params.safe_z_mm}, {},
                                      0.0, cutting.rpm});
            toolpath.moves.push_back({MoveKind::PlungeEntry, {start_x, y, z}, {},
                                      cutting.feed_mm_min * 0.3, cutting.rpm});
            toolpath.moves.push_back({MoveKind::Linear, {end_x, y, z}, {},
                                      cutting.feed_mm_min, cutting.rpm});
            toolpath.moves.push_back({MoveKind::Retract, {end_x, y, params.safe_z_mm}, {},
                                      0.0, cutting.rpm});
            forward = !forward;
        }
    }
    return toolpath;
}

Toolpath MillDocument::generate_parallel_offset(const OperationParams& params) const {
    const Tool& tool = tools_.at(params.tool_index);
    const FeedSpeed cutting = calc_feed_speed(params.tool_index);
    const double stepover = params.stepover_mm > 0.0 ? params.stepover_mm : tool.diameter_mm * 0.5;
    Toolpath toolpath{"ContourParallel", params, {}};
    for (double z : depth_levels(params)) {
        for (std::size_t ring = 0; ring < 10000; ++ring) {
            try {
                const double offset = tool.diameter_mm * 0.5 +
                                      static_cast<double>(ring) * stepover;
                append_closed_path(toolpath, selected_boundary(params, offset), z,
                                   params.safe_z_mm, cutting.feed_mm_min, cutting.rpm);
            } catch (const std::invalid_argument&) {
                break;
            }
        }
    }
    return toolpath;
}

Toolpath MillDocument::generate_pencil(const OperationParams& params) const {
    const Tool& tool = tools_.at(params.tool_index);
    const FeedSpeed cutting = calc_feed_speed(params.tool_index);
    Toolpath toolpath{"Pencil", params, {}};
    append_closed_path(toolpath, selected_boundary(params, tool.diameter_mm * 0.5),
                       params.z_bottom_mm, params.safe_z_mm,
                       cutting.feed_mm_min * 0.5, cutting.rpm);
    return toolpath;
}

Toolpath MillDocument::generate_drilling(const OperationParams& params) const {
    const FeedSpeed cutting = calc_feed_speed(params.tool_index);
    Toolpath toolpath{"Drilling", params, {}};
    for (const Vec2& point : params.drill_points) {
        toolpath.moves.push_back({MoveKind::Rapid, {point.x, point.y, params.safe_z_mm}, {},
                                  0.0, cutting.rpm});
        toolpath.moves.push_back({MoveKind::PlungeEntry,
                                  {point.x, point.y, params.z_bottom_mm}, {},
                                  cutting.feed_mm_min * 0.25, cutting.rpm});
        toolpath.moves.push_back({MoveKind::Dwell,
                                  {point.x, point.y, params.z_bottom_mm}, {}, 0.0, cutting.rpm});
        toolpath.moves.push_back({MoveKind::Retract,
                                  {point.x, point.y, params.safe_z_mm}, {}, 0.0, cutting.rpm});
    }
    return toolpath;
}

Toolpath MillDocument::generate_profile(const OperationParams& params) const {
    const Tool& tool = tools_.at(params.tool_index);
    const FeedSpeed cutting = calc_feed_speed(params.tool_index);
    std::vector<Vec2> boundary = selected_boundary(params, tool.diameter_mm * 0.5);
    if (!params.climb_cut) std::reverse(boundary.begin(), boundary.end());
    Toolpath toolpath{"Profile", params, {}};
    for (double z : depth_levels(params)) {
        append_closed_path(toolpath, boundary, z, params.safe_z_mm,
                           cutting.feed_mm_min, cutting.rpm);
    }
    return toolpath;
}

std::size_t MillDocument::generate(const OperationParams& params) {
    validate_operation(params);
    Toolpath toolpath;
    switch (params.strategy) {
        case Strategy::RasterZigZag: toolpath = generate_raster(params); break;
        case Strategy::ParallelOffset: toolpath = generate_parallel_offset(params); break;
        case Strategy::PencilMilling: toolpath = generate_pencil(params); break;
        case Strategy::Drilling: toolpath = generate_drilling(params); break;
        case Strategy::Profile: toolpath = generate_profile(params); break;
    }
    toolpaths_.push_back(std::move(toolpath));
    return toolpaths_.size() - 1;
}

ToolpathValidation MillDocument::validate_toolpath(std::size_t index) const {
    const Toolpath& path = toolpaths_.at(index);
    ToolpathValidation result;
    bool has_cut = false;
    for (const ToolMove& move : path.moves) {
        if (!finite(move.pos.x) || !finite(move.pos.y) || !finite(move.pos.z)) {
            result.errors.push_back("toolpath contains a non-finite coordinate");
            continue;
        }
        const bool cutting = move.kind == MoveKind::Linear ||
                             move.kind == MoveKind::PlungeEntry ||
                             move.kind == MoveKind::Dwell;
        has_cut = has_cut || cutting;
        if (move.pos.x < stock_.min_corner.x - epsilon ||
            move.pos.x > stock_.max_corner.x + epsilon ||
            move.pos.y < stock_.min_corner.y - epsilon ||
            move.pos.y > stock_.max_corner.y + epsilon) {
            result.errors.push_back("tool centre leaves stock XY extents");
        }
        if (cutting && (move.pos.z < stock_.min_corner.z - epsilon ||
                        move.pos.z > stock_.max_corner.z + epsilon)) {
            result.errors.push_back("cutting move leaves stock Z extents");
        }
    }
    if (!has_cut) result.errors.push_back("toolpath contains no cutting moves");
    result.collision_free = result.errors.empty();
    return result;
}

std::string MillDocument::export_gcode(const GCodeOptions& options) const {
    std::ostringstream output;
    auto format = [&options](double value) {
        std::ostringstream text;
        text.precision(options.decimal_places);
        text << std::fixed << value;
        return text.str();
    };

    output << "%\nO0001 (" << options.machine_name << ")\n";
    if (options.metric) output << "G21  ; metric\n";
    if (options.absolute_mode) output << "G90  ; absolute\n";
    output << "G17  ; XY plane\nG94  ; feed in mm/min\n";
    if (options.use_tool_length_comp) output << "G43 H1 ; tool length comp\n";
    output << '\n';

    for (std::size_t index = 0; index < toolpaths_.size(); ++index) {
        const ToolpathValidation validation = validate_toolpath(index);
        if (!validation.collision_free) {
            throw std::runtime_error("refusing to post an invalid toolpath: " +
                                     validation.errors.front());
        }
        const Toolpath& path = toolpaths_[index];
        const Tool& tool = tools_.at(path.params.tool_index);
        output << "; === Toolpath: " << path.name << " ===\n"
               << 'T' << (path.params.tool_index + 1) << " M06  ; " << tool.name << '\n';
        double last_rpm = -1.0;
        for (const ToolMove& move : path.moves) {
            if (move.spindle_rpm != last_rpm && move.spindle_rpm > 0.0) {
                output << 'S' << static_cast<int>(move.spindle_rpm) << " M03\n";
                last_rpm = move.spindle_rpm;
            }
            switch (move.kind) {
                case MoveKind::Rapid:
                case MoveKind::Retract:
                    output << "G0 X" << format(move.pos.x) << " Y" << format(move.pos.y)
                           << " Z" << format(move.pos.z) << '\n';
                    break;
                case MoveKind::Linear:
                case MoveKind::PlungeEntry:
                    output << "G1 X" << format(move.pos.x) << " Y" << format(move.pos.y)
                           << " Z" << format(move.pos.z) << " F" << format(move.feed_mm_min) << '\n';
                    break;
                case MoveKind::ArcCW:
                case MoveKind::ArcCCW:
                    output << (move.kind == MoveKind::ArcCW ? "G2" : "G3")
                           << " X" << format(move.pos.x) << " Y" << format(move.pos.y)
                           << " Z" << format(move.pos.z) << " I" << format(move.arc_center.x)
                           << " J" << format(move.arc_center.y)
                           << " F" << format(move.feed_mm_min) << '\n';
                    break;
                case MoveKind::Dwell: output << "G4 P500\n"; break;
                case MoveKind::ToolChange: output << "M06\n"; break;
            }
        }
        output << "M09  ; coolant off\n\n";
    }
    output << "M30  ; program end\n%\n";
    return output.str();
}

} // namespace mydesk::mill
