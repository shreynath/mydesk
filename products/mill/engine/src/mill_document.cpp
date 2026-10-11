// SPDX-License-Identifier: MPL-2.0
// MyDesk Mill — CAM engine implementation
#include "mydesk/mill/mill_document.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

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

void MillDocument::validate_tool(const Tool& tool) {
    if (tool.name.empty() || !finite(tool.diameter_mm) || tool.diameter_mm <= 0.0 ||
        !finite(tool.corner_radius_mm) || tool.corner_radius_mm < 0.0 ||
        tool.corner_radius_mm > tool.diameter_mm * 0.5 + epsilon ||
        !finite(tool.flutes) || tool.flutes <= 0.0 ||
        !finite(tool.overall_length_mm) || tool.overall_length_mm <= 0.0 ||
        !finite(tool.cutting_length_mm) || tool.cutting_length_mm <= 0.0 ||
        tool.cutting_length_mm > tool.overall_length_mm + epsilon ||
        !finite(tool.shank_diameter_mm) || tool.shank_diameter_mm <= 0.0 ||
        !finite(tool.surface_speed_m_min) || tool.surface_speed_m_min <= 0.0 ||
        !finite(tool.feed_per_tooth_mm) || tool.feed_per_tooth_mm <= 0.0) {
        throw std::invalid_argument("tool geometry and cutting data must be positive, finite, and consistent");
    }
}

void MillDocument::validate_stock(const StockBox& stock) {
    const bool valid = finite(stock.min_corner.x) && finite(stock.min_corner.y) &&
                       finite(stock.min_corner.z) && finite(stock.max_corner.x) &&
                       finite(stock.max_corner.y) && finite(stock.max_corner.z) &&
                       stock.min_corner.x < stock.max_corner.x &&
                       stock.min_corner.y < stock.max_corner.y &&
                       stock.min_corner.z < stock.max_corner.z;
    if (!valid) throw std::invalid_argument("stock must be a finite, non-empty box");
}

MillDocument::Snapshot MillDocument::capture() const {
    return {name_, tools_, boundaries_, toolpaths_, stock_};
}

void MillDocument::restore(Snapshot snapshot) {
    name_ = std::move(snapshot.name);
    tools_ = std::move(snapshot.tools);
    boundaries_ = std::move(snapshot.boundaries);
    toolpaths_ = std::move(snapshot.toolpaths);
    stock_ = snapshot.stock;
}

void MillDocument::checkpoint() {
    undo_.push_back(capture());
    redo_.clear();
}

bool MillDocument::undo() {
    if (undo_.empty()) return false;
    redo_.push_back(capture());
    Snapshot snapshot = std::move(undo_.back());
    undo_.pop_back();
    restore(std::move(snapshot));
    return true;
}

bool MillDocument::redo() {
    if (redo_.empty()) return false;
    undo_.push_back(capture());
    Snapshot snapshot = std::move(redo_.back());
    redo_.pop_back();
    restore(std::move(snapshot));
    return true;
}

void MillDocument::set_name(std::string name) {
    if (name.empty()) throw std::invalid_argument("project name cannot be empty");
    if (name == name_) return;
    checkpoint();
    name_ = std::move(name);
}

std::size_t MillDocument::add_tool(Tool tool) {
    validate_tool(tool);
    checkpoint();
    tools_.push_back(std::move(tool));
    return tools_.size() - 1;
}

bool MillDocument::set_tool(std::size_t index, Tool tool) {
    if (index >= tools_.size()) return false;
    validate_tool(tool);
    checkpoint();
    tools_[index] = std::move(tool);
    // Tool geometry or cutting data invalidates every dependent calculated path.
    toolpaths_.erase(std::remove_if(toolpaths_.begin(), toolpaths_.end(),
        [index](const Toolpath& path) { return path.params.tool_index == index; }),
        toolpaths_.end());
    return true;
}

