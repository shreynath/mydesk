#pragma once

#include "mydesk/document/document.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace mydesk::exchange {

struct PlotReport {
    bool ok{false};
    std::string message;
    std::size_t renderedEntities{0};
    std::size_t renderedViewports{0};
    double paperWidthMm{0.0};
    double paperHeightMm{0.0};
    std::vector<std::string> warnings;
};

// Plots a layout (paper space with viewports, or Model space) to vector SVG.
[[nodiscard]] PlotReport plotToSvg(const document::Document& doc, const std::string& layoutName,
                                   const std::filesystem::path& outputPath);

// Plots Model space directly to vector SVG with specified scale.
[[nodiscard]] PlotReport plotModelToSvg(const document::Document& doc,
                                        const std::filesystem::path& outputPath,
                                        double scale = 1.0);

} // namespace mydesk::exchange
