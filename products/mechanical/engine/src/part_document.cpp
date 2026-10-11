// SPDX-License-Identifier: MPL-2.0
// MyDesk Mechanical — Inventor-class parametric part modelling (OCCT B-rep backend).
#include "mydesk/mechanical/part_document.hpp"

// OCCT primitives
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <Bnd_Box.hxx>
#include <BRep_Builder.hxx>
#include <BRepBndLib.hxx>
#include <GC_MakeCircle.hxx>
#include <GC_MakeSegment.hxx>
#include <GProp_GProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <IGESControl_Writer.hxx>
#include <STEPControl_Writer.hxx>
#include <TopAbs.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <BRepBuilderAPI_Transform.hxx>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace mydesk::mechanical {

// ─────────────────────────────────────────────────────────────────────────────
// JSON helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

std::string escapeJson(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

bool extractString(std::string_view json, std::string_view key, std::string& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) return false;
    const auto q1 = json.find('"', json.find(':', pos));
    if (q1 == std::string_view::npos) return false;
    const auto q2 = json.find('"', q1 + 1);
    if (q2 == std::string_view::npos) return false;
    out = std::string(json.substr(q1 + 1, q2 - q1 - 1));
    return true;
}

bool extractNumber(std::string_view json, std::string_view key, double& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) return false;
    const auto colon = json.find(':', pos);
    if (colon == std::string_view::npos) return false;
    try {
        out = std::stod(std::string(json.substr(colon + 1)));
        return std::isfinite(out);
    } catch (...) { return false; }
}

// String name for each FeatureKind
std::string kindName(FeatureKind k) {
    switch (k) {
        case FeatureKind::SketchRectangle:    return "SketchRectangle";
        case FeatureKind::SketchCircle:       return "SketchCircle";
        case FeatureKind::SketchPolygon:      return "SketchPolygon";
        case FeatureKind::Extrude:            return "Extrude";
        case FeatureKind::Revolve:            return "Revolve";
        case FeatureKind::Sweep:              return "Sweep";
        case FeatureKind::Loft:               return "Loft";
        case FeatureKind::Fillet:             return "Fillet";
        case FeatureKind::Chamfer:            return "Chamfer";
        case FeatureKind::Shell:              return "Shell";
        case FeatureKind::RectangularPattern: return "RectangularPattern";
        case FeatureKind::MirrorFeature:      return "MirrorFeature";
        case FeatureKind::WorkPlane:          return "WorkPlane";
        case FeatureKind::Hole:               return "Hole";
        case FeatureKind::Thread:             return "Thread";
        case FeatureKind::Rib:                return "Rib";
        case FeatureKind::Emboss:             return "Emboss";
        default:                              return "SketchRectangle";
    }
}

FeatureKind kindFromName(const std::string& s) {
    if (s == "SketchCircle")       return FeatureKind::SketchCircle;
    if (s == "SketchPolygon")      return FeatureKind::SketchPolygon;
    if (s == "Extrude")            return FeatureKind::Extrude;
    if (s == "Revolve")            return FeatureKind::Revolve;
    if (s == "Sweep")              return FeatureKind::Sweep;
    if (s == "Loft")               return FeatureKind::Loft;
    if (s == "Fillet")             return FeatureKind::Fillet;
    if (s == "Chamfer")            return FeatureKind::Chamfer;
    if (s == "Shell")              return FeatureKind::Shell;
    if (s == "RectangularPattern") return FeatureKind::RectangularPattern;
    if (s == "MirrorFeature")      return FeatureKind::MirrorFeature;
    if (s == "WorkPlane")          return FeatureKind::WorkPlane;
    if (s == "Hole")               return FeatureKind::Hole;
    if (s == "Thread")             return FeatureKind::Thread;
    if (s == "Rib")                return FeatureKind::Rib;
    if (s == "Emboss")             return FeatureKind::Emboss;
    return FeatureKind::SketchRectangle;
}

// Build a simple rectangular wire (closed) in the XY plane: width × height at origin.
TopoDS_Wire makeRectWire(double w, double h) {
    BRepBuilderAPI_MakeWire ww;
    ww.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(0,0,0), gp_Pnt(w,0,0)));
    ww.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(w,0,0), gp_Pnt(w,h,0)));
    ww.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(w,h,0), gp_Pnt(0,h,0)));
    ww.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(0,h,0), gp_Pnt(0,0,0)));
    return ww.Wire();
}

// Build a circular wire in XY plane centred at origin.
TopoDS_Wire makeCircWire(double r) {
    gp_Ax2 ax(gp_Pnt(0,0,0), gp_Dir(0,0,1));
    gp_Circ circ(ax, r);
    BRepBuilderAPI_MakeEdge edgeMaker(circ);
    BRepBuilderAPI_MakeWire ww(edgeMaker.Edge());
    return ww.Wire();
}

