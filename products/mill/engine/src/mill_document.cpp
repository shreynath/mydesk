// SPDX-License-Identifier: MPL-2.0
// MyDesk Mill — CAM engine implementation
#include "mydesk/mill/mill_document.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace mydesk::mill {

// ─── Toolpath metrics ───────────────────────────────────────────────────────

double Toolpath::total_cut_length_mm() const noexcept {
    double len = 0.0;
    for (std::size_t i = 1; i < moves.size(); ++i) {
        if (moves[i].kind == MoveKind::Linear ||
            moves[i].kind == MoveKind::PlungeEntry) {
            double dx = moves[i].pos.x - moves[i-1].pos.x;
            double dy = moves[i].pos.y - moves[i-1].pos.y;
            double dz = moves[i].pos.z - moves[i-1].pos.z;
            len += std::sqrt(dx*dx + dy*dy + dz*dz);
        }
    }
    return len;
}

double Toolpath::estimated_time_min() const noexcept {
    double t = 0.0;
    for (std::size_t i = 1; i < moves.size(); ++i) {
        const ToolMove& m = moves[i];
        double dx = m.pos.x - moves[i-1].pos.x;
        double dy = m.pos.y - moves[i-1].pos.y;
        double dz = m.pos.z - moves[i-1].pos.z;
        double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        double feed = m.feed_mm_min > 0 ? m.feed_mm_min : 10000.0; // rapid ~10m/min
        t += dist / feed;
    }
    return t;
}

// ─── MillDocument ────────────────────────────────────────────────────────────

MillDocument::MillDocument() {
    stock_.min_corner = {0,0,-10};
    stock_.max_corner = {100,100,0};
}

std::size_t MillDocument::add_tool(Tool t) {
    std::size_t idx = tools_.size();
    tools_.push_back(std::move(t));
    return idx;
}

MillDocument::FeedSpeed MillDocument::calc_feed_speed(std::size_t tool_idx) const noexcept {
    const Tool& t = tools_.at(tool_idx);
    // RPM = (Vc * 1000) / (pi * D)
    double rpm = (t.surface_speed_m_min * 1000.0) / (3.14159265358979323846 * t.diameter_mm);
    // Feed = RPM * flutes * fpt
    double feed = rpm * t.flutes * t.feed_per_tooth_mm;
    return {rpm, feed};
}

// ─── Raster zig-zag strategy ─────────────────────────────────────────────────
// Generates horizontal scan lines over the stock X-Y extent, stepping in Z.

Toolpath MillDocument::generate_raster(const OperationParams& p) const {
    const Tool& t = tools_.at(p.tool_index);
    auto fs = calc_feed_speed(p.tool_index);
    double stepover = p.stepover_mm > 0 ? p.stepover_mm : t.diameter_mm * 0.5;

    double xmin = stock_.min_corner.x + t.diameter_mm * 0.5;
    double xmax = stock_.max_corner.x - t.diameter_mm * 0.5;
    double ymin = stock_.min_corner.y + t.diameter_mm * 0.5;
    double ymax = stock_.max_corner.y - t.diameter_mm * 0.5;

    Toolpath tp;
    tp.name   = "Raster";
    tp.params = p;

    // Retract to safe Z first
    tp.moves.push_back({MoveKind::Rapid, {xmin, ymin, p.safe_z_mm}, {}, 0, fs.rpm});

    // Step down through Z levels
    double z = p.z_top_mm;
    while (z > p.z_bottom_mm - 1e-9) {
        z = std::max(z, p.z_bottom_mm);

        bool dir = true;  // zig-zag direction
        double y = ymin;
        while (y <= ymax + 1e-9) {
            y = std::min(y, ymax);
            double x_start = dir ? xmin : xmax;
            double x_end   = dir ? xmax : xmin;

            // Rapid to lane start (at safe Z)
            tp.moves.push_back({MoveKind::Rapid, {x_start, y, p.safe_z_mm}, {}, 0, fs.rpm});
            // Plunge
            tp.moves.push_back({MoveKind::PlungeEntry, {x_start, y, z}, {}, fs.feed_mm_min * 0.3, fs.rpm});
            // Cut across
            tp.moves.push_back({MoveKind::Linear, {x_end, y, z}, {}, fs.feed_mm_min, fs.rpm});
            // Retract
            tp.moves.push_back({MoveKind::Retract, {x_end, y, p.safe_z_mm}, {}, fs.feed_mm_min * 2.0, fs.rpm});

            y += stepover;
            dir = !dir;
        }

        if (z <= p.z_bottom_mm + 1e-9) break;
        z -= p.stepdown_mm;
    }

    return tp;
}

