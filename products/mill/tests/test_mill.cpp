// SPDX-License-Identifier: MPL-2.0
// MyDesk Mill — regression tests
#include "mydesk/mill/mill_document.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

using namespace mydesk::mill;

static int g_failed = 0;
#define CHECK(cond) \
    do { if (!(cond)) { \
        std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #cond); \
        ++g_failed; \
    } } while(0)
#define CHECK_NEAR(a, b, eps) \
    do { double _a = (a), _b = (b), _e = (eps); \
         if (std::abs(_a - _b) > _e) { \
             std::fprintf(stderr, "FAIL [line %d]: |%.6g - %.6g| > %.6g\n", \
                          __LINE__, _a, _b, _e); \
             ++g_failed; \
    } } while(0)

// ─── Tool library ────────────────────────────────────────────────────────────

static void test_tool_library() {
    MillDocument doc;
    Tool t; t.name = "6mm Flatmill"; t.diameter_mm = 6; t.flutes = 4;
    t.surface_speed_m_min = 200; t.feed_per_tooth_mm = 0.05;
    std::size_t idx = doc.add_tool(t);
    CHECK(idx == 0);
    CHECK(doc.tool_count() == 1);
    CHECK(doc.tool(0).name == "6mm Flatmill");
}

// ─── Feed/speed calculator ───────────────────────────────────────────────────

static void test_feed_speed() {
    MillDocument doc;
    Tool t; t.name = "D12"; t.diameter_mm = 12; t.flutes = 4;
    t.surface_speed_m_min = 150; t.feed_per_tooth_mm = 0.06;
    doc.add_tool(t);
    auto fs = doc.calc_feed_speed(0);
    // RPM = (150*1000)/(pi*12) ~ 3979
    CHECK_NEAR(fs.rpm, 150000.0 / (3.14159265358979 * 12.0), 1.0);
    // Feed = RPM * 4 * 0.06
    CHECK_NEAR(fs.feed_mm_min, fs.rpm * 4 * 0.06, 1.0);
}

// ─── Raster strategy ─────────────────────────────────────────────────────────

static void test_raster_generates_moves() {
    MillDocument doc;
    Tool t; t.name = "6mm FE"; t.diameter_mm = 6; t.flutes = 4;
    t.surface_speed_m_min = 200; t.feed_per_tooth_mm = 0.05;
    doc.add_tool(t);
    doc.stock() = {{0,0,-5}, {50,50,0}};

    OperationParams p;
    p.strategy = Strategy::RasterZigZag;
    p.tool_index = 0;
    p.stepover_mm = 3.0;
    p.stepdown_mm = 2.5;
    p.z_top_mm = 0.0;
    p.z_bottom_mm = -5.0;
    p.safe_z_mm = 10.0;

    std::size_t idx = doc.generate(p);
    CHECK(idx == 0);
    CHECK(doc.toolpath(0).moves.size() > 0);
    // Must have at least some linear cutting moves
    std::size_t linears = 0;
    for (const auto& mv : doc.toolpath(0).moves)
        if (mv.kind == MoveKind::Linear) ++linears;
    CHECK(linears > 0);
}

static void test_raster_cut_length() {
    MillDocument doc;
    Tool t; t.name = "D6"; t.diameter_mm = 6; t.flutes = 4;
    t.surface_speed_m_min = 200; t.feed_per_tooth_mm = 0.05;
    doc.add_tool(t);
    doc.stock() = {{0,0,-2}, {100,100,0}};

    OperationParams p;
    p.strategy     = Strategy::RasterZigZag;
    p.tool_index   = 0;
    p.stepover_mm  = 5.0;
    p.stepdown_mm  = 2.0;
    p.z_top_mm     = 0.0;
    p.z_bottom_mm  = -2.0;
    p.safe_z_mm    = 10.0;

    doc.generate(p);
    double len = doc.toolpath(0).total_cut_length_mm();
    CHECK(len > 100.0);   // at minimum 100mm of cuts on a 100x100 stock
}

// ─── Contour parallel strategy ───────────────────────────────────────────────

static void test_parallel_offset_generates_moves() {
    MillDocument doc;
    Tool t; t.name = "8mm"; t.diameter_mm = 8; t.flutes = 4;
    t.surface_speed_m_min = 180; t.feed_per_tooth_mm = 0.04;
    doc.add_tool(t);
    doc.stock() = {{0,0,-3}, {60,60,0}};

    OperationParams p;
    p.strategy    = Strategy::ParallelOffset;
    p.tool_index  = 0;
    p.stepover_mm = 4.0;
    p.stepdown_mm = 3.0;
    p.z_top_mm    = 0.0;
    p.z_bottom_mm = -3.0;
    p.safe_z_mm   = 5.0;

    doc.generate(p);
    CHECK(doc.toolpath(0).moves.size() > 0);
}