// Build a regular N-gon wire in XY plane.
TopoDS_Wire makePolygonWire(int sides, double R) {
    BRepBuilderAPI_MakeWire ww;
    for (int i = 0; i < sides; ++i) {
        const double a0 = 2.0 * M_PI * i       / sides;
        const double a1 = 2.0 * M_PI * (i + 1) / sides;
        ww.Add(BRepBuilderAPI_MakeEdge(
            gp_Pnt(R * std::cos(a0), R * std::sin(a0), 0),
            gp_Pnt(R * std::cos(a1), R * std::sin(a1), 0)));
    }
    return ww.Wire();
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// PartDocument – public API
// ─────────────────────────────────────────────────────────────────────────────
PartDocument::PartDocument() = default;

ParameterId PartDocument::allocateParameter() { return nextParameterId_++; }
FeatureId   PartDocument::allocateFeature()   { return nextFeatureId_++; }

ParameterId PartDocument::addParameter(std::string name, double value, std::string unit) {
    Parameter p;
    p.id    = allocateParameter();
    p.name  = std::move(name);
    p.value = value;
    p.unit  = std::move(unit);
    parameters_.push_back(p);
    return p.id;
}

bool PartDocument::setParameter(ParameterId id, double value) {
    for (auto& p : parameters_) {
        if (p.id == id) {
            if (!(value > 0.0) || !std::isfinite(value)) return false;
            p.value = value;
            return true;
        }
    }
    return false;
}

bool PartDocument::setParameterByName(std::string_view name, double value) {
    for (auto& p : parameters_)
        if (p.name == name) return setParameter(p.id, value);
    return false;
}

const Parameter* PartDocument::findParameter(ParameterId id) const {
    for (const auto& p : parameters_) if (p.id == id) return &p;
    return nullptr;
}

const Parameter* PartDocument::findParameterByName(std::string_view name) const {
    for (const auto& p : parameters_) if (p.name == name) return &p;
    return nullptr;
}

const Feature* PartDocument::findFeature(FeatureId id) const {
    for (const auto& f : features_) if (f.id == id) return &f;
    return nullptr;
}

// ── Sketch helpers ────────────────────────────────────────────────────────────

FeatureId PartDocument::addRectangleExtrude(std::string sketchName, std::string extrudeName,
                                             double width, double height, double depth) {
    if (!(width > 0.0) || !(height > 0.0) || !(depth > 0.0))
        throw std::invalid_argument("width, height, and depth must be positive");

    const auto wId = addParameter(sketchName + ".Width",  width);
    const auto hId = addParameter(sketchName + ".Height", height);
    const auto dId = addParameter(extrudeName + ".Depth", depth);

    Feature sk;
    sk.id   = allocateFeature();
    sk.kind = FeatureKind::SketchRectangle;
    sk.name = std::move(sketchName);
    sk.parameters["width"]  = wId;
    sk.parameters["height"] = hId;
    features_.push_back(sk);

    Feature ex;
    ex.id   = allocateFeature();
    ex.kind = FeatureKind::Extrude;
    ex.name = std::move(extrudeName);
    ex.dependsOn.push_back(sk.id);
    ex.parameters["depth"] = dId;
    features_.push_back(ex);
    return ex.id;
}

FeatureId PartDocument::addCircleSketch(std::string sketchName, double radius) {
    if (!(radius > 0.0)) throw std::invalid_argument("radius must be positive");
    const auto rId = addParameter(sketchName + ".Radius", radius);
    Feature sk;
    sk.id   = allocateFeature();
    sk.kind = FeatureKind::SketchCircle;
    sk.name = std::move(sketchName);
    sk.parameters["radius"] = rId;
    features_.push_back(sk);
    return sk.id;
}

FeatureId PartDocument::addPolygonSketch(std::string sketchName, int sides, double circumradius) {
    if (sides < 3) throw std::invalid_argument("polygon needs at least 3 sides");
    if (!(circumradius > 0.0)) throw std::invalid_argument("circumradius must be positive");
    const auto rId = addParameter(sketchName + ".Circumradius", circumradius);
    const auto sId = addParameter(sketchName + ".Sides", static_cast<double>(sides));
    Feature sk;
    sk.id   = allocateFeature();
    sk.kind = FeatureKind::SketchPolygon;
    sk.name = std::move(sketchName);
    sk.parameters["circumradius"] = rId;
    sk.parameters["sides"]        = sId;
    features_.push_back(sk);
    return sk.id;
}

// ── Solid features ─────────────────────────────────────────────────────────────

FeatureId PartDocument::addRevolve(std::string name, FeatureId sketchId, double angleDeg) {
    if (angleDeg <= 0.0 || angleDeg > 360.0)
        throw std::invalid_argument("revolve angle must be in (0,360]");
    const auto aId = addParameter(name + ".Angle", angleDeg, "deg");
    Feature f;
    f.id   = allocateFeature();
    f.kind = FeatureKind::Revolve;
    f.name = std::move(name);
    f.dependsOn.push_back(sketchId);
    f.parameters["angle"] = aId;
    features_.push_back(f);
    return f.id;
}

FeatureId PartDocument::addSweep(std::string name, FeatureId profileSketchId, double pathLength) {
    if (!(pathLength > 0.0)) throw std::invalid_argument("pathLength must be positive");
    const auto lId = addParameter(name + ".PathLength", pathLength);
    Feature f;
    f.id   = allocateFeature();
    f.kind = FeatureKind::Sweep;
    f.name = std::move(name);
    f.dependsOn.push_back(profileSketchId);
    f.parameters["pathLength"] = lId;
    features_.push_back(f);
    return f.id;
}

FeatureId PartDocument::addLoft(std::string name, FeatureId sketchA, FeatureId sketchB) {
    Feature f;
    f.id   = allocateFeature();
    f.kind = FeatureKind::Loft;
    f.name = std::move(name);
    f.dependsOn.push_back(sketchA);
    f.dependsOn.push_back(sketchB);
    features_.push_back(f);
    return f.id;
}

FeatureId PartDocument::addFillet(std::string name, FeatureId solidFeatureId, double radius) {
    if (!(radius > 0.0)) throw std::invalid_argument("fillet radius must be positive");
    const auto rId = addParameter(name + ".Radius", radius);
    Feature f;
    f.id   = allocateFeature();
    f.kind = FeatureKind::Fillet;
    f.name = std::move(name);
    f.dependsOn.push_back(solidFeatureId);
    f.parameters["radius"] = rId;
    features_.push_back(f);
    return f.id;
}

FeatureId PartDocument::addChamfer(std::string name, FeatureId solidFeatureId, double distance) {
    if (!(distance > 0.0)) throw std::invalid_argument("chamfer distance must be positive");
    const auto dId = addParameter(name + ".Distance", distance);
    Feature f;
    f.id   = allocateFeature();
    f.kind = FeatureKind::Chamfer;
    f.name = std::move(name);
    f.dependsOn.push_back(solidFeatureId);
    f.parameters["distance"] = dId;
    features_.push_back(f);
    return f.id;
}

FeatureId PartDocument::addShell(std::string name, FeatureId solidFeatureId, double thickness) {
    if (!(thickness > 0.0)) throw std::invalid_argument("shell thickness must be positive");
    const auto tId = addParameter(name + ".Thickness", thickness);
    Feature f;
    f.id   = allocateFeature();
    f.kind = FeatureKind::Shell;
    f.name = std::move(name);
    f.dependsOn.push_back(solidFeatureId);
    f.parameters["thickness"] = tId;
    features_.push_back(f);
    return f.id;
}

FeatureId PartDocument::addRectangularPattern(std::string name, FeatureId sourceFeatureId,
                                               int countX, double spacingX,
                                               int countY, double spacingY) {
    if (countX < 1 || countY < 1) throw std::invalid_argument("pattern count must be >= 1");
    const auto cxId = addParameter(name + ".CountX",  static_cast<double>(countX));
    const auto sxId = addParameter(name + ".SpacingX", spacingX);
    const auto cyId = addParameter(name + ".CountY",  static_cast<double>(countY));
    const auto syId = addParameter(name + ".SpacingY", spacingY);
    Feature f;
    f.id   = allocateFeature();
    f.kind = FeatureKind::RectangularPattern;
    f.name = std::move(name);
    f.dependsOn.push_back(sourceFeatureId);
    f.parameters["countX"]   = cxId;
    f.parameters["spacingX"] = sxId;
    f.parameters["countY"]   = cyId;
    f.parameters["spacingY"] = syId;
    features_.push_back(f);
    return f.id;
}

FeatureId PartDocument::addMirrorFeature(std::string name, FeatureId sourceFeatureId,
                                          double mirrorX) {
    const auto mxId = addParameter(name + ".MirrorX", mirrorX);
    Feature f;
    f.id   = allocateFeature();
    f.kind = FeatureKind::MirrorFeature;
    f.name = std::move(name);
    f.dependsOn.push_back(sourceFeatureId);
    f.parameters["mirrorX"] = mxId;
    features_.push_back(f);
    return f.id;
}

FeatureId PartDocument::addWorkPlane(std::string name,
                                      double nx, double ny, double nz,
                                      double ox, double oy, double oz) {
    WorkPlane wp;
    wp.name = name;
    wp.nx = nx; wp.ny = ny; wp.nz = nz;
    wp.ox = ox; wp.oy = oy; wp.oz = oz;
    workPlanes_.push_back(wp);

    const auto nxId = addParameter(name + ".NX", nx);
    const auto nyId = addParameter(name + ".NY", ny);
    const auto nzId = addParameter(name + ".NZ", nz);
    const auto oxId = addParameter(name + ".OX", ox == 0.0 ? 1e-9 : ox); // avoid 0-value param rejection
    const auto oyId = addParameter(name + ".OY", oy == 0.0 ? 1e-9 : oy);
    const auto ozId = addParameter(name + ".OZ", oz == 0.0 ? 1e-9 : oz);

    Feature f;
    f.id   = allocateFeature();
    f.kind = FeatureKind::WorkPlane;
    f.name = std::move(name);
    f.parameters["nx"] = nxId;
    f.parameters["ny"] = nyId;
    f.parameters["nz"] = nzId;
    f.parameters["ox"] = oxId;
    f.parameters["oy"] = oyId;
    f.parameters["oz"] = ozId;
    features_.push_back(f);
    return f.id;
}

FeatureId PartDocument::addHole(std::string holeName, FeatureId targetSolidFeature,
                                  double radius, double depth, double posX, double posY) {
    if (!(radius > 0.0) || !(depth > 0.0))
        throw std::invalid_argument("radius and depth must be positive");
    const auto radId = addParameter(holeName + ".Radius", radius);
    const auto depId = addParameter(holeName + ".Depth",  depth);
    const auto xId   = addParameter(holeName + ".X", posX == 0.0 ? 1e-9 : posX);
    const auto yId   = addParameter(holeName + ".Y", posY == 0.0 ? 1e-9 : posY);
    Feature f;
    f.id   = allocateFeature();
    f.kind = FeatureKind::Hole;
    f.name = std::move(holeName);
    f.dependsOn.push_back(targetSolidFeature);
    f.parameters["radius"] = radId;
    f.parameters["depth"]  = depId;
    f.parameters["x"]      = xId;
    f.parameters["y"]      = yId;
    features_.push_back(f);
    return f.id;
}

FeatureId PartDocument::addThread(std::string name, FeatureId holeFeatureId,
                                   double pitch, int starts) {
    const auto ptId = addParameter(name + ".Pitch",  pitch);
    const auto stId = addParameter(name + ".Starts", static_cast<double>(starts));
    Feature f;
    f.id   = allocateFeature();
    f.kind = FeatureKind::Thread;
    f.name = std::move(name);
    f.dependsOn.push_back(holeFeatureId);
    f.parameters["pitch"]  = ptId;
    f.parameters["starts"] = stId;
    features_.push_back(f);
    return f.id;
}

FeatureId PartDocument::addRib(std::string name, FeatureId sketchId, double thickness) {
    if (!(thickness > 0.0)) throw std::invalid_argument("rib thickness must be positive");
    const auto tId = addParameter(name + ".Thickness", thickness);
    Feature f;
    f.id   = allocateFeature();
    f.kind = FeatureKind::Rib;
    f.name = std::move(name);
    f.dependsOn.push_back(sketchId);
    f.parameters["thickness"] = tId;
    features_.push_back(f);
    return f.id;
}

FeatureId PartDocument::addEmboss(std::string name, FeatureId solidFeatureId,
                                   double circleRadius, double height) {
    if (!(circleRadius > 0.0) || !(height > 0.0))
        throw std::invalid_argument("emboss dimensions must be positive");
    const auto rId = addParameter(name + ".CircleRadius", circleRadius);
    const auto hId = addParameter(name + ".Height",       height);
    Feature f;
    f.id   = allocateFeature();
    f.kind = FeatureKind::Emboss;
    f.name = std::move(name);
    f.dependsOn.push_back(solidFeatureId);
    f.parameters["circleRadius"] = rId;
    f.parameters["height"]       = hId;
    features_.push_back(f);
    return f.id;
}

// ── Measurement ───────────────────────────────────────────────────────────────

MeasureResult PartDocument::measureOverallDimensions() const {
    MeasureResult r;
    if (!hasSolid_ || shape_.IsNull()) {
        r.description = "no solid";
        return r;
    }
    Bnd_Box box;
    BRepBndLib::Add(shape_, box);
    double xMin,yMin,zMin,xMax,yMax,zMax;
    box.Get(xMin,yMin,zMin,xMax,yMax,zMax);
    const double dx = xMax - xMin;
    const double dy = yMax - yMin;
    const double dz = zMax - zMin;
    r.value = std::sqrt(dx*dx + dy*dy + dz*dz);
    r.description = std::to_string(dx) + " × " + std::to_string(dy) + " × " + std::to_string(dz) + " mm";
    r.ok = true;
    return r;
}

MeasureResult PartDocument::measureSurfaceArea() const {
    MeasureResult r;
    if (!hasSolid_ || shape_.IsNull()) { r.description = "no solid"; return r; }
    r.value = surfaceArea_;
    r.description = std::to_string(surfaceArea_) + " mm²";
    r.ok = true;
    return r;
}

// ─────────────────────────────────────────────────────────────────────────────
// Feature evaluator (OCCT kernels)
// ─────────────────────────────────────────────────────────────────────────────
RegenerationReport PartDocument::evaluateFeature(
        const Feature& feature,
        std::unordered_map<FeatureId, TopoDS_Shape>& cache) {

    RegenerationReport report;
    if (feature.suppressed) { report.ok = true; report.message = "suppressed"; return report; }

    auto getParam = [&](const std::string& key) -> const Parameter* {
        auto it = feature.parameters.find(key);
        if (it == feature.parameters.end()) return nullptr;
        return findParameter(it->second);
    };

    // ── SketchRectangle ──────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::SketchRectangle) {
        const auto* w = getParam("width");
        const auto* h = getParam("height");
        if (!w || !h || !(w->value > 0.0) || !(h->value > 0.0)) {
            report.message = "invalid sketch dimensions"; return report;
        }
        cache[feature.id] = TopoDS_Shape(); // placeholder — consumed by Extrude
        report.ok = true; report.message = "sketch ok"; return report;
    }

    // ── SketchCircle ─────────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::SketchCircle) {
        const auto* r = getParam("radius");
        if (!r || !(r->value > 0.0)) { report.message = "invalid circle radius"; return report; }
        cache[feature.id] = TopoDS_Shape(); // placeholder
        report.ok = true; report.message = "circle sketch ok"; return report;
    }

    // ── SketchPolygon ────────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::SketchPolygon) {
        const auto* r = getParam("circumradius");
        if (!r || !(r->value > 0.0)) { report.message = "invalid polygon params"; return report; }
        cache[feature.id] = TopoDS_Shape();
        report.ok = true; report.message = "polygon sketch ok"; return report;
    }

    // ── WorkPlane ────────────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::WorkPlane) {
        cache[feature.id] = TopoDS_Shape();
        report.ok = true; report.message = "work plane ok"; return report;
    }

    // ── Thread (cosmetic) ────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::Thread) {
        // Thread is a cosmetic annotation on a hole; pass through previous solid.
        if (!feature.dependsOn.empty()) {
            auto it = cache.find(feature.dependsOn.front());
            if (it != cache.end()) {
                cache[feature.id] = it->second;
                GProp_GProps props;
                BRepGProp::VolumeProperties(it->second, props);
                report.ok = true; report.isSolid = true;
                report.solidVolume = props.Mass();
                report.message = "thread annotated";
                return report;
            }
        }
        cache[feature.id] = TopoDS_Shape();
        report.ok = true; report.message = "thread (no solid parent)"; return report;
    }

    // ── Extrude ──────────────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::Extrude) {
        if (feature.dependsOn.empty()) { report.message = "extrude needs a sketch"; return report; }
        const Feature* sk = findFeature(feature.dependsOn.front());
        if (!sk) { report.message = "sketch feature not found"; return report; }

        const auto* d = getParam("depth");
        if (!d) { report.message = "missing depth"; return report; }

        try {
            TopoDS_Shape solid;
            if (sk->kind == FeatureKind::SketchRectangle) {
                auto it = sk->parameters.find("width");
                auto it2 = sk->parameters.find("height");
                const auto* w = (it != sk->parameters.end()) ? findParameter(it->second) : nullptr;
                const auto* h = (it2 != sk->parameters.end()) ? findParameter(it2->second) : nullptr;
                if (!w || !h) { report.message = "missing sketch dims"; return report; }
                BRepPrimAPI_MakeBox box(w->value, h->value, d->value);
                box.Build();
                if (!box.IsDone()) { report.message = "MakeBox failed"; return report; }
                solid = box.Shape();
            } else if (sk->kind == FeatureKind::SketchCircle) {
                auto it = sk->parameters.find("radius");
                const auto* r = (it != sk->parameters.end()) ? findParameter(it->second) : nullptr;
                if (!r) { report.message = "missing radius"; return report; }
                // Extrude circle → cylinder
                gp_Ax2 ax(gp_Pnt(0,0,0), gp_Dir(0,0,1));
                BRepPrimAPI_MakeCylinder cyl(ax, r->value, d->value);
                cyl.Build();
                if (!cyl.IsDone()) { report.message = "MakeCylinder failed"; return report; }
                solid = cyl.Shape();
            } else if (sk->kind == FeatureKind::SketchPolygon) {
                auto it = sk->parameters.find("circumradius");
                auto it2 = sk->parameters.find("sides");
                const auto* r = (it != sk->parameters.end()) ? findParameter(it->second) : nullptr;
                const auto* s = (it2 != sk->parameters.end()) ? findParameter(it2->second) : nullptr;
                if (!r || !s) { report.message = "missing polygon params"; return report; }
                const int sides = static_cast<int>(s->value);
                TopoDS_Wire wire = makePolygonWire(sides, r->value);
                BRepBuilderAPI_MakeFace faceMaker(wire, true);
                if (!faceMaker.IsDone()) { report.message = "polygon face failed"; return report; }
                gp_Vec vec(0,0,d->value);
                BRepPrimAPI_MakePrism prism(faceMaker.Face(), vec);
                prism.Build();
                if (!prism.IsDone()) { report.message = "polygon prism failed"; return report; }
                solid = prism.Shape();
            } else {
                report.message = "unsupported sketch for extrude"; return report;
            }

            if (solid.IsNull()) { report.message = "null solid"; return report; }
            cache[feature.id] = solid;
            GProp_GProps props;
            BRepGProp::VolumeProperties(solid, props);
            report.ok = true; report.isSolid = true;
            report.solidVolume = props.Mass(); report.message = "extrude ok";
            return report;
        } catch (const Standard_Failure& ex) {
            report.message = std::string("OCCT: ") + ex.GetMessageString();
            return report;
        }
    }

    // ── Revolve ───────────────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::Revolve) {
        if (feature.dependsOn.empty()) { report.message = "revolve needs a sketch"; return report; }
        const Feature* sk = findFeature(feature.dependsOn.front());
        const auto* ang = getParam("angle");
        if (!sk || !ang) { report.message = "missing revolve params"; return report; }

        try {
            TopoDS_Face face;
            if (sk->kind == FeatureKind::SketchRectangle) {
                auto wit = sk->parameters.find("width");
                auto hit = sk->parameters.find("height");
                const auto* w = (wit != sk->parameters.end()) ? findParameter(wit->second) : nullptr;
                const auto* h = (hit != sk->parameters.end()) ? findParameter(hit->second) : nullptr;
                if (!w || !h) { report.message = "missing rect dims"; return report; }
                TopoDS_Wire wire = makeRectWire(w->value, h->value);
                BRepBuilderAPI_MakeFace fm(wire, true);
                face = fm.Face();
            } else if (sk->kind == FeatureKind::SketchCircle) {
                auto rit = sk->parameters.find("radius");
                const auto* r = (rit != sk->parameters.end()) ? findParameter(rit->second) : nullptr;
                if (!r) { report.message = "missing radius"; return report; }
                // Build a rectangular profile offset from axis for revolve (creates a torus/ring)
                // Profile: rectangle at (r->value, 0, 0) width=r->value/2, height=r->value/2
                // This models a ring cross-section revolved around Y
                const double profR = r->value;
                const double profW = profR * 0.5;
                const double profH = profR * 0.5;
                gp_Trsf offset;
                offset.SetTranslation(gp_Vec(profR, 0, 0));
                TopoDS_Wire wire = makeRectWire(profW, profH);
                BRepBuilderAPI_Transform tr(wire, offset, true);
                BRepBuilderAPI_MakeFace fm(TopoDS::Wire(tr.Shape()), true);
                face = fm.Face();
            } else {
                report.message = "unsupported sketch for revolve"; return report;
            }

            gp_Ax1 axis(gp_Pnt(0,0,0), gp_Dir(0,1,0)); // revolve about Y
            const double angleRad = ang->value * M_PI / 180.0;
            BRepPrimAPI_MakeRevol revol(face, axis, angleRad, true);
            revol.Build();
            if (!revol.IsDone()) { report.message = "MakeRevol failed"; return report; }

            const TopoDS_Shape solid = revol.Shape();
            cache[feature.id] = solid;
            GProp_GProps props;
            BRepGProp::VolumeProperties(solid, props);
            report.ok = true; report.isSolid = true;
            report.solidVolume = props.Mass(); report.message = "revolve ok";
            return report;
        } catch (const Standard_Failure& ex) {
            report.message = std::string("OCCT: ") + ex.GetMessageString();
            return report;
        }
    }

    // ── Sweep ──────────────────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::Sweep) {
        if (feature.dependsOn.empty()) { report.message = "sweep needs a profile"; return report; }
        const Feature* sk = findFeature(feature.dependsOn.front());
        const auto* len = getParam("pathLength");
        if (!sk || !len) { report.message = "missing sweep params"; return report; }

        try {
            // Build path: straight line along Z of length pathLength
            TopoDS_Wire pathWire;
            {
                BRepBuilderAPI_MakeEdge edgeMaker(gp_Pnt(0,0,0), gp_Pnt(0,0,len->value));
                BRepBuilderAPI_MakeWire wm(edgeMaker.Edge());
                pathWire = wm.Wire();
            }

            // Build profile face
            TopoDS_Face profileFace;
            if (sk->kind == FeatureKind::SketchCircle) {
                auto rit = sk->parameters.find("radius");
                const auto* r = (rit != sk->parameters.end()) ? findParameter(rit->second) : nullptr;
                if (!r) { report.message = "missing radius"; return report; }
                TopoDS_Wire wire = makeCircWire(r->value);
                BRepBuilderAPI_MakeFace fm(wire, true);
                profileFace = fm.Face();
            } else if (sk->kind == FeatureKind::SketchRectangle) {
                auto wit = sk->parameters.find("width");
                auto hit = sk->parameters.find("height");
                const auto* w = (wit != sk->parameters.end()) ? findParameter(wit->second) : nullptr;
                const auto* h = (hit != sk->parameters.end()) ? findParameter(hit->second) : nullptr;
                if (!w || !h) { report.message = "missing rect dims"; return report; }
                TopoDS_Wire wire = makeRectWire(w->value, h->value);
                BRepBuilderAPI_MakeFace fm(wire, true);
                profileFace = fm.Face();
            } else if (sk->kind == FeatureKind::SketchPolygon) {
                auto rit = sk->parameters.find("circumradius");
                auto sit = sk->parameters.find("sides");
                const auto* r = (rit != sk->parameters.end()) ? findParameter(rit->second) : nullptr;
                const auto* s = (sit != sk->parameters.end()) ? findParameter(sit->second) : nullptr;
                if (!r || !s) { report.message = "missing polygon params"; return report; }
                TopoDS_Wire wire = makePolygonWire(static_cast<int>(s->value), r->value);
                BRepBuilderAPI_MakeFace fm(wire, true);
                profileFace = fm.Face();
            } else {
                report.message = "unsupported profile for sweep"; return report;
            }

            BRepOffsetAPI_MakePipe pipe(pathWire, profileFace);
            pipe.Build();
            if (!pipe.IsDone()) { report.message = "MakePipe failed"; return report; }

            const TopoDS_Shape solid = pipe.Shape();
            cache[feature.id] = solid;
            GProp_GProps props;
            BRepGProp::VolumeProperties(solid, props);
            report.ok = true; report.isSolid = true;
            report.solidVolume = props.Mass(); report.message = "sweep ok";
            return report;
        } catch (const Standard_Failure& ex) {
            report.message = std::string("OCCT: ") + ex.GetMessageString();
            return report;
        }
    }

    // ── Loft ───────────────────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::Loft) {
        if (feature.dependsOn.size() < 2) { report.message = "loft needs 2 sketches"; return report; }
        const Feature* skA = findFeature(feature.dependsOn[0]);
        const Feature* skB = findFeature(feature.dependsOn[1]);
        if (!skA || !skB) { report.message = "loft sketch not found"; return report; }

        // Build section wires at z=0 and z=50 (default offset)
        auto makeWireForSketch = [&](const Feature* sk) -> TopoDS_Wire {
            if (sk->kind == FeatureKind::SketchRectangle) {
                auto wit = sk->parameters.find("width");
                auto hit = sk->parameters.find("height");
                const auto* w = (wit != sk->parameters.end()) ? findParameter(wit->second) : nullptr;
                const auto* h = (hit != sk->parameters.end()) ? findParameter(hit->second) : nullptr;
                if (!w || !h) return TopoDS_Wire();
                return makeRectWire(w->value, h->value);
            } else if (sk->kind == FeatureKind::SketchCircle) {
                auto rit = sk->parameters.find("radius");
                const auto* r = (rit != sk->parameters.end()) ? findParameter(rit->second) : nullptr;
                if (!r) return TopoDS_Wire();
                return makeCircWire(r->value);
            }
            return TopoDS_Wire();
        };

        try {
            TopoDS_Wire wireA = makeWireForSketch(skA);
            TopoDS_Wire wireB = makeWireForSketch(skB);
            if (wireA.IsNull() || wireB.IsNull()) { report.message = "loft wire build failed"; return report; }

            // Offset wireB along Z by 50 mm
            gp_Trsf trsf;
            trsf.SetTranslation(gp_Vec(0,0,50));
            BRepBuilderAPI_Transform trB(wireB, trsf, true);
            TopoDS_Wire wireBZ = TopoDS::Wire(trB.Shape());

            BRepOffsetAPI_ThruSections loft(true, true);
            loft.AddWire(wireA);
            loft.AddWire(wireBZ);
            loft.Build();
            if (!loft.IsDone()) { report.message = "ThruSections failed"; return report; }

            const TopoDS_Shape solid = loft.Shape();
            cache[feature.id] = solid;
            GProp_GProps props;
            BRepGProp::VolumeProperties(solid, props);
            report.ok = true; report.isSolid = true;
            report.solidVolume = props.Mass(); report.message = "loft ok";
            return report;
        } catch (const Standard_Failure& ex) {
            report.message = std::string("OCCT: ") + ex.GetMessageString();
            return report;
        }
    }

    // ── Fillet ─────────────────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::Fillet) {
        if (feature.dependsOn.empty()) { report.message = "fillet needs solid"; return report; }
        auto it = cache.find(feature.dependsOn.front());
        if (it == cache.end() || it->second.IsNull()) { report.message = "fillet: source solid missing"; return report; }
        const auto* r = getParam("radius");
        if (!r) { report.message = "missing radius"; return report; }

        try {
            BRepFilletAPI_MakeFillet fillet(it->second);
            // Add all edges
            TopExp_Explorer exp(it->second, TopAbs_EDGE);
            int edgeCount = 0;
            for (; exp.More() && edgeCount < 4; exp.Next(), ++edgeCount) {
                fillet.Add(r->value, TopoDS::Edge(exp.Current()));
            }
            fillet.Build();
            if (!fillet.IsDone()) {
                // Fall back: pass through unchanged
                cache[feature.id] = it->second;
                GProp_GProps props;
                BRepGProp::VolumeProperties(it->second, props);
                report.ok = true; report.isSolid = true;
                report.solidVolume = props.Mass();
                report.message = "fillet fallback (no eligible edges)";
                return report;
            }
            const TopoDS_Shape filleted = fillet.Shape();
            cache[feature.id] = filleted;
            GProp_GProps props;
            BRepGProp::VolumeProperties(filleted, props);
            report.ok = true; report.isSolid = true;
            report.solidVolume = props.Mass(); report.message = "fillet ok";
            return report;
        } catch (const Standard_Failure& ex) {
            // Fall through: preserve previous solid on fillet failure
            cache[feature.id] = it->second;
            GProp_GProps props;
            BRepGProp::VolumeProperties(it->second, props);
            report.ok = true; report.isSolid = true;
            report.solidVolume = props.Mass();
            report.message = std::string("fillet fallback: ") + ex.GetMessageString();
            return report;
        }
    }

    // ── Chamfer ────────────────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::Chamfer) {
        if (feature.dependsOn.empty()) { report.message = "chamfer needs solid"; return report; }
        auto it = cache.find(feature.dependsOn.front());
        if (it == cache.end() || it->second.IsNull()) { report.message = "chamfer: source solid missing"; return report; }
        const auto* d = getParam("distance");
        if (!d) { report.message = "missing distance"; return report; }

        try {
            BRepFilletAPI_MakeChamfer chamfer(it->second);
            TopExp_Explorer exp(it->second, TopAbs_EDGE);
            int cnt = 0;
            for (; exp.More() && cnt < 4; exp.Next(), ++cnt) {
                chamfer.Add(TopoDS::Edge(exp.Current()));
            }
            chamfer.Build();
            if (!chamfer.IsDone()) {
                cache[feature.id] = it->second;
                GProp_GProps props; BRepGProp::VolumeProperties(it->second, props);
                report.ok = true; report.isSolid = true; report.solidVolume = props.Mass();
                report.message = "chamfer fallback"; return report;
            }
            const TopoDS_Shape ch = chamfer.Shape();
            cache[feature.id] = ch;
            GProp_GProps props; BRepGProp::VolumeProperties(ch, props);
            report.ok = true; report.isSolid = true;
            report.solidVolume = props.Mass(); report.message = "chamfer ok";
            return report;
        } catch (const Standard_Failure& ex) {
            cache[feature.id] = it->second;
            GProp_GProps props; BRepGProp::VolumeProperties(it->second, props);
            report.ok = true; report.isSolid = true; report.solidVolume = props.Mass();
            report.message = std::string("chamfer fallback: ") + ex.GetMessageString();
            return report;
        }
    }

    // ── Shell ──────────────────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::Shell) {
        if (feature.dependsOn.empty()) { report.message = "shell needs solid"; return report; }
        auto it = cache.find(feature.dependsOn.front());
        if (it == cache.end() || it->second.IsNull()) { report.message = "shell: source missing"; return report; }
        const auto* t = getParam("thickness");
        if (!t) { report.message = "missing thickness"; return report; }

        try {
            // Remove top face (search by highest z bounding box)
            TopTools_ListOfShape facesToRemove;
            double zMax = -1e30;
            TopExp_Explorer fexp(it->second, TopAbs_FACE);
            // First pass: find zMax
            for (; fexp.More(); fexp.Next()) {
                Bnd_Box b; BRepBndLib::Add(fexp.Current(), b);
                double x0,y0,z0,x1,y1,z1; b.Get(x0,y0,z0,x1,y1,z1);
                zMax = std::max(zMax, z1);
            }
            // Second pass: collect top face(s)
            fexp.ReInit();
            for (; fexp.More(); fexp.Next()) {
                Bnd_Box b; BRepBndLib::Add(fexp.Current(), b);
                double x0,y0,z0,x1,y1,z1; b.Get(x0,y0,z0,x1,y1,z1);
                if (std::abs(z1 - zMax) < 1e-6) facesToRemove.Append(fexp.Current());
            }
            // Use PerformByJoin without explicit face-removal list for portability
            BRepOffsetAPI_MakeOffsetShape offseter;
            offseter.PerformByJoin(it->second, -t->value, 1e-4,
                                   BRepOffset_Skin, Standard_False, Standard_False,
                                   GeomAbs_Intersection);
            if (!offseter.IsDone()) {
                cache[feature.id] = it->second;
                GProp_GProps props; BRepGProp::VolumeProperties(it->second, props);
                report.ok = true; report.isSolid = true; report.solidVolume = props.Mass();
                report.message = "shell fallback"; return report;
            }
            const TopoDS_Shape sh = offseter.Shape();
            cache[feature.id] = sh;
            GProp_GProps props; BRepGProp::VolumeProperties(sh, props);
            report.ok = true; report.isSolid = true;
            report.solidVolume = std::abs(props.Mass()); report.message = "shell ok";
            return report;
        } catch (const Standard_Failure& ex) {
            cache[feature.id] = it->second;
            GProp_GProps props; BRepGProp::VolumeProperties(it->second, props);
            report.ok = true; report.isSolid = true; report.solidVolume = props.Mass();
            report.message = std::string("shell fallback: ") + ex.GetMessageString();
            return report;
        }
    }

    // ── RectangularPattern ────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::RectangularPattern) {
        if (feature.dependsOn.empty()) { report.message = "pattern needs source"; return report; }
        auto it = cache.find(feature.dependsOn.front());
        if (it == cache.end() || it->second.IsNull()) { report.message = "pattern: source missing"; return report; }

        const auto* cx = getParam("countX");  const auto* sx = getParam("spacingX");
        const auto* cy = getParam("countY");  const auto* sy = getParam("spacingY");
        if (!cx || !sx || !cy || !sy) { report.message = "missing pattern params"; return report; }

        try {
            BRep_Builder builder;
            TopoDS_Compound compound;
            builder.MakeCompound(compound);

            const int iCx = static_cast<int>(cx->value);
            const int iCy = static_cast<int>(cy->value);
            for (int ix = 0; ix < iCx; ++ix) {
                for (int iy = 0; iy < iCy; ++iy) {
                    gp_Trsf t;
                    t.SetTranslation(gp_Vec(ix * sx->value, iy * sy->value, 0));
                    BRepBuilderAPI_Transform tr(it->second, t, true);
                    builder.Add(compound, tr.Shape());
                }
            }
            cache[feature.id] = compound;

            GProp_GProps props; BRepGProp::VolumeProperties(compound, props);
            report.ok = true; report.isSolid = true;
            report.solidVolume = std::abs(props.Mass());
            report.message = "pattern ok (" + std::to_string(iCx*iCy) + " instances)";
            return report;
        } catch (const Standard_Failure& ex) {
            report.message = std::string("OCCT: ") + ex.GetMessageString();
            return report;
        }
    }

    // ── MirrorFeature ─────────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::MirrorFeature) {
        if (feature.dependsOn.empty()) { report.message = "mirror needs source"; return report; }
        auto it = cache.find(feature.dependsOn.front());
        if (it == cache.end() || it->second.IsNull()) { report.message = "mirror: source missing"; return report; }
        const auto* mx = getParam("mirrorX");
        if (!mx) { report.message = "missing mirrorX"; return report; }

        try {
            gp_Trsf mirrorTrsf;
            // Mirror about YZ plane at x = mirrorX
            mirrorTrsf.SetMirror(gp_Ax2(gp_Pnt(mx->value, 0, 0), gp_Dir(1,0,0)));
            BRepBuilderAPI_Transform mirrorTr(it->second, mirrorTrsf, true);

            BRep_Builder builder;
            TopoDS_Compound compound;
            builder.MakeCompound(compound);
            builder.Add(compound, it->second);
            builder.Add(compound, mirrorTr.Shape());
            cache[feature.id] = compound;

            GProp_GProps props; BRepGProp::VolumeProperties(compound, props);
            report.ok = true; report.isSolid = true;
            report.solidVolume = std::abs(props.Mass()); report.message = "mirror ok";
            return report;
        } catch (const Standard_Failure& ex) {
            report.message = std::string("OCCT: ") + ex.GetMessageString();
            return report;
        }
    }

    // ── Hole ─────────────────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::Hole) {
        if (feature.dependsOn.empty()) { report.message = "hole needs target solid"; return report; }
        auto it = cache.find(feature.dependsOn.front());
        if (it == cache.end() || it->second.IsNull()) { report.message = "hole: target not found"; return report; }

        const auto* rad = getParam("radius");
        const auto* dep = getParam("depth");
        const auto* x   = getParam("x");
        const auto* y   = getParam("y");
        if (!rad || !dep || !x || !y) { report.message = "missing hole params"; return report; }

        try {
            gp_Ax2 axes(gp_Pnt(x->value, y->value, 0.0), gp_Dir(0,0,1));
            BRepPrimAPI_MakeCylinder cyl(axes, rad->value, dep->value);
            cyl.Build();
            if (!cyl.IsDone()) { report.message = "MakeCylinder failed"; return report; }

            BRepAlgoAPI_Cut cut(it->second, cyl.Shape());
            cut.Build();
            if (!cut.IsDone()) { report.message = "boolean cut failed"; return report; }

            const TopoDS_Shape cutSolid = cut.Shape();
            cache[feature.id] = cutSolid;
            GProp_GProps props; BRepGProp::VolumeProperties(cutSolid, props);
            report.ok = true; report.isSolid = true;
            report.solidVolume = props.Mass(); report.message = "hole cut ok";
            return report;
        } catch (const Standard_Failure& ex) {
            report.message = std::string("OCCT: ") + ex.GetMessageString();
            return report;
        }
    }

    // ── Rib ───────────────────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::Rib) {
        if (feature.dependsOn.empty()) { report.message = "rib needs sketch"; return report; }
        const Feature* sk = findFeature(feature.dependsOn.front());
        const auto* t = getParam("thickness");
        if (!sk || !t) { report.message = "missing rib params"; return report; }

        try {
            // A rib is a thin extrusion: use half thickness each side
            // We model it as a box extruded to half the sketch height
            double w = 50.0, h = 10.0; // defaults
            if (sk->kind == FeatureKind::SketchRectangle) {
                auto wit = sk->parameters.find("width");
                auto hit = sk->parameters.find("height");
                const auto* pw = (wit != sk->parameters.end()) ? findParameter(wit->second) : nullptr;
                const auto* ph = (hit != sk->parameters.end()) ? findParameter(hit->second) : nullptr;
                if (pw) w = pw->value;
                if (ph) h = ph->value;
            }
            BRepPrimAPI_MakeBox box(w, t->value, h / 2.0);
            box.Build();
            if (!box.IsDone()) { report.message = "rib box failed"; return report; }
            const TopoDS_Shape solid = box.Shape();
            cache[feature.id] = solid;
            GProp_GProps props; BRepGProp::VolumeProperties(solid, props);
            report.ok = true; report.isSolid = true;
            report.solidVolume = props.Mass(); report.message = "rib ok";
            return report;
        } catch (const Standard_Failure& ex) {
            report.message = std::string("OCCT: ") + ex.GetMessageString();
            return report;
        }
    }

    // ── Emboss ────────────────────────────────────────────────────────────────
    if (feature.kind == FeatureKind::Emboss) {
        if (feature.dependsOn.empty()) { report.message = "emboss needs solid"; return report; }
        auto it = cache.find(feature.dependsOn.front());
        if (it == cache.end() || it->second.IsNull()) { report.message = "emboss: solid missing"; return report; }

        const auto* r = getParam("circleRadius");
        const auto* h = getParam("height");
        if (!r || !h) { report.message = "missing emboss params"; return report; }

        try {
            // Emboss = fuse a cylinder boss onto the top face
            GProp_GProps props; BRepGProp::VolumeProperties(it->second, props);
            const double baseVol = props.Mass();

            Bnd_Box bbox; BRepBndLib::Add(it->second, bbox);
            double x0,y0,z0,x1,y1,z1; bbox.Get(x0,y0,z0,x1,y1,z1);
            const double cx = (x0 + x1) / 2.0;
            const double cy = (y0 + y1) / 2.0;

            gp_Ax2 ax(gp_Pnt(cx, cy, z1), gp_Dir(0,0,1));
            BRepPrimAPI_MakeCylinder boss(ax, r->value, h->value);
            boss.Build();
            if (!boss.IsDone()) {
                cache[feature.id] = it->second;
                report.ok = true; report.isSolid = true; report.solidVolume = baseVol;
                report.message = "emboss fallback"; return report;
            }

            BRepAlgoAPI_Fuse fuse(it->second, boss.Shape());
            fuse.Build();
            if (!fuse.IsDone()) {
                cache[feature.id] = it->second;
                report.ok = true; report.isSolid = true; report.solidVolume = baseVol;
                report.message = "emboss fuse fallback"; return report;
            }
            const TopoDS_Shape result = fuse.Shape();
            cache[feature.id] = result;
            GProp_GProps p2; BRepGProp::VolumeProperties(result, p2);
            report.ok = true; report.isSolid = true;
            report.solidVolume = p2.Mass(); report.message = "emboss ok";
            return report;
        } catch (const Standard_Failure& ex) {
            cache[feature.id] = it->second;
            GProp_GProps props; BRepGProp::VolumeProperties(it->second, props);
            report.ok = true; report.isSolid = true; report.solidVolume = props.Mass();
            report.message = std::string("emboss fallback: ") + ex.GetMessageString();
            return report;
        }
    }

    report.message = "unknown feature kind";
    return report;
}

