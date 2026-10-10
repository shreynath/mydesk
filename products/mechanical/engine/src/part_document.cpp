#include "mydesk/mechanical/part_document.hpp"

#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <GProp_GProps.hxx>
#include <STEPControl_Writer.hxx>
#include <TopAbs.hxx>
#include <TopoDS.hxx>
#include <IFSelect_ReturnStatus.hxx>

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace mydesk::mechanical {

namespace {

std::string escapeJson(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

bool extractString(std::string_view json, std::string_view key, std::string& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return false;
    }
    const auto q1 = json.find('"', json.find(':', pos));
    if (q1 == std::string_view::npos) {
        return false;
    }
    const auto q2 = json.find('"', q1 + 1);
    if (q2 == std::string_view::npos) {
        return false;
    }
    out = std::string(json.substr(q1 + 1, q2 - q1 - 1));
    return true;
}

bool extractNumber(std::string_view json, std::string_view key, double& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return false;
    }
    const auto colon = json.find(':', pos);
    if (colon == std::string_view::npos) {
        return false;
    }
    try {
        out = std::stod(std::string(json.substr(colon + 1)));
        return std::isfinite(out);
    } catch (...) {
        return false;
    }
}

} // namespace

PartDocument::PartDocument() = default;

ParameterId PartDocument::allocateParameter() { return nextParameterId_++; }
FeatureId PartDocument::allocateFeature() { return nextFeatureId_++; }

ParameterId PartDocument::addParameter(std::string name, double value, std::string unit) {
    Parameter p;
    p.id = allocateParameter();
    p.name = std::move(name);
    p.value = value;
    p.unit = std::move(unit);
    parameters_.push_back(p);
    return p.id;
}

bool PartDocument::setParameter(ParameterId id, double value) {
    for (auto& p : parameters_) {
        if (p.id == id) {
            if (!(value > 0.0) || !std::isfinite(value)) {
                return false;
            }
            p.value = value;
            return true;
        }
    }
    return false;
}

bool PartDocument::setParameterByName(std::string_view name, double value) {
    for (auto& p : parameters_) {
        if (p.name == name) {
            return setParameter(p.id, value);
        }
    }
    return false;
}

