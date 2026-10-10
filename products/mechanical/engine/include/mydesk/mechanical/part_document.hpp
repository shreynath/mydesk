#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <TopoDS_Shape.hxx>

namespace mydesk::mechanical {

using FeatureId = std::uint64_t;
using ParameterId = std::uint64_t;

enum class FeatureKind {
    SketchRectangle,
    Extrude,
};

struct Parameter {
    ParameterId id{0};
    std::string name;
    double value{0.0};
    std::string unit{"mm"};
};

struct Feature {
    FeatureId id{0};
    FeatureKind kind{FeatureKind::SketchRectangle};
    std::string name;
    std::vector<FeatureId> dependsOn;
    std::unordered_map<std::string, ParameterId> parameters;
    bool suppressed{false};
};

struct RegenerationReport {
    bool ok{false};
    std::string message;
    std::size_t featuresEvaluated{0};
    double solidVolume{0.0};
    bool isSolid{false};
};

// Inventor-class part: parametric feature graph + OCCT B-rep result.
class PartDocument {
public:
    PartDocument();

    [[nodiscard]] const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    ParameterId addParameter(std::string name, double value, std::string unit = "mm");
    bool setParameter(ParameterId id, double value);
    bool setParameterByName(std::string_view name, double value);
    [[nodiscard]] const Parameter* findParameter(ParameterId id) const;
    [[nodiscard]] const Parameter* findParameterByName(std::string_view name) const;
    [[nodiscard]] const std::vector<Parameter>& parameters() const { return parameters_; }

    // Creates a rectangle sketch driven by width/height parameters, then an extrude of depth.
    FeatureId addRectangleExtrude(std::string sketchName, std::string extrudeName, double width,
                                  double height, double depth);

    [[nodiscard]] const std::vector<Feature>& features() const { return features_; }
    [[nodiscard]] const Feature* findFeature(FeatureId id) const;

    RegenerationReport regenerate();
    [[nodiscard]] const TopoDS_Shape& shape() const { return shape_; }
    [[nodiscard]] double solidVolume() const { return solidVolume_; }
    [[nodiscard]] bool hasSolid() const { return hasSolid_; }

    [[nodiscard]] std::string serializeNative() const;
    static PartDocument deserializeNative(std::string_view json, std::string* error = nullptr);

    bool exportStep(const std::string& path, std::string* error = nullptr) const;

private:
    FeatureId allocateFeature();
    ParameterId allocateParameter();
    RegenerationReport evaluateFeature(const Feature& feature,
                                       std::unordered_map<FeatureId, TopoDS_Shape>& cache);

    std::string name_{"Part1"};
    std::vector<Parameter> parameters_;
    std::vector<Feature> features_;
    FeatureId nextFeatureId_{1};
    ParameterId nextParameterId_{1};
    TopoDS_Shape shape_;
    double solidVolume_{0.0};
    bool hasSolid_{false};
};

} // namespace mydesk::mechanical
