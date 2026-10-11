// SPDX-License-Identifier: MPL-2.0
// MyDesk Mechanical – Inventor-class parametric part modelling (OCCT B-rep backend).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <TopoDS_Shape.hxx>

namespace mydesk::mechanical {

using FeatureId    = std::uint64_t;
using ParameterId  = std::uint64_t;

// ──────────────────────────────────────────────────────────────────────────────
// Feature taxonomy – mirrors Inventor's browser-tree entries.
// ──────────────────────────────────────────────────────────────────────────────
enum class FeatureKind {
    SketchRectangle,   // parametric 2-D rectangle profile
    SketchCircle,      // parametric 2-D circle profile (for revolve/loft)
    SketchPolygon,     // regular N-gon profile (for sweep)
    Extrude,           // linear extrusion of preceding sketch
    Revolve,           // rotation of sketch profile around an axis
    Sweep,             // profile swept along a linear path
    Loft,              // blended solid between two sketch profiles
    Fillet,            // edge-radius blend (constant radius)
    Chamfer,           // edge bevel
    Shell,             // hollow shell (remove one face, offset walls)
    RectangularPattern,// linear/rectangular feature instance array
    MirrorFeature,     // mirror across a work-plane
    WorkPlane,         // reference datum plane
    Hole,              // cylindrical boolean cut
    Thread,            // cosmetic or computed thread
    Rib,               // thin rib/web from a sketch
    Emboss,            // raised/indented profile onto a face
};

// ──────────────────────────────────────────────────────────────────────────────
// Data structures
// ──────────────────────────────────────────────────────────────────────────────
struct Parameter {
    ParameterId id{0};
    std::string name;
    double      value{0.0};
    std::string unit{"mm"};
};

struct Feature {
    FeatureId   id{0};
    FeatureKind kind{FeatureKind::SketchRectangle};
    std::string name;
    std::vector<FeatureId> dependsOn;
    std::unordered_map<std::string, ParameterId> parameters;
    bool suppressed{false};
};

struct RegenerationReport {
    bool        ok{false};
    std::string message;
    std::size_t featuresEvaluated{0};
    double      solidVolume{0.0};
    bool        isSolid{false};
};

// Result of STEP/IGES export
struct ExportReport {
    bool        ok{false};
    std::string message;
    std::string path;
};

// Sketch constraint types (geometry-level, not feature-level)
enum class ConstraintKind { Coincident, Parallel, Perpendicular, Tangent, Equal, Fixed };

struct SketchConstraint {
    ConstraintKind kind;
    std::size_t    entityA{0};
    std::size_t    entityB{0};
};

// Measurement result returned by measureDistance / measureAngle
struct MeasureResult {
    bool   ok{false};
    double value{0.0};  // mm for distance, degrees for angle
    std::string description;
};

// Work-feature reference plane
struct WorkPlane {
    std::string name;
    double nx{0.0}, ny{0.0}, nz{1.0}; // normal
    double ox{0.0}, oy{0.0}, oz{0.0}; // origin
};

// ──────────────────────────────────────────────────────────────────────────────
// PartDocument – Inventor-class part: parametric feature graph + OCCT B-rep.
// ──────────────────────────────────────────────────────────────────────────────
class PartDocument {
public:
    PartDocument();

    // ── Identity ─────────────────────────────────────────────────────────────
    [[nodiscard]] const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    // ── Parameters ───────────────────────────────────────────────────────────
    ParameterId addParameter(std::string name, double value, std::string unit = "mm");
    bool        setParameter(ParameterId id, double value);
    bool        setParameterByName(std::string_view name, double value);
    [[nodiscard]] const Parameter* findParameter(ParameterId id) const;
    [[nodiscard]] const Parameter* findParameterByName(std::string_view name) const;
    [[nodiscard]] const std::vector<Parameter>& parameters() const { return parameters_; }

    // ── Sketch profiles ──────────────────────────────────────────────────────
    /// Rectangle profile driven by width × height parameters.
    FeatureId addRectangleExtrude(std::string sketchName, std::string extrudeName,
                                  double width, double height, double depth);

    /// Circle profile for revolve/loft input.
    FeatureId addCircleSketch(std::string sketchName, double radius);

    /// Regular N-gon (for sweep profile).
    FeatureId addPolygonSketch(std::string sketchName, int sides, double circumradius);