bool MillDocument::remove_tool(std::size_t index) {
    if (index >= tools_.size()) return false;
    checkpoint();
    toolpaths_.erase(std::remove_if(toolpaths_.begin(), toolpaths_.end(),
        [index](const Toolpath& path) { return path.params.tool_index == index; }),
        toolpaths_.end());
    for (Toolpath& path : toolpaths_) {
        if (path.params.tool_index > index) --path.params.tool_index;
    }
    tools_.erase(tools_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

void MillDocument::set_stock(StockBox stock) {
    validate_stock(stock);
    for (const MachiningBoundary& boundary : boundaries_) {
        for (const Vec2& point : boundary.polygon) {
            if (point.x < stock.min_corner.x - epsilon || point.x > stock.max_corner.x + epsilon ||
                point.y < stock.min_corner.y - epsilon || point.y > stock.max_corner.y + epsilon) {
                throw std::invalid_argument("new stock extents would exclude an existing machining boundary");
            }
        }
    }
    checkpoint();
    stock_ = stock;
    toolpaths_.clear();
}

std::size_t MillDocument::add_boundary(MachiningBoundary boundary) {
    boundary.polygon = normalize_convex(std::move(boundary.polygon));
    for (const Vec2& point : boundary.polygon) {
        if (point.x < stock_.min_corner.x - epsilon || point.x > stock_.max_corner.x + epsilon ||
            point.y < stock_.min_corner.y - epsilon || point.y > stock_.max_corner.y + epsilon) {
            throw std::invalid_argument("machining boundary must lie within stock XY extents");
        }
    }
    if (boundary.name.empty()) throw std::invalid_argument("machining boundary needs a name");
    checkpoint();
    boundaries_.push_back(std::move(boundary));
    return boundaries_.size() - 1;
}

bool MillDocument::set_boundary(std::size_t index, MachiningBoundary boundary) {
    if (index >= boundaries_.size()) return false;
    boundary.polygon = normalize_convex(std::move(boundary.polygon));
    if (boundary.name.empty()) throw std::invalid_argument("machining boundary needs a name");
    for (const Vec2& point : boundary.polygon) {
        if (point.x < stock_.min_corner.x - epsilon || point.x > stock_.max_corner.x + epsilon ||
            point.y < stock_.min_corner.y - epsilon || point.y > stock_.max_corner.y + epsilon) {
            throw std::invalid_argument("machining boundary must lie within stock XY extents");
        }
    }
    checkpoint();
    boundaries_[index] = std::move(boundary);
    toolpaths_.erase(std::remove_if(toolpaths_.begin(), toolpaths_.end(),
        [index](const Toolpath& path) {
            return path.params.boundary_index && *path.params.boundary_index == index;
        }), toolpaths_.end());
    return true;
}

bool MillDocument::remove_boundary(std::size_t index) {
    if (index >= boundaries_.size()) return false;
    checkpoint();
    toolpaths_.erase(std::remove_if(toolpaths_.begin(), toolpaths_.end(),
        [index](const Toolpath& path) {
            return path.params.boundary_index && *path.params.boundary_index == index;
        }), toolpaths_.end());
    for (Toolpath& path : toolpaths_) {
        if (path.params.boundary_index && *path.params.boundary_index > index) {
            --*path.params.boundary_index;
        }
    }
    boundaries_.erase(boundaries_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
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

std::size_t MillDocument::generate(const OperationParams& params, std::string name) {
    validate_operation(params);
    Toolpath toolpath;
    switch (params.strategy) {
        case Strategy::RasterZigZag: toolpath = generate_raster(params); break;
        case Strategy::ParallelOffset: toolpath = generate_parallel_offset(params); break;
        case Strategy::PencilMilling: toolpath = generate_pencil(params); break;
        case Strategy::Drilling: toolpath = generate_drilling(params); break;
        case Strategy::Profile: toolpath = generate_profile(params); break;
    }
    if (!name.empty()) toolpath.name = std::move(name);
    checkpoint();
    toolpaths_.push_back(std::move(toolpath));
    return toolpaths_.size() - 1;
}

bool MillDocument::regenerate(std::size_t index, const OperationParams& params, std::string name) {
    if (index >= toolpaths_.size()) return false;
    validate_operation(params);
    Toolpath calculated;
    switch (params.strategy) {
        case Strategy::RasterZigZag: calculated = generate_raster(params); break;
        case Strategy::ParallelOffset: calculated = generate_parallel_offset(params); break;
        case Strategy::PencilMilling: calculated = generate_pencil(params); break;
        case Strategy::Drilling: calculated = generate_drilling(params); break;
        case Strategy::Profile: calculated = generate_profile(params); break;
    }
    if (name.empty()) name = toolpaths_[index].name;
    checkpoint();
    calculated.name = std::move(name);
    toolpaths_[index] = std::move(calculated);
    return true;
}

bool MillDocument::rename_toolpath(std::size_t index, std::string name) {
    if (index >= toolpaths_.size()) return false;
    if (name.empty()) throw std::invalid_argument("toolpath name cannot be empty");
    if (toolpaths_[index].name == name) return true;
    checkpoint();
    toolpaths_[index].name = std::move(name);
    return true;
}

bool MillDocument::remove_toolpath(std::size_t index) {
    if (index >= toolpaths_.size()) return false;
    checkpoint();
    toolpaths_.erase(toolpaths_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

void MillDocument::clear_toolpaths() {
    if (toolpaths_.empty()) return;
    checkpoint();
    toolpaths_.clear();
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
    if (options.machine_name.empty() || options.machine_name.find_first_of("\r\n()") != std::string::npos) {
        throw std::invalid_argument("machine name must be a single safe comment token");
    }
    if (!options.absolute_mode) {
        throw std::invalid_argument("the generic post currently supports absolute positioning only");
    }
    if (options.decimal_places < 0 || options.decimal_places > 6) {
        throw std::invalid_argument("decimal places must be between zero and six");
    }
    const double unit_scale = options.metric ? 1.0 : (1.0 / 25.4);
    std::ostringstream output;
    auto format = [&options](double value) {
        std::ostringstream text;
        text.precision(options.decimal_places);
        text << std::fixed << value;
        return text.str();
    };

    output << "%\nO0001 (" << options.machine_name << ")\n";
    output << (options.metric ? "G21  ; metric\n" : "G20  ; inch\n");
    output << "G90  ; absolute\n";
    output << "G17  ; XY plane\nG94  ; feed per minute\n";
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
                    output << "G0 X" << format(move.pos.x * unit_scale)
                           << " Y" << format(move.pos.y * unit_scale)
                           << " Z" << format(move.pos.z * unit_scale) << '\n';
                    break;
                case MoveKind::Linear:
                case MoveKind::PlungeEntry:
                    output << "G1 X" << format(move.pos.x * unit_scale)
                           << " Y" << format(move.pos.y * unit_scale)
                           << " Z" << format(move.pos.z * unit_scale)
                           << " F" << format(move.feed_mm_min * unit_scale) << '\n';
                    break;
                case MoveKind::ArcCW:
                case MoveKind::ArcCCW:
                    output << (move.kind == MoveKind::ArcCW ? "G2" : "G3")
                           << " X" << format(move.pos.x * unit_scale)
                           << " Y" << format(move.pos.y * unit_scale)
                           << " Z" << format(move.pos.z * unit_scale)
                           << " I" << format(move.arc_center.x * unit_scale)
                           << " J" << format(move.arc_center.y * unit_scale)
                           << " F" << format(move.feed_mm_min * unit_scale) << '\n';
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

std::string MillDocument::serialize_native() const {
    using nlohmann::json;
    json root{{"schema", "mydesk.mill"}, {"version", 1}, {"name", name_}};
    const auto vec3 = [](const Vec3& value) {
        return json::array({value.x, value.y, value.z});
    };
    root["stock"] = {{"min", vec3(stock_.min_corner)}, {"max", vec3(stock_.max_corner)}};
    root["tools"] = json::array();
    for (const Tool& tool : tools_) {
        root["tools"].push_back({
            {"name", tool.name}, {"kind", static_cast<int>(tool.kind)},
            {"diameter_mm", tool.diameter_mm}, {"corner_radius_mm", tool.corner_radius_mm},
            {"flutes", tool.flutes}, {"overall_length_mm", tool.overall_length_mm},
            {"cutting_length_mm", tool.cutting_length_mm},
            {"shank_diameter_mm", tool.shank_diameter_mm},
            {"surface_speed_m_min", tool.surface_speed_m_min},
            {"feed_per_tooth_mm", tool.feed_per_tooth_mm}});
    }
    root["boundaries"] = json::array();
    for (const MachiningBoundary& boundary : boundaries_) {
        json points = json::array();
        for (const Vec2& point : boundary.polygon) points.push_back({point.x, point.y});
        root["boundaries"].push_back({{"name", boundary.name}, {"polygon", points}});
    }
    root["operations"] = json::array();
    for (const Toolpath& path : toolpaths_) {
        json drills = json::array();
        for (const Vec2& point : path.params.drill_points) drills.push_back({point.x, point.y});
        json params{
            {"strategy", static_cast<int>(path.params.strategy)},
            {"tool_index", path.params.tool_index},
            {"stepover_mm", path.params.stepover_mm}, {"stepdown_mm", path.params.stepdown_mm},
            {"z_top_mm", path.params.z_top_mm}, {"z_bottom_mm", path.params.z_bottom_mm},
            {"safe_z_mm", path.params.safe_z_mm}, {"tolerance_mm", path.params.tolerance_mm},
            {"climb_cut", path.params.climb_cut}, {"ramp_angle_deg", path.params.ramp_angle_deg},
            {"boundary_index", path.params.boundary_index ? json(*path.params.boundary_index) : json(nullptr)},
            {"drill_points", drills}};
        root["operations"].push_back({{"name", path.name}, {"params", params}});
    }
    return root.dump(2);
}

MillDocument MillDocument::deserialize_native(std::string_view text, std::string* error) {
    using nlohmann::json;
    if (error) error->clear();
    try {
        const json root = json::parse(text);
        if (root.at("schema").get<std::string>() != "mydesk.mill" ||
            root.at("version").get<int>() != 1) {
            throw std::runtime_error("unsupported MyDesk Mill document schema");
        }
        auto parse_vec3 = [](const json& value) {
            if (!value.is_array() || value.size() != 3) {
                throw std::runtime_error("expected a three-component coordinate");
            }
            return Vec3{value.at(0).get<double>(), value.at(1).get<double>(),
                        value.at(2).get<double>()};
        };

        MillDocument document;
        document.set_name(root.at("name").get<std::string>());
        const json& stock = root.at("stock");
        document.set_stock({parse_vec3(stock.at("min")), parse_vec3(stock.at("max"))});
        for (const json& value : root.at("tools")) {
            const int kind = value.at("kind").get<int>();
            if (kind < static_cast<int>(ToolKind::FlatEndmill) ||
                kind > static_cast<int>(ToolKind::FormTool)) {
                throw std::runtime_error("document contains an unknown tool kind");
            }
            Tool tool;
            tool.name = value.at("name").get<std::string>();
            tool.kind = static_cast<ToolKind>(kind);
            tool.diameter_mm = value.at("diameter_mm").get<double>();
            tool.corner_radius_mm = value.at("corner_radius_mm").get<double>();
            tool.flutes = value.at("flutes").get<double>();
            tool.overall_length_mm = value.at("overall_length_mm").get<double>();
            tool.cutting_length_mm = value.at("cutting_length_mm").get<double>();
            tool.shank_diameter_mm = value.at("shank_diameter_mm").get<double>();
            tool.surface_speed_m_min = value.at("surface_speed_m_min").get<double>();
            tool.feed_per_tooth_mm = value.at("feed_per_tooth_mm").get<double>();
            document.add_tool(std::move(tool));
        }
        for (const json& value : root.at("boundaries")) {
            MachiningBoundary boundary;
            boundary.name = value.at("name").get<std::string>();
            for (const json& point : value.at("polygon")) {
                if (!point.is_array() || point.size() != 2) {
                    throw std::runtime_error("expected a two-component boundary coordinate");
                }
                boundary.polygon.push_back({point.at(0).get<double>(), point.at(1).get<double>()});
            }
            document.add_boundary(std::move(boundary));
        }
        for (const json& value : root.at("operations")) {
            const json& source = value.at("params");
            const int strategy = source.at("strategy").get<int>();
            if (strategy < static_cast<int>(Strategy::RasterZigZag) ||
                strategy > static_cast<int>(Strategy::Profile)) {
                throw std::runtime_error("document contains an unknown machining strategy");
            }
            OperationParams params;
            params.strategy = static_cast<Strategy>(strategy);
            params.tool_index = source.at("tool_index").get<std::size_t>();
            params.stepover_mm = source.at("stepover_mm").get<double>();
            params.stepdown_mm = source.at("stepdown_mm").get<double>();
            params.z_top_mm = source.at("z_top_mm").get<double>();
            params.z_bottom_mm = source.at("z_bottom_mm").get<double>();
            params.safe_z_mm = source.at("safe_z_mm").get<double>();
            params.tolerance_mm = source.at("tolerance_mm").get<double>();
            params.climb_cut = source.at("climb_cut").get<bool>();
            params.ramp_angle_deg = source.at("ramp_angle_deg").get<double>();
            if (!source.at("boundary_index").is_null()) {
                params.boundary_index = source.at("boundary_index").get<std::size_t>();
            }
            for (const json& point : source.at("drill_points")) {
                if (!point.is_array() || point.size() != 2) {
                    throw std::runtime_error("expected a two-component drilling coordinate");
                }
                params.drill_points.push_back({point.at(0).get<double>(), point.at(1).get<double>()});
            }
            document.generate(params, value.at("name").get<std::string>());
        }
        document.undo_.clear();
        document.redo_.clear();
        return document;
    } catch (const std::exception& exception) {
        if (error) *error = exception.what();
        return MillDocument{};
    }
}

bool MillDocument::save_native_file(const std::filesystem::path& path, std::string* error) const {
    if (error) error->clear();
    try {
        if (path.empty()) throw std::runtime_error("document path is empty");
        const std::filesystem::path temporary = path.string() + ".tmp";
        const std::filesystem::path backup = path.string() + ".bak";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) throw std::runtime_error("cannot open temporary document for writing");
            output << serialize_native();
            output.flush();
            if (!output) throw std::runtime_error("failed while writing temporary document");
        }
        std::error_code ec;
        const bool had_original = std::filesystem::exists(path, ec) && !ec;
        if (had_original) {
            std::filesystem::remove(backup, ec);
            ec.clear();
            std::filesystem::rename(path, backup, ec);
            if (ec) throw std::runtime_error("cannot stage existing document for replacement");
        }
        std::filesystem::rename(temporary, path, ec);
        if (ec) {
            if (had_original) {
                std::error_code restore_error;
                std::filesystem::rename(backup, path, restore_error);
            }
            throw std::runtime_error("cannot replace the destination document");
        }
        if (had_original) std::filesystem::remove(backup, ec);
        return true;
    } catch (const std::exception& exception) {
        if (error) *error = exception.what();
        return false;
    }
}

MillDocument MillDocument::load_native_file(const std::filesystem::path& path, std::string* error) {
    if (error) error->clear();
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (error) *error = "cannot open MyDesk Mill document";
        return {};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) {
        if (error) *error = "failed while reading MyDesk Mill document";
        return {};
    }
    return deserialize_native(contents.str(), error);
}

} // namespace mydesk::mill
