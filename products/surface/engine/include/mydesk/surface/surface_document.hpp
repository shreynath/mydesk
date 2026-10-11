// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mydesk::surface {

using CurveId = std::uint64_t;
using SurfacePatchId = std::uint64_t;

struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    [[nodiscard]] double length() const;
    [[nodiscard]] Vec3 normalized() const;
    [[nodiscard]] Vec3 cross(const Vec3& o) const;
    [[nodiscard]] double dot(const Vec3& o) const;
};

struct WeightedPoint {
    Vec3 p;
    double w{1.0};
};

struct NurbsCurve {
    CurveId id{0};
    std::string name;
    int degree{3};
    std::vector<double> knots;
    std::vector<WeightedPoint> controlPoints;

    [[nodiscard]] Vec3 evaluate(double u) const;
    [[nodiscard]] double curvatureAt(double u) const;
};

struct CurvatureCombSample {
    double u{0.0};
    Vec3 point;
    Vec3 combVector;
    double curvature{0.0};
};

struct NurbsSurface {
    SurfacePatchId id{0};
    std::string name;
    int degreeU{3};
    int degreeV{3};
    std::vector<double> knotsU;
    std::vector<double> knotsV;
    std::size_t numU{0};
    std::size_t numV{0};
    std::vector<WeightedPoint> controlGrid; // size numU * numV (row-major)

    [[nodiscard]] Vec3 evaluate(double u, double v) const;
    [[nodiscard]] Vec3 normalAt(double u, double v) const;
    [[nodiscard]] double gaussianCurvatureAt(double u, double v) const;
    [[nodiscard]] double meanCurvatureAt(double u, double v) const;
};

enum class ContinuityOrder {
    G0,
    G1,
    G2
};

struct ContinuityReport {
    ContinuityOrder achieved{ContinuityOrder::G0};
    double maxPositionalGap{0.0};
    double maxAngularDeviationDeg{0.0};
    double maxCurvatureDeviation{0.0};
    bool passesTolerance{false};
};

class SurfaceDocument {
public:
    SurfaceDocument();

    [[nodiscard]] const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    // NURBS Curves
    CurveId addCubicCurve(std::string name, std::vector<Vec3> controlPoints);
    [[nodiscard]] const NurbsCurve* findCurve(CurveId id) const;
    [[nodiscard]] const std::vector<NurbsCurve>& curves() const { return curves_; }
    [[nodiscard]] std::vector<CurvatureCombSample> generateCurvatureComb(CurveId id, int samples, double scale = 1.0) const;

    // NURBS Surfaces
    SurfacePatchId addBilinearPatch(std::string name, Vec3 p00, Vec3 p10, Vec3 p01, Vec3 p11);
    SurfacePatchId addBicubicPatch(std::string name, const std::vector<Vec3>& grid4x4);
    [[nodiscard]] const NurbsSurface* findSurface(SurfacePatchId id) const;
    [[nodiscard]] const std::vector<NurbsSurface>& surfaces() const { return surfaces_; }

    // Continuity Analysis
    [[nodiscard]] ContinuityReport checkSeamContinuity(SurfacePatchId patchA, SurfacePatchId patchB,
                                                       int samples = 10) const;

    // Zebra Stripe Reflection Simulation
    [[nodiscard]] std::vector<double> evaluateZebraStripes(SurfacePatchId patchId, int uRes, int vRes,
                                                          double stripeFrequency = 10.0) const;

    // Interoperability (IGES Entity 128 / STEP export preview)
    [[nodiscard]] std::string exportIges() const;

    // Native persistence
    [[nodiscard]] std::string serializeNative() const;
    static SurfaceDocument deserializeNative(std::string_view json, std::string* error = nullptr);

private:
    std::string name_{"Untitled_Surface"};
    std::vector<NurbsCurve> curves_;
    std::vector<NurbsSurface> surfaces_;
    CurveId nextCurveId_{1};
    SurfacePatchId nextSurfId_{1};
};

} // namespace mydesk::surface