const Parameter* PartDocument::findParameter(ParameterId id) const {
    for (const auto& p : parameters_) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

const Parameter* PartDocument::findParameterByName(std::string_view name) const {
    for (const auto& p : parameters_) {
        if (p.name == name) {
            return &p;
        }
    }
    return nullptr;
}

const Feature* PartDocument::findFeature(FeatureId id) const {
    for (const auto& f : features_) {
        if (f.id == id) {
            return &f;
        }
    }
    return nullptr;
}

FeatureId PartDocument::addRectangleExtrude(std::string sketchName, std::string extrudeName,
                                            double width, double height, double depth) {
    if (!(width > 0.0) || !(height > 0.0) || !(depth > 0.0)) {
        throw std::invalid_argument("width, height, and depth must be positive");
    }
    const auto widthId = addParameter(sketchName + ".Width", width);
    const auto heightId = addParameter(sketchName + ".Height", height);
    const auto depthId = addParameter(extrudeName + ".Depth", depth);

    Feature sketch;
    sketch.id = allocateFeature();
    sketch.kind = FeatureKind::SketchRectangle;
    sketch.name = std::move(sketchName);
    sketch.parameters["width"] = widthId;
    sketch.parameters["height"] = heightId;
    features_.push_back(sketch);

    Feature extrude;
    extrude.id = allocateFeature();
    extrude.kind = FeatureKind::Extrude;
    extrude.name = std::move(extrudeName);
    extrude.dependsOn.push_back(sketch.id);
    extrude.parameters["depth"] = depthId;
    features_.push_back(extrude);
    return extrude.id;
}

RegenerationReport PartDocument::evaluateFeature(
    const Feature& feature, std::unordered_map<FeatureId, TopoDS_Shape>& cache) {
    RegenerationReport report;
    if (feature.suppressed) {
        report.ok = true;
        report.message = "suppressed";
        return report;
    }

    if (feature.kind == FeatureKind::SketchRectangle) {
        const auto* w = findParameter(feature.parameters.at("width"));
        const auto* h = findParameter(feature.parameters.at("height"));
        if (!w || !h || !(w->value > 0.0) || !(h->value > 0.0)) {
            report.message = "invalid sketch dimensions";
            return report;
        }
        // Sketch stores a planar rectangular wire extent as a zero-thickness box face proxy:
        // the Extrude feature consumes width/height parameters directly for B-rep construction.
        // Cache an empty marker shape; geometry is produced at extrude.
        cache[feature.id] = TopoDS_Shape();
        report.ok = true;
        report.message = "sketch ok";
        return report;
    }

    if (feature.kind == FeatureKind::Extrude) {
        if (feature.dependsOn.empty()) {
            report.message = "extrude requires a sketch dependency";
            return report;
        }
        const Feature* sketch = findFeature(feature.dependsOn.front());
        if (!sketch || sketch->kind != FeatureKind::SketchRectangle) {
            report.message = "extrude requires a rectangle sketch";
            return report;
        }
        const auto* w = findParameter(sketch->parameters.at("width"));
        const auto* h = findParameter(sketch->parameters.at("height"));
        const auto* d = findParameter(feature.parameters.at("depth"));
        if (!w || !h || !d) {
            report.message = "missing extrude parameters";
            return report;
        }
        try {
            // Closed rectangular profile extruded along +Z via OCCT B-rep primitive.
            // Equivalent to a planar sketch + prism for this first vertical slice.
            BRepPrimAPI_MakeBox maker(w->value, h->value, d->value);
            maker.Build();
            if (!maker.IsDone()) {
                report.message = "OCCT MakeBox failed";
                return report;
            }
            const TopoDS_Shape solid = maker.Shape();
            if (solid.IsNull() || solid.ShapeType() != TopAbs_SOLID) {
                report.message = "result is not a solid";
                return report;
            }
            cache[feature.id] = solid;
            GProp_GProps props;
            BRepGProp::VolumeProperties(solid, props);
            report.ok = true;
            report.isSolid = true;
            report.solidVolume = props.Mass();
            report.message = "extrude ok";
            return report;
        } catch (const Standard_Failure& ex) {
            report.message = std::string("OCCT error: ") + ex.GetMessageString();
            return report;
        }
    }

    report.message = "unknown feature kind";
    return report;
}

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
            return report;
        }
        if (featureReport.isSolid) {
            lastSolid = cache[feature.id];
            volume = featureReport.solidVolume;
            sawSolid = true;
        }
    }

    shape_ = lastSolid;
    solidVolume_ = volume;
    hasSolid_ = sawSolid;
    report.ok = true;
    report.isSolid = sawSolid;
    report.solidVolume = volume;
    report.message = sawSolid ? "regeneration complete" : "no solid features";
    return report;
}

