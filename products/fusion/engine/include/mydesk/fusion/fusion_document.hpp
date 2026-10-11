// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <TopoDS_Shape.hxx>

namespace mydesk::fusion {

using FeatureId = std::uint64_t;

enum class TimelineKind {
    Sketch,
    Extrude,
    Revolve,
    Chamfer,
    DirectPushPull,
    CAMOperation,
    ECADBoard
};

struct TimelineEntry {
    FeatureId id{0};
    TimelineKind kind{TimelineKind::Sketch};
    std::string name;
    bool suppressed{false};
    std::unordered_map<std::string, double> parameters;
};

struct SolidBody {
    FeatureId featureId{0};
    std::string name;
    TopoDS_Shape shape;
    bool isValid{false};
    std::size_t solidCount{0};
    std::size_t shellCount{0};
    std::size_t faceCount{0};
    std::size_t edgeCount{0};
    std::size_t vertexCount{0};
    double volumeMm3{0.0};
    double surfaceAreaMm2{0.0};
    double bboxMin[3]{0, 0, 0};
    double bboxMax[3]{0, 0, 0};
};

struct ToolpathPoint {
    double xMm{0.0};
    double yMm{0.0};
    double zMm{0.0};
    bool rapid{false};
};

struct CAMSetup {
    std::string operationName;
    FeatureId targetBodyFeatureId{0};
    double toolDiameterMm{6.0};
    double pocketDepthMm{0.0};
    double stepoverMm{0.0};
    double stepdownMm{0.0};
    double spindleRpm{12000.0};
    double feedRateMmMin{1500.0};
    double totalPathLengthMm{0.0};
    double boundaryMin[2]{0.0, 0.0};
    double boundaryMax[2]{0.0, 0.0};
    std::vector<ToolpathPoint> toolpath;
    std::vector<std::string> gcodeLines;
};

struct ECADComponent {
    std::string refDes;
    std::string package;
    double xMm{0.0};
    double yMm{0.0};
    double heightMm{0.0};
};

struct ECADBoard {
    double widthMm{100.0};
    double heightMm{80.0};
    double thicknessMm{1.6};
    std::vector<ECADComponent> components;
};

class FusionDocument {
public:
    FusionDocument();

    [[nodiscard]] const std::string& name() const { return name_; }
    void setName(std::string name);

    // Timeline management
    FeatureId addSketchRectangle(std::string name, double width, double height);
    FeatureId addExtrude(std::string name, FeatureId sketchId, double depth);
    FeatureId addDirectPushPull(std::string name, FeatureId solidId, double offset);
    FeatureId addCAMPocket(std::string name, FeatureId solidId, double toolDia,
                           double depth, double stepover, double stepdown);
    FeatureId addECADBoard(std::string name, double width, double height, double thickness);

    [[nodiscard]] const std::vector<TimelineEntry>& timeline() const { return timeline_; }
    [[nodiscard]] std::size_t timelineCursor() const { return timelineCursor_; }
    void setTimelineCursor(std::size_t cursor);
    void suppressFeature(FeatureId id, bool suppress);
    bool renameFeature(FeatureId id, std::string name);
    bool setFeatureParameter(FeatureId id, std::string_view parameter, double value);

    [[nodiscard]] bool canUndo() const { return !undo_.empty(); }
    [[nodiscard]] bool canRedo() const { return !redo_.empty(); }
    bool undo();
    bool redo();
    void clearHistory();

    // Regeneration
    bool regenerate();
    [[nodiscard]] const std::string& lastError() const { return lastError_; }
    [[nodiscard]] const std::vector<SolidBody>& bodies() const { return bodies_; }
    [[nodiscard]] const std::vector<CAMSetup>& camSetups() const { return camSetups_; }
    [[nodiscard]] const std::optional<ECADBoard>& ecadBoard() const { return ecadBoard_; }

    // G-code post-processing
    [[nodiscard]] std::string exportGCode() const;

    // Native persistence
    [[nodiscard]] std::string serializeNative() const;
    static FusionDocument deserializeNative(std::string_view json, std::string* error = nullptr);

private:
    struct Snapshot {
        std::string name;
        std::vector<TimelineEntry> timeline;
        std::size_t timelineCursor{0};
        FeatureId nextId{1};
    };

    [[nodiscard]] Snapshot snapshot() const;
    void restore(const Snapshot& state);
    void checkpoint();

    std::string name_{"Untitled_Fusion"};
    std::vector<TimelineEntry> timeline_;
    std::size_t timelineCursor_{0};
    FeatureId nextId_{1};

    std::vector<SolidBody> bodies_;
    std::vector<CAMSetup> camSetups_;
    std::optional<ECADBoard> ecadBoard_;
    std::string lastError_;
    std::vector<Snapshot> undo_;
    std::vector<Snapshot> redo_;
};

} // namespace mydesk::fusion