    // ── Solid features ───────────────────────────────────────────────────────
    /// Revolve a rectangle sketch profile around the Y-axis by the given angle (degrees).
    FeatureId addRevolve(std::string name, FeatureId sketchId, double angleDeg);

    /// Sweep a circle sketch along a straight path of the given length.
    FeatureId addSweep(std::string name, FeatureId profileSketchId, double pathLength);

    /// Loft between two sketch profiles; both must be rectangle or circle sketches.
    FeatureId addLoft(std::string name, FeatureId sketchA, FeatureId sketchB);

    /// Fillet (edge rounding) on the previous solid feature.
    FeatureId addFillet(std::string name, FeatureId solidFeatureId, double radius);

    /// Chamfer (edge bevel) on the previous solid feature.
    FeatureId addChamfer(std::string name, FeatureId solidFeatureId, double distance);

    /// Shell: hollow the body, removing the top face, with a wall thickness.
    FeatureId addShell(std::string name, FeatureId solidFeatureId, double thickness);

    /// Rectangular pattern: instances a source feature N × M times along X and Y.
    FeatureId addRectangularPattern(std::string name, FeatureId sourceFeatureId,
                                    int countX, double spacingX,
                                    int countY, double spacingY);

    /// Mirror: reflects a source feature about a work-plane normal to Z.
    FeatureId addMirrorFeature(std::string name, FeatureId sourceFeatureId, double mirrorX);

    /// Adds a named work-plane datum.
    FeatureId addWorkPlane(std::string name, double nx, double ny, double nz,
                           double ox, double oy, double oz);

    /// Cylindrical hole boolean cut on a target solid feature.
    FeatureId addHole(std::string holeName, FeatureId targetSolidFeature, double radius,
                      double depth, double posX, double posY);

    /// Cosmetic or analytical thread on the last hole feature.
    FeatureId addThread(std::string name, FeatureId holeFeatureId, double pitch, int starts = 1);

    /// Rib: thin web extruded from a sketch, half-depth each side.
    FeatureId addRib(std::string name, FeatureId sketchId, double thickness);

    /// Emboss: raises a circle profile on the last solid face.
    FeatureId addEmboss(std::string name, FeatureId solidFeatureId,
                        double circleRadius, double height);

    // ── Measurement ─────────────────────────────────────────────────────────
    /// Measure the bounding-box diagonal of the current solid.
    [[nodiscard]] MeasureResult measureOverallDimensions() const;

    /// Measure surface area of the current solid.
    [[nodiscard]] MeasureResult measureSurfaceArea() const;

    // ── Work planes ──────────────────────────────────────────────────────────
    [[nodiscard]] const std::vector<WorkPlane>& workPlanes() const { return workPlanes_; }

    // ── Feature graph ────────────────────────────────────────────────────────
    [[nodiscard]] const std::vector<Feature>& features() const { return features_; }
    [[nodiscard]] const Feature* findFeature(FeatureId id) const;

    // ── Regeneration ─────────────────────────────────────────────────────────
    RegenerationReport regenerate();
    [[nodiscard]] const TopoDS_Shape& shape()       const { return shape_; }
    [[nodiscard]] double solidVolume()              const { return solidVolume_; }
    [[nodiscard]] bool   hasSolid()                const { return hasSolid_; }
    [[nodiscard]] double surfaceArea()              const { return surfaceArea_; }

    // ── Persistence ──────────────────────────────────────────────────────────
    [[nodiscard]] std::string serializeNative() const;
    static PartDocument deserializeNative(std::string_view json, std::string* error = nullptr);

    // ── Export ────────────────────────────────────────────────────────────────
    bool exportStep(const std::string& path, std::string* error = nullptr) const;
    bool exportIges(const std::string& path, std::string* error = nullptr) const;

private:
    FeatureId   allocateFeature();
    ParameterId allocateParameter();
    RegenerationReport evaluateFeature(const Feature& feature,
                                       std::unordered_map<FeatureId, TopoDS_Shape>& cache);

    std::string            name_{"Part1"};
    std::vector<Parameter> parameters_;
    std::vector<Feature>   features_;
    std::vector<WorkPlane> workPlanes_;
    FeatureId   nextFeatureId_{1};
    ParameterId nextParameterId_{1};
    TopoDS_Shape shape_;
    double solidVolume_{0.0};
    double surfaceArea_{0.0};
    bool   hasSolid_{false};
};

} // namespace mydesk::mechanical