// ─── Contour parallel (equidistant offset) ───────────────────────────────────
// Generates rectangular offset paths shrinking inward from stock perimeter.

Toolpath MillDocument::generate_parallel_offset(const OperationParams& p) const {
    const Tool& t = tools_.at(p.tool_index);
    auto fs = calc_feed_speed(p.tool_index);
    double stepover = p.stepover_mm > 0 ? p.stepover_mm : t.diameter_mm * 0.5;

    Toolpath tp;
    tp.name   = "ContourParallel";
    tp.params = p;

    double z = p.z_top_mm;
    while (z > p.z_bottom_mm - 1e-9) {
        z = std::max(z, p.z_bottom_mm);

        // Concentric rectangles stepping inward
        double off = t.diameter_mm * 0.5;
        while (true) {
            double x0 = stock_.min_corner.x + off;
            double y0 = stock_.min_corner.y + off;
            double x1 = stock_.max_corner.x - off;
            double y1 = stock_.max_corner.y - off;
            if (x1 <= x0 || y1 <= y0) break;

            tp.moves.push_back({MoveKind::Rapid,       {x0, y0, p.safe_z_mm}, {}, 0, fs.rpm});
            tp.moves.push_back({MoveKind::PlungeEntry, {x0, y0, z}, {}, fs.feed_mm_min * 0.3, fs.rpm});
            tp.moves.push_back({MoveKind::Linear,      {x1, y0, z}, {}, fs.feed_mm_min, fs.rpm});
            tp.moves.push_back({MoveKind::Linear,      {x1, y1, z}, {}, fs.feed_mm_min, fs.rpm});
            tp.moves.push_back({MoveKind::Linear,      {x0, y1, z}, {}, fs.feed_mm_min, fs.rpm});
            tp.moves.push_back({MoveKind::Linear,      {x0, y0, z}, {}, fs.feed_mm_min, fs.rpm});
            tp.moves.push_back({MoveKind::Retract,     {x0, y0, p.safe_z_mm}, {}, 0, fs.rpm});

            off += stepover;
        }

        if (z <= p.z_bottom_mm + 1e-9) break;
        z -= p.stepdown_mm;
    }
    return tp;
}

// ─── Pencil milling ──────────────────────────────────────────────────────────
// Traces the floor perimeter at the bottom Z to clean up fillets.

Toolpath MillDocument::generate_pencil(const OperationParams& p) const {
    const Tool& t = tools_.at(p.tool_index);
    auto fs = calc_feed_speed(p.tool_index);

    Toolpath tp;
    tp.name   = "Pencil";
    tp.params = p;

    double off = t.diameter_mm * 0.5;
    double z   = p.z_bottom_mm;
    double x0  = stock_.min_corner.x + off;
    double y0  = stock_.min_corner.y + off;
    double x1  = stock_.max_corner.x - off;
    double y1  = stock_.max_corner.y - off;

    tp.moves.push_back({MoveKind::Rapid,       {x0, y0, p.safe_z_mm}, {}, 0, fs.rpm});
    tp.moves.push_back({MoveKind::PlungeEntry, {x0, y0, z}, {}, fs.feed_mm_min * 0.2, fs.rpm});
    tp.moves.push_back({MoveKind::Linear, {x1, y0, z}, {}, fs.feed_mm_min * 0.5, fs.rpm});
    tp.moves.push_back({MoveKind::Linear, {x1, y1, z}, {}, fs.feed_mm_min * 0.5, fs.rpm});
    tp.moves.push_back({MoveKind::Linear, {x0, y1, z}, {}, fs.feed_mm_min * 0.5, fs.rpm});
    tp.moves.push_back({MoveKind::Linear, {x0, y0, z}, {}, fs.feed_mm_min * 0.5, fs.rpm});
    tp.moves.push_back({MoveKind::Retract, {x0, y0, p.safe_z_mm}, {}, 0, fs.rpm});
    return tp;
}

