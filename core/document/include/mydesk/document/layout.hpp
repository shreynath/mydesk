#pragma once

#include "mydesk/geometry/vec2.hpp"
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace mydesk::document {

enum class PaperOrientation : std::uint8_t {
    Portrait = 0,
    Landscape = 1,
};

struct PageSetup {
    std::string paperName{"ANSI A (8.5 x 11 in)"};
    double paperWidthMm{279.4};
    double paperHeightMm{215.9};
    double printableWidthMm{260.0};
    double printableHeightMm{200.0};
    double originXMm{0.0};
    double originYMm{0.0};
    double plotScale{1.0}; // 1.0 paper mm = plotScale model units
    std::string plotStyleTable{"monochrome.ctb"};
    PaperOrientation orientation{PaperOrientation::Landscape};
};

// Paper space viewport displaying a region of model space.
struct Viewport {
    std::uint64_t id{0};
    geometry::Vec2 center{130.0, 100.0}; // in paper space mm
    double width{240.0};                 // paper space mm
    double height{180.0};                // paper space mm
    geometry::Vec2 viewCenter{0.0, 0.0}; // model space center
    double viewHeight{100.0};            // model space visible height
    double twistAngleDeg{0.0};
    double customScale{1.0};             // paper units / model units
    bool on{true};
    bool locked{false};
    std::unordered_set<std::uint64_t> frozenLayers; // VP Freeze per layer
};

struct Layout {
    std::string name{"Layout1"};
    bool isModelSpace{false};
    PageSetup pageSetup;
    std::vector<std::uint64_t> entityIds; // Entities in paper space (title blocks, annotations)
    std::vector<Viewport> viewports;      // Viewports in this layout
};

} // namespace mydesk::document
