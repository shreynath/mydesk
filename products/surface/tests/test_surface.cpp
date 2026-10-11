// SPDX-License-Identifier: MPL-2.0
// MyDesk Surface — Regression & Verification Tests
#include "mydesk/surface/surface_document.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

using namespace mydesk::surface;

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

static void test_surface_nurbs_curve_eval() {
    SurfaceDocument doc;
    // Straight line cubic spline
    const auto id = doc.addCubicCurve("Line", {
        {0, 0, 0}, {10, 0, 0}, {20, 0, 0}, {30, 0, 0}
    });
    const auto* c = doc.findCurve(id);
    CHECK(c != nullptr);

    auto p0 = c->evaluate(0.0);
    CHECK_NEAR(p0.x, 0.0, 1e-6);

    auto p1 = c->evaluate(1.0);
    CHECK_NEAR(p1.x, 30.0, 1e-6);

    auto pMid = c->evaluate(0.5);
    CHECK_NEAR(pMid.x, 15.0, 1e-6);
    CHECK_NEAR(c->curvatureAt(0.5), 0.0, 1e-4); // straight line has zero curvature
}

static void test_surface_curvature_comb() {
    SurfaceDocument doc;
    // Curved cubic spline
    const auto id = doc.addCubicCurve("Arch", {
        {0, 0, 0}, {10, 15, 0}, {20, 15, 0}, {30, 0, 0}
    });

    const auto comb = doc.generateCurvatureComb(id, 5, 2.0);
    CHECK(comb.size() == 5);
    // Midpoint should have positive curvature
    CHECK(comb[2].curvature > 0.0);
}

static void test_surface_bilinear_patch() {
    SurfaceDocument doc;
    const auto id = doc.addBilinearPatch("FlatQuad",
        {0, 0, 0}, {10, 0, 0},
        {0, 10, 0}, {10, 10, 0}
    );
    const auto* s = doc.findSurface(id);
    CHECK(s != nullptr);

    auto pCenter = s->evaluate(0.5, 0.5);
    CHECK_NEAR(pCenter.x, 5.0, 1e-6);
    CHECK_NEAR(pCenter.y, 5.0, 1e-6);
    CHECK_NEAR(pCenter.z, 0.0, 1e-6);

    auto n = s->normalAt(0.5, 0.5);
    CHECK_NEAR(n.z, 1.0, 1e-4);
}

static void test_surface_gaussian_mean_curvature() {
    SurfaceDocument doc;
    // Flat planar patch has zero Gaussian and Mean curvature
    const auto id = doc.addBilinearPatch("FlatPlane",
        {0, 0, 0}, {20, 0, 0},
        {0, 20, 0}, {20, 20, 0}
    );
    const auto* s = doc.findSurface(id);
    CHECK_NEAR(s->gaussianCurvatureAt(0.5, 0.5), 0.0, 1e-4);
    CHECK_NEAR(s->meanCurvatureAt(0.5, 0.5), 0.0, 1e-4);
}

static void test_surface_g0_g1_continuity() {
    SurfaceDocument doc;
    // Two contiguous planar patches sharing seam at X = 10
    const auto pA = doc.addBilinearPatch("PatchA",
        {0, 0, 0}, {10, 0, 0},
        {0, 10, 0}, {10, 10, 0}
    );
    const auto pB = doc.addBilinearPatch("PatchB",
        {10, 0, 0}, {20, 0, 0},
        {10, 10, 0}, {20, 10, 0}
    );

    const auto rep = doc.checkSeamContinuity(pA, pB, 5);
    CHECK(rep.passesTolerance);
    CHECK(rep.achieved == ContinuityOrder::G1);
    CHECK_NEAR(rep.maxPositionalGap, 0.0, 1e-6);
    CHECK_NEAR(rep.maxAngularDeviationDeg, 0.0, 1e-4);
}

static void test_surface_zebra_stripes() {
    SurfaceDocument doc;
    const auto id = doc.addBilinearPatch("Panel",
        {0, 0, 0}, {50, 0, 0},
        {0, 50, 0}, {50, 50, 0}
    );
    const auto stripes = doc.evaluateZebraStripes(id, 8, 8, 5.0);
    CHECK(stripes.size() == 64);
    for (double val : stripes) {
        CHECK(val >= 0.0 && val <= 1.0);
    }
}

static void test_surface_native_roundtrip() {
    SurfaceDocument doc;
    doc.setName("ClassAHood");
    doc.addCubicCurve("Profile", {{0, 0, 0}, {10, 5, 0}, {20, 5, 0}, {30, 0, 0}});
    doc.addBilinearPatch("RoofPatch", {0, 0, 0}, {10, 0, 0}, {0, 10, 0}, {10, 10, 0});

    const auto json = doc.serializeNative();
    CHECK(!json.empty());

    std::string err;
    SurfaceDocument loaded = SurfaceDocument::deserializeNative(json, &err);
    CHECK(err.empty());
    CHECK(loaded.name() == "ClassAHood");
    CHECK(loaded.curves().size() == 1);
    CHECK(loaded.curves().front().name == "Profile");
}

int main() {
    // Registered capability test identifiers:
    // "test_surface_nurbs_curve_eval"
    test_surface_nurbs_curve_eval();
    // "test_surface_curvature_comb"
    test_surface_curvature_comb();
    // "test_surface_bilinear_patch"
    test_surface_bilinear_patch();
    // "test_surface_gaussian_mean_curvature"
    test_surface_gaussian_mean_curvature();
    // "test_surface_g0_g1_continuity"
    test_surface_g0_g1_continuity();
    // "test_surface_zebra_stripes"
    test_surface_zebra_stripes();
    // "test_surface_native_roundtrip"
    test_surface_native_roundtrip();

    if (g_failed == 0) {
        std::printf("All Surface tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d Surface test(s) FAILED.\n", g_failed);
    return 1;
}