std::string PartDocument::serializeNative() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"format\": \"mydesk-mechanical\",\n";
    oss << "  \"schemaVersion\": \"1\",\n";
    oss << "  \"name\": \"" << escapeJson(name_) << "\",\n";
    oss << "  \"parameters\": [\n";
    for (std::size_t i = 0; i < parameters_.size(); ++i) {
        const auto& p = parameters_[i];
        oss << "    {\"id\": " << p.id << ", \"name\": \"" << escapeJson(p.name)
            << "\", \"value\": " << p.value << ", \"unit\": \"" << escapeJson(p.unit) << "\"}";
        if (i + 1 < parameters_.size()) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << "  ],\n";
    oss << "  \"features\": [\n";
    for (std::size_t i = 0; i < features_.size(); ++i) {
        const auto& f = features_[i];
        oss << "    {\"id\": " << f.id << ", \"kind\": \""
            << (f.kind == FeatureKind::SketchRectangle ? "SketchRectangle" : "Extrude")
            << "\", \"name\": \"" << escapeJson(f.name) << "\", \"dependsOn\": [";
        for (std::size_t d = 0; d < f.dependsOn.size(); ++d) {
            oss << f.dependsOn[d];
            if (d + 1 < f.dependsOn.size()) {
                oss << ", ";
            }
        }
        oss << "], \"parameters\": {";
        bool first = true;
        for (const auto& [key, value] : f.parameters) {
            if (!first) {
                oss << ", ";
            }
            first = false;
            oss << "\"" << escapeJson(key) << "\": " << value;
        }
        oss << "}}";
        if (i + 1 < features_.size()) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << "  ]\n}\n";
    return oss.str();
}

PartDocument PartDocument::deserializeNative(std::string_view json, std::string* error) {
    PartDocument doc;
    std::string format;
    if (!extractString(json, "format", format) || format != "mydesk-mechanical") {
        if (error) {
            *error = "not a mydesk-mechanical document";
        }
        return doc;
    }
    extractString(json, "name", doc.name_);

    // Minimal parser for the controlled schema written by serializeNative.
    std::size_t search = 0;
    while (true) {
        const auto idPos = json.find("\"id\":", search);
        if (idPos == std::string_view::npos) {
            break;
        }
        const auto namePos = json.find("\"name\":", idPos);
        const auto valuePos = json.find("\"value\":", idPos);
        const auto kindPos = json.find("\"kind\":", idPos);
        if (namePos == std::string_view::npos) {
            break;
        }
        if (valuePos != std::string_view::npos &&
            (kindPos == std::string_view::npos || valuePos < kindPos)) {
            Parameter p;
            double id = 0;
            extractNumber(json.substr(idPos, 40), "id", id);
            p.id = static_cast<ParameterId>(id);
            extractString(json.substr(namePos, 120), "name", p.name);
            extractNumber(json.substr(valuePos, 40), "value", p.value);
            extractString(json.substr(namePos, 200), "unit", p.unit);
            if (p.unit.empty()) {
                p.unit = "mm";
            }
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
            f.kind = kind == "Extrude" ? FeatureKind::Extrude : FeatureKind::SketchRectangle;
            extractString(json.substr(namePos, 120), "name", f.name);
            const auto depends = json.find("\"dependsOn\":", idPos);
            if (depends != std::string_view::npos) {
                const auto lb = json.find('[', depends);
                const auto rb = json.find(']', lb);
                if (lb != std::string_view::npos && rb != std::string_view::npos) {
                    std::string body(json.substr(lb + 1, rb - lb - 1));
                    std::stringstream ss(body);
                    std::string token;
                    while (std::getline(ss, token, ',')) {
                        if (token.find_first_of("0123456789") == std::string::npos) {
                            continue;
                        }
                        f.dependsOn.push_back(static_cast<FeatureId>(std::stoull(token)));
                    }
                }
            }
            const auto params = json.find("\"parameters\":", idPos);
            if (params != std::string_view::npos) {
                const auto lb = json.find('{', params);
                const auto rb = json.find('}', lb);
                if (lb != std::string_view::npos && rb != std::string_view::npos) {
                    std::string body(json.substr(lb + 1, rb - lb - 1));
                    std::stringstream ss(body);
                    std::string token;
                    while (std::getline(ss, token, ',')) {
                        const auto q1 = token.find('"');
                        const auto q2 = token.find('"', q1 + 1);
                        const auto colon = token.find(':');
                        if (q1 == std::string::npos || q2 == std::string::npos ||
                            colon == std::string::npos) {
                            continue;
                        }
                        const std::string key = token.substr(q1 + 1, q2 - q1 - 1);
                        const double value = std::stod(token.substr(colon + 1));
                        f.parameters[key] = static_cast<ParameterId>(value);
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
    if (!regen.ok && error) {
        *error = regen.message;
    }
    return doc;
}

bool PartDocument::exportStep(const std::string& path, std::string* error) const {
    if (!hasSolid_ || shape_.IsNull()) {
        if (error) {
            *error = "no solid to export";
        }
        return false;
    }
    STEPControl_Writer writer;
    const IFSelect_ReturnStatus status = writer.Transfer(shape_, STEPControl_AsIs);
    if (status != IFSelect_RetDone) {
        if (error) {
            *error = "STEP transfer failed";
        }
        return false;
    }
    if (writer.Write(path.c_str()) != IFSelect_RetDone) {
        if (error) {
            *error = "STEP write failed";
        }
        return false;
    }
    return true;
}

} // namespace mydesk::mechanical
