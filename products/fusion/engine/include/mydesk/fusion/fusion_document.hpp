// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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
    std::string name;
    double volumeMm3{0.0};
    double surfaceAreaMm2{0.0};
    double bboxMin[3]{0, 0, 0};
    double bboxMax[3]{0, 0, 0};
};

struct CAMSetup {
    std::string operationName;
    double toolDiameterMm{6.0};
    double spindleRpm{12000.0};
    double feedRateMmMin{1500.0};
    double totalPathLengthMm{0.0};
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
    void setName(std::string name) { name_ = std::move(name); }

    // Timeline management
    FeatureId addSketchRectangle(std::string name, double width, double height);
    FeatureId addExtrude(std::string name, FeatureId sketchId, double depth);
    FeatureId addDirectPushPull(std::string name, FeatureId solidId, double offset);
    FeatureId addCAMPocket(std::string name, double toolDia, double depth, double stepover);
    FeatureId addECADBoard(std::string name, double width, double height, double thickness);

    [[nodiscard]] const std::vector<TimelineEntry>& timeline() const { return timeline_; }
    [[nodiscard]] std::size_t timelineCursor() const { return timelineCursor_; }
    void setTimelineCursor(std::size_t cursor);
    void suppressFeature(FeatureId id, bool suppress);

    // Regeneration
    bool regenerate();
    [[nodiscard]] const std::vector<SolidBody>& bodies() const { return bodies_; }
    [[nodiscard]] const std::vector<CAMSetup>& camSetups() const { return camSetups_; }
    [[nodiscard]] const std::optional<ECADBoard>& ecadBoard() const { return ecadBoard_; }

    // G-code post-processing
    [[nodiscard]] std::string exportGCode() const;

    // Native persistence
    [[nodiscard]] std::string serializeNative() const;
    static FusionDocument deserializeNative(std::string_view json, std::string* error = nullptr);

private:
    std::string name_{"Untitled_Fusion"};
    std::vector<TimelineEntry> timeline_;
    std::size_t timelineCursor_{0};
    FeatureId nextId_{1};

    std::vector<SolidBody> bodies_;
    std::vector<CAMSetup> camSetups_;
    std::optional<ECADBoard> ecadBoard_;
};

} // namespace mydesk::fusion