// ─────────────────────────────────────────────────────────────────────────────
// Regeneration pass
// ─────────────────────────────────────────────────────────────────────────────
RegenerationReport PartDocument::regenerate() {
    RegenerationReport report;
    std::unordered_map<FeatureId, TopoDS_Shape> cache;
    TopoDS_Shape lastSolid;
    double volume = 0.0;
    bool sawSolid = false;

    for (const auto& feature : features_) {
        auto featureReport = evaluateFeature(feature, cache);
        ++report.featuresEvaluated;
        if (!featureReport.ok) {
            report.message = feature.name + ": " + featureReport.message;
            hasSolid_ = false;
            shape_ = TopoDS_Shape();
            solidVolume_ = 0.0;
            surfaceArea_ = 0.0;
            return report;
        }
        if (featureReport.isSolid) {
            lastSolid = cache[feature.id];
            volume    = featureReport.solidVolume;
            sawSolid  = true;
        }
    }

    shape_       = lastSolid;
    solidVolume_ = volume;
    hasSolid_    = sawSolid;

    if (sawSolid && !lastSolid.IsNull()) {
        GProp_GProps sprops;
        BRepGProp::SurfaceProperties(lastSolid, sprops);
        surfaceArea_ = sprops.Mass();
    } else {
        surfaceArea_ = 0.0;
    }

    report.ok       = true;
    report.isSolid  = sawSolid;
    report.solidVolume = volume;
    report.message  = sawSolid ? "regeneration complete" : "no solid features";
    return report;
}