// ─── Drilling ────────────────────────────────────────────────────────────────

Toolpath MillDocument::generate_drilling(const OperationParams& p) const {
    auto fs = calc_feed_speed(p.tool_index);
    Toolpath tp;
    tp.name   = "Drilling";
    tp.params = p;

    // Drill grid across stock centre (exemplar: single hole at stock centre)
    double cx = (stock_.min_corner.x + stock_.max_corner.x) * 0.5;
    double cy = (stock_.min_corner.y + stock_.max_corner.y) * 0.5;

    tp.moves.push_back({MoveKind::Rapid,       {cx, cy, p.safe_z_mm}, {}, 0, fs.rpm});
    tp.moves.push_back({MoveKind::PlungeEntry, {cx, cy, p.z_bottom_mm}, {}, fs.feed_mm_min * 0.25, fs.rpm});
    tp.moves.push_back({MoveKind::Dwell,       {cx, cy, p.z_bottom_mm}, {}, 0, fs.rpm});
    tp.moves.push_back({MoveKind::Retract,     {cx, cy, p.safe_z_mm},   {}, 0, fs.rpm});
    return tp;
}

// ─── Profile ─────────────────────────────────────────────────────────────────

Toolpath MillDocument::generate_profile(const OperationParams& p) const {
    const Tool& t = tools_.at(p.tool_index);
    auto fs = calc_feed_speed(p.tool_index);
    Toolpath tp;
    tp.name   = "Profile";
    tp.params = p;

    double off = t.diameter_mm * 0.5;
    double z   = p.z_top_mm;
    while (z > p.z_bottom_mm - 1e-9) {
        z = std::max(z, p.z_bottom_mm);

        double x0 = stock_.min_corner.x + off;
        double y0 = stock_.min_corner.y + off;
        double x1 = stock_.max_corner.x - off;
        double y1 = stock_.max_corner.y - off;

        tp.moves.push_back({MoveKind::Rapid,       {x0, y0, p.safe_z_mm}, {}, 0, fs.rpm});
        // Ramp entry
        tp.moves.push_back({MoveKind::PlungeEntry, {x0, y0, z}, {}, fs.feed_mm_min * 0.3, fs.rpm});
        tp.moves.push_back({MoveKind::Linear, {x1, y0, z}, {}, fs.feed_mm_min, fs.rpm});
        tp.moves.push_back({MoveKind::Linear, {x1, y1, z}, {}, fs.feed_mm_min, fs.rpm});
        tp.moves.push_back({MoveKind::Linear, {x0, y1, z}, {}, fs.feed_mm_min, fs.rpm});
        tp.moves.push_back({MoveKind::Linear, {x0, y0, z}, {}, fs.feed_mm_min, fs.rpm});
        tp.moves.push_back({MoveKind::Retract, {x0, y0, p.safe_z_mm}, {}, 0, fs.rpm});

        if (z <= p.z_bottom_mm + 1e-9) break;
        z -= p.stepdown_mm;
    }
    return tp;
}

// ─── Dispatch ────────────────────────────────────────────────────────────────