// ─── Pencil strategy ─────────────────────────────────────────────────────────

static void test_pencil_generates_moves() {
    MillDocument doc;
    Tool t; t.name = "3mm Ball"; t.kind = ToolKind::BallEndmill;
    t.diameter_mm = 3; t.flutes = 2;
    t.surface_speed_m_min = 120; t.feed_per_tooth_mm = 0.02;
    doc.add_tool(t);
    doc.stock() = {{0,0,-1}, {40,40,0}};

    OperationParams p;
    p.strategy    = Strategy::PencilMilling;
    p.tool_index  = 0;
    p.z_bottom_mm = -1.0;
    p.safe_z_mm   = 5.0;

    doc.generate(p);
    CHECK(doc.toolpath(0).moves.size() >= 4);
}

// ─── Drilling strategy ───────────────────────────────────────────────────────

static void test_drilling() {
    MillDocument doc;
    Tool drill; drill.name = "D5 Drill"; drill.kind = ToolKind::DrillBit;
    drill.diameter_mm = 5; drill.flutes = 2;
    drill.surface_speed_m_min = 50; drill.feed_per_tooth_mm = 0.1;
    doc.add_tool(drill);
    doc.stock() = {{0,0,-20}, {50,50,0}};

    OperationParams p;
    p.strategy    = Strategy::Drilling;
    p.tool_index  = 0;
    p.z_bottom_mm = -20.0;
    p.safe_z_mm   = 5.0;

    doc.generate(p);
    bool has_dwell = false;
    for (const auto& mv : doc.toolpath(0).moves)
        if (mv.kind == MoveKind::Dwell) has_dwell = true;
    CHECK(has_dwell);
}

// ─── Profile strategy ────────────────────────────────────────────────────────

static void test_profile() {
    MillDocument doc;
    Tool t; t.name = "D10"; t.diameter_mm = 10; t.flutes = 4;
    t.surface_speed_m_min = 160; t.feed_per_tooth_mm = 0.05;
    doc.add_tool(t);
    doc.stock() = {{0,0,-10}, {80,80,0}};

    OperationParams p;
    p.strategy    = Strategy::Profile;
    p.tool_index  = 0;
    p.stepdown_mm = 5.0;
    p.z_top_mm    = 0.0;
    p.z_bottom_mm = -10.0;
    p.safe_z_mm   = 5.0;

    doc.generate(p);
    CHECK(doc.toolpath(0).moves.size() > 0);
}

// ─── G-code export ───────────────────────────────────────────────────────────

static void test_gcode_export_header() {
    MillDocument doc;
    Tool t; t.name = "D6"; t.diameter_mm = 6; t.flutes = 4;
    t.surface_speed_m_min = 200; t.feed_per_tooth_mm = 0.05;
    doc.add_tool(t);
    doc.stock() = {{0,0,-2}, {50,50,0}};

    OperationParams p;
    p.strategy = Strategy::RasterZigZag;
    p.tool_index = 0; p.stepover_mm = 3; p.stepdown_mm = 2;
    p.z_top_mm = 0; p.z_bottom_mm = -2; p.safe_z_mm = 5;
    doc.generate(p);

    std::string gcode = doc.export_gcode();
    CHECK(gcode.find("G21") != std::string::npos);   // metric
    CHECK(gcode.find("G90") != std::string::npos);   // absolute
    CHECK(gcode.find("M30") != std::string::npos);   // program end
    CHECK(gcode.find("G1")  != std::string::npos);   // cutting moves
    CHECK(gcode.find("G0")  != std::string::npos);   // rapid moves
}

static void test_gcode_export_spindle() {
    MillDocument doc;
    Tool t; t.name = "D6"; t.diameter_mm = 6; t.flutes = 4;
    t.surface_speed_m_min = 200; t.feed_per_tooth_mm = 0.05;
    doc.add_tool(t);
    doc.stock() = {{0,0,-2}, {50,50,0}};

    OperationParams p;
    p.strategy = Strategy::Drilling; p.tool_index = 0;
    p.z_bottom_mm = -2; p.safe_z_mm = 5;
    doc.generate(p);

    std::string gcode = doc.export_gcode();
    CHECK(gcode.find("M03") != std::string::npos);  // spindle on
    CHECK(gcode.find("M30") != std::string::npos);
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main() {
    test_tool_library();
    test_feed_speed();
    test_raster_generates_moves();
    test_raster_cut_length();
    test_parallel_offset_generates_moves();
    test_pencil_generates_moves();
    test_drilling();
    test_profile();
    test_gcode_export_header();
    test_gcode_export_spindle();

    if (g_failed == 0) {
        std::printf("All Mill tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d Mill test(s) FAILED.\n", g_failed);
    return 1;
}