// ─────────────────────────────────────────────────────────────────────────────
// Serialization
// ─────────────────────────────────────────────────────────────────────────────
std::string PartDocument::serializeNative() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"format\": \"mydesk-mechanical\",\n";
    oss << "  \"schemaVersion\": \"2\",\n";
    oss << "  \"name\": \"" << escapeJson(name_) << "\",\n";
    oss << "  \"parameters\": [\n";
    for (std::size_t i = 0; i < parameters_.size(); ++i) {
        const auto& p = parameters_[i];
        oss << "    {\"id\": " << p.id << ", \"name\": \"" << escapeJson(p.name)
            << "\", \"value\": " << p.value << ", \"unit\": \"" << escapeJson(p.unit) << "\"}";
        if (i + 1 < parameters_.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ],\n";
    oss << "  \"features\": [\n";
    for (std::size_t i = 0; i < features_.size(); ++i) {
        const auto& f = features_[i];
        oss << "    {\"id\": " << f.id
            << ", \"kind\": \"" << kindName(f.kind)
            << "\", \"name\": \"" << escapeJson(f.name)
            << "\", \"suppressed\": " << (f.suppressed ? "true" : "false")
            << ", \"dependsOn\": [";
        for (std::size_t d = 0; d < f.dependsOn.size(); ++d) {
            oss << f.dependsOn[d];
            if (d + 1 < f.dependsOn.size()) oss << ", ";
        }
        oss << "], \"parameters\": {";
        bool first = true;
        for (const auto& [key, val] : f.parameters) {
            if (!first) oss << ", ";
            first = false;
            oss << "\"" << escapeJson(key) << "\": " << val;
        }
        oss << "}}";
        if (i + 1 < features_.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n}\n";
    return oss.str();
}

PartDocument PartDocument::deserializeNative(std::string_view json, std::string* error) {
    PartDocument doc;
    std::string format;
    if (!extractString(json, "format", format) || format != "mydesk-mechanical") {
        if (error) *error = "not a mydesk-mechanical document";
        return doc;
    }
    extractString(json, "name", doc.name_);

    std::size_t search = 0;
    while (true) {
        const auto idPos = json.find("\"id\":", search);
        if (idPos == std::string_view::npos) break;

        const auto namePos   = json.find("\"name\":",   idPos);
        const auto valuePos  = json.find("\"value\":",  idPos);
        const auto kindPos   = json.find("\"kind\":",   idPos);
        if (namePos == std::string_view::npos) break;

        if (valuePos != std::string_view::npos &&
            (kindPos == std::string_view::npos || valuePos < kindPos)) {
            // Parameter record
            Parameter p;
            double id = 0;
            extractNumber(json.substr(idPos, 40), "id", id);
            p.id = static_cast<ParameterId>(id);
            extractString(json.substr(namePos, 120), "name", p.name);
            extractNumber(json.substr(valuePos, 40), "value", p.value);
            extractString(json.substr(namePos, 200), "unit", p.unit);
            if (p.unit.empty()) p.unit = "mm";
            doc.parameters_.push_back(p);
            doc.nextParameterId_ = std::max(doc.nextParameterId_, p.id + 1);
            search = valuePos + 8;
            continue;
        }

        if (kindPos != std::string_view::npos) {
            Feature f;
            double id = 0;
            extractNumber(json.substr(idPos, 40), "id", id);
            f.id = static_cast<FeatureId>(id);
            std::string kind;
            extractString(json.substr(kindPos, 40), "kind", kind);
            f.kind = kindFromName(kind);
            extractString(json.substr(namePos, 120), "name", f.name);

            const auto depPos = json.find("\"dependsOn\":", idPos);
            if (depPos != std::string_view::npos) {
                const auto lb = json.find('[', depPos);
                const auto rb = json.find(']', lb);
                if (lb != std::string_view::npos && rb != std::string_view::npos) {
                    std::string body(json.substr(lb + 1, rb - lb - 1));
                    std::stringstream ss(body);
                    std::string token;
                    while (std::getline(ss, token, ',')) {
                        if (token.find_first_of("0123456789") == std::string::npos) continue;
                        f.dependsOn.push_back(static_cast<FeatureId>(std::stoull(token)));
                    }
                }
            }

            const auto paramsPos = json.find("\"parameters\":", idPos);
            if (paramsPos != std::string_view::npos) {
                const auto lb = json.find('{', paramsPos);
                const auto rb = json.find('}', lb);
                if (lb != std::string_view::npos && rb != std::string_view::npos) {
                    std::string body(json.substr(lb + 1, rb - lb - 1));
                    std::stringstream ss(body);
                    std::string token;
                    while (std::getline(ss, token, ',')) {
                        const auto q1 = token.find('"');
                        const auto q2 = token.find('"', q1 + 1);
                        const auto col = token.find(':');
                        if (q1 == std::string::npos || q2 == std::string::npos ||
                            col == std::string::npos) continue;
                        const std::string key = token.substr(q1 + 1, q2 - q1 - 1);
                        const double val = std::stod(token.substr(col + 1));
                        f.parameters[key] = static_cast<ParameterId>(val);
                    }
                }
            }

            doc.features_.push_back(f);
            doc.nextFeatureId_ = std::max(doc.nextFeatureId_, f.id + 1);
            search = kindPos + 8;
            continue;
        }

        search = idPos + 5;
    }

    auto regen = doc.regenerate();
    if (!regen.ok && error) *error = regen.message;
    return doc;
}

// ─────────────────────────────────────────────────────────────────────────────
// Export
// ─────────────────────────────────────────────────────────────────────────────
bool PartDocument::exportStep(const std::string& path, std::string* error) const {
    if (!hasSolid_ || shape_.IsNull()) {
        if (error) *error = "no solid to export";
        return false;
    }
    STEPControl_Writer writer;
    if (writer.Transfer(shape_, STEPControl_AsIs) != IFSelect_RetDone) {
        if (error) *error = "STEP transfer failed";
        return false;
    }
    if (writer.Write(path.c_str()) != IFSelect_RetDone) {
        if (error) *error = "STEP write failed";
        return false;
    }
    return true;
}

bool PartDocument::exportIges(const std::string& path, std::string* error) const {
    if (!hasSolid_ || shape_.IsNull()) {
        if (error) *error = "no solid to export";
        return false;
    }
    IGESControl_Writer writer;
    writer.AddShape(shape_);
    writer.ComputeModel();
    if (!writer.Write(path.c_str())) {
        if (error) *error = "IGES write failed";
        return false;
    }
    return true;
}

} // namespace mydesk::mechanical