std::size_t MillDocument::generate(const OperationParams& p) {
    Toolpath tp;
    switch (p.strategy) {
        case Strategy::RasterZigZag:    tp = generate_raster(p);          break;
        case Strategy::ParallelOffset:  tp = generate_parallel_offset(p); break;
        case Strategy::PencilMilling:   tp = generate_pencil(p);          break;
        case Strategy::Drilling:        tp = generate_drilling(p);        break;
        case Strategy::Profile:         tp = generate_profile(p);         break;
    }
    std::size_t idx = toolpaths_.size();
    toolpaths_.push_back(std::move(tp));
    return idx;
}

// ─── G-code export ───────────────────────────────────────────────────────────

std::string MillDocument::export_gcode(const GCodeOptions& opts) const {
    std::ostringstream ss;
    int dp = opts.decimal_places;

    auto fmt = [&](double v) -> std::string {
        std::ostringstream tmp;
        tmp.precision(dp);
        tmp << std::fixed << v;
        return tmp.str();
    };

    // Program header
    ss << "%" << "\n";
    ss << "O0001 (" << opts.machine_name << ")\n";
    if (opts.metric)       ss << "G21  ; metric\n";
    if (opts.absolute_mode) ss << "G90  ; absolute\n";
    ss << "G17  ; XY plane\n";
    ss << "G94  ; feed in mm/min\n";
    if (opts.use_tool_length_comp) ss << "G43 H1 ; tool length comp\n";
    ss << "\n";

    for (std::size_t pi = 0; pi < toolpaths_.size(); ++pi) {
        const Toolpath& tp = toolpaths_[pi];
        const Tool& tool = tools_.at(tp.params.tool_index);
        ss << "; === Toolpath: " << tp.name << " ===\n";
        ss << "T" << (tp.params.tool_index + 1) << " M06  ; " << tool.name << "\n";

        double last_rpm = -1;
        for (const ToolMove& mv : tp.moves) {
            if (mv.spindle_rpm != last_rpm && mv.spindle_rpm > 0) {
                ss << "S" << static_cast<int>(mv.spindle_rpm) << " M03\n";
                last_rpm = mv.spindle_rpm;
            }
            switch (mv.kind) {
                case MoveKind::Rapid:
                case MoveKind::Retract:
                    ss << "G0 X" << fmt(mv.pos.x)
                       << " Y" << fmt(mv.pos.y)
                       << " Z" << fmt(mv.pos.z) << "\n";
                    break;
                case MoveKind::Linear:
                case MoveKind::PlungeEntry:
                    ss << "G1 X" << fmt(mv.pos.x)
                       << " Y" << fmt(mv.pos.y)
                       << " Z" << fmt(mv.pos.z)
                       << " F" << fmt(mv.feed_mm_min) << "\n";
                    break;
                case MoveKind::ArcCW:
                    ss << "G2 X" << fmt(mv.pos.x)
                       << " Y" << fmt(mv.pos.y)
                       << " Z" << fmt(mv.pos.z)
                       << " I" << fmt(mv.arc_center.x)
                       << " J" << fmt(mv.arc_center.y)
                       << " F" << fmt(mv.feed_mm_min) << "\n";
                    break;
                case MoveKind::ArcCCW:
                    ss << "G3 X" << fmt(mv.pos.x)
                       << " Y" << fmt(mv.pos.y)
                       << " Z" << fmt(mv.pos.z)
                       << " I" << fmt(mv.arc_center.x)
                       << " J" << fmt(mv.arc_center.y)
                       << " F" << fmt(mv.feed_mm_min) << "\n";
                    break;
                case MoveKind::Dwell:
                    ss << "G4 P500\n";
                    break;
                case MoveKind::ToolChange:
                    ss << "M06\n";
                    break;
            }
        }
        ss << "M09  ; coolant off\n\n";
    }
    ss << "M30  ; program end\n%\n";
    return ss.str();
}

} // namespace mydesk::mill
