#include "mydesk/exchange/plotter.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace mydesk::exchange {

namespace {

std::string linetypeToSvgDash(document::Linetype lt, double scale) {
    const double s = scale > 0.0 ? scale : 1.0;
    switch (lt) {
    case document::Linetype::Dashed:
        return std::to_string(6.0 * s) + "," + std::to_string(3.0 * s);
    case document::Linetype::Hidden:
        return std::to_string(3.0 * s) + "," + std::to_string(2.0 * s);
    case document::Linetype::Center:
        return std::to_string(10.0 * s) + "," + std::to_string(2.5 * s) + "," +
               std::to_string(2.5 * s) + "," + std::to_string(2.5 * s);
    case document::Linetype::Phantom:
        return std::to_string(12.0 * s) + "," + std::to_string(2.5 * s) + "," +
               std::to_string(2.5 * s) + "," + std::to_string(2.5 * s) + "," +
               std::to_string(2.5 * s) + "," + std::to_string(2.5 * s);
    case document::Linetype::Dot:
        return std::to_string(1.0 * s) + "," + std::to_string(2.0 * s);
    case document::Linetype::Continuous:
    case document::Linetype::ByLayer:
    default:
        return "";
    }
}

std::string argbToHex(std::uint32_t argb) {
    std::ostringstream ss;
    ss << "#" << std::hex << std::setfill('0') << std::setw(6) << (argb & 0xFFFFFF);
    return ss.str();
}

} // namespace

PlotReport plotToSvg(const document::Document& doc, const std::string& layoutName,
                     const std::filesystem::path& outputPath) {
    PlotReport report;
    const auto* layout = doc.findLayout(layoutName);
    if (!layout) {
        report.ok = false;
        report.message = "Layout not found: " + layoutName;
        return report;
    }

    if (layout->isModelSpace) {
        return plotModelToSvg(doc, outputPath, 1.0);
    }

    const auto& setup = layout->pageSetup;
    report.paperWidthMm = setup.paperWidthMm;
    report.paperHeightMm = setup.paperHeightMm;

    std::ofstream out(outputPath);
    if (!out.is_open()) {
        report.ok = false;
        report.message = "Cannot open output file: " + outputPath.string();
        return report;
    }

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\"\n";
    out << "     width=\"" << setup.paperWidthMm << "mm\" height=\"" << setup.paperHeightMm << "mm\"\n";
    out << "     viewBox=\"0 0 " << setup.paperWidthMm << " " << setup.paperHeightMm << "\">\n";

    // White paper sheet background
    out << "  <!-- Paper Sheet: " << setup.paperName << " -->\n";
    out << "  <rect x=\"0\" y=\"0\" width=\"" << setup.paperWidthMm << "\" height=\""
        << setup.paperHeightMm << "\" fill=\"#ffffff\" stroke=\"#d0d0d0\" stroke-width=\"0.5\"/>\n";

    // Printable margin boundary (dashed light gray)
    const double mX = (setup.paperWidthMm - setup.printableWidthMm) * 0.5;
    const double mY = (setup.paperHeightMm - setup.printableHeightMm) * 0.5;
    out << "  <!-- Printable Area -->\n";
    out << "  <rect x=\"" << mX << "\" y=\"" << mY << "\" width=\"" << setup.printableWidthMm
        << "\" height=\"" << setup.printableHeightMm
        << "\" fill=\"none\" stroke=\"#b0b0b0\" stroke-dasharray=\"3,3\" stroke-width=\"0.25\"/>\n";

    // Render Viewports
    std::size_t vpIndex = 0;
    for (const auto& vp : layout->viewports) {
        if (!vp.on) {
            continue;
        }
        ++report.renderedViewports;
        ++vpIndex;

        const double vpLeft = vp.center.x - vp.width * 0.5;
        const double vpTop = vp.center.y - vp.height * 0.5;
        const std::string clipId = "vp_clip_" + std::to_string(vpIndex);

        out << "  <defs>\n";
        out << "    <clipPath id=\"" << clipId << "\">\n";
        out << "      <rect x=\"" << vpLeft << "\" y=\"" << vpTop << "\" width=\"" << vp.width
            << "\" height=\"" << vp.height << "\"/>\n";
        out << "    </clipPath>\n";
        out << "  </defs>\n";

        // Viewport border
        out << "  <rect x=\"" << vpLeft << "\" y=\"" << vpTop << "\" width=\"" << vp.width
            << "\" height=\"" << vp.height
            << "\" fill=\"#ffffff\" stroke=\"#404040\" stroke-width=\"0.5\"/>\n";

        // Viewport contents group clipped to viewport bounds
        out << "  <g clip-path=\"url(#" << clipId << ")\">\n";

        const double scale = vp.customScale > 0.0 ? vp.customScale : (vp.height / vp.viewHeight);
        auto mapPoint = [&](geometry::Vec2 modelP) -> geometry::Vec2 {
            const double dx = (modelP.x - vp.viewCenter.x) * scale;
            const double dy = (modelP.y - vp.viewCenter.y) * scale;
            // CAD Y goes up (+y is up), SVG Y goes down (+y is down)
            return geometry::Vec2{vp.center.x + dx, vp.center.y - dy};
        };

        // Render model space entities inside viewport
        for (const auto& entity : doc.entities()) {
            const auto* layer = doc.findLayer(entity.layerId);
            if (!layer || !layer->visible || layer->frozen || !layer->plottable) {
                continue;
            }
            if (vp.frozenLayers.count(entity.layerId) > 0) {
                continue;
            }

            const auto strokeColor = (setup.plotStyleTable == "monochrome.ctb")
                                         ? "#000000"
                                         : argbToHex(doc.resolvedEntityColor(entity));
            const double lw = (entity.lineweightByLayer ? layer->lineweightMm : entity.lineweightMm);
            const double strokeWidth = lw > 0.0 ? lw : 0.25;
            const auto effectiveLt = entity.linetype == document::Linetype::ByLayer ? layer->linetype : entity.linetype;
            const std::string dash = linetypeToSvgDash(effectiveLt, doc.linetypeScale());

            std::ostringstream style;
            style << "stroke=\"" << strokeColor << "\" stroke-width=\"" << strokeWidth << "\" fill=\"none\"";
            if (!dash.empty()) {
                style << " stroke-dasharray=\"" << dash << "\"";
            }

            if (const auto* line = std::get_if<document::LineEntity>(&entity.geometry)) {
                const auto p1 = mapPoint(line->start);
                const auto p2 = mapPoint(line->end);
                out << "    <line x1=\"" << p1.x << "\" y1=\"" << p1.y << "\" x2=\"" << p2.x
                    << "\" y2=\"" << p2.y << "\" " << style.str() << "/>\n";
                ++report.renderedEntities;
            } else if (const auto* circle = std::get_if<document::CircleEntity>(&entity.geometry)) {
                const auto c = mapPoint(circle->center);
                const double r = circle->radius * scale;
                out << "    <circle cx=\"" << c.x << "\" cy=\"" << c.y << "\" r=\"" << r
                    << "\" " << style.str() << "/>\n";
                ++report.renderedEntities;
            } else if (const auto* pl = std::get_if<document::PolylineEntity>(&entity.geometry)) {
                if (pl->vertices.size() >= 2) {
                    out << "    <path d=\"";
                    for (std::size_t i = 0; i < pl->vertices.size(); ++i) {
                        const auto p = mapPoint(pl->vertices[i]);
                        out << (i == 0 ? "M " : "L ") << p.x << " " << p.y << " ";
                    }
                    if (pl->closed) {
                        out << "Z";
                    }
                    out << "\" " << style.str() << "/>\n";
                    ++report.renderedEntities;
                }
            } else if (const auto* txt = std::get_if<document::TextEntity>(&entity.geometry)) {
                const auto p = mapPoint(txt->position);
                out << "    <text x=\"" << p.x << "\" y=\"" << p.y << "\" font-size=\""
                    << (txt->height * scale) << "px\" font-family=\"sans-serif\" fill=\""
                    << strokeColor << "\">" << txt->contents << "</text>\n";
                ++report.renderedEntities;
            }
        }

        out << "  </g>\n";
    }

    out << "</svg>\n";
    out.close();

    report.ok = true;
    report.message = "Plot completed successfully to " + outputPath.string();
    return report;
}

PlotReport plotModelToSvg(const document::Document& doc, const std::filesystem::path& outputPath,
                          double scale) {
    PlotReport report;
    geometry::BBox bbox;
    for (const auto& entity : doc.entities()) {
        bbox.expand(entity.bounds());
    }
    if (!bbox.isValid()) {
        bbox.expand(geometry::Vec2{-10.0, -10.0});
        bbox.expand(geometry::Vec2{100.0, 100.0});
    }

    const double margin = 10.0;
    const double width = (bbox.maxX - bbox.minX) * scale + margin * 2.0;
    const double height = (bbox.maxY - bbox.minY) * scale + margin * 2.0;
    report.paperWidthMm = width;
    report.paperHeightMm = height;

    std::ofstream out(outputPath);
    if (!out.is_open()) {
        report.ok = false;
        report.message = "Cannot open output file: " + outputPath.string();
        return report;
    }

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\"\n";
    out << "     width=\"" << width << "mm\" height=\"" << height << "mm\"\n";
    out << "     viewBox=\"0 0 " << width << " " << height << "\">\n";
    out << "  <rect x=\"0\" y=\"0\" width=\"" << width << "\" height=\"" << height
        << "\" fill=\"#ffffff\"/>\n";

    auto mapPoint = [&](geometry::Vec2 p) -> geometry::Vec2 {
        const double dx = (p.x - bbox.minX) * scale + margin;
        const double dy = height - ((p.y - bbox.minY) * scale + margin);
        return geometry::Vec2{dx, dy};
    };

    for (const auto& entity : doc.entities()) {
        const auto* layer = doc.findLayer(entity.layerId);
        if (!layer || !layer->visible || layer->frozen || !layer->plottable) {
            continue;
        }

        const auto strokeColor = argbToHex(doc.resolvedEntityColor(entity));
        const double lw = (entity.lineweightByLayer ? layer->lineweightMm : entity.lineweightMm);
        const double strokeWidth = lw > 0.0 ? lw : 0.25;

        if (const auto* line = std::get_if<document::LineEntity>(&entity.geometry)) {
            const auto p1 = mapPoint(line->start);
            const auto p2 = mapPoint(line->end);
            out << "  <line x1=\"" << p1.x << "\" y1=\"" << p1.y << "\" x2=\"" << p2.x
                << "\" y2=\"" << p2.y << "\" stroke=\"" << strokeColor << "\" stroke-width=\""
                << strokeWidth << "\"/>\n";
            ++report.renderedEntities;
        } else if (const auto* circle = std::get_if<document::CircleEntity>(&entity.geometry)) {
            const auto c = mapPoint(circle->center);
            out << "  <circle cx=\"" << c.x << "\" cy=\"" << c.y << "\" r=\""
                << (circle->radius * scale) << "\" stroke=\"" << strokeColor << "\" stroke-width=\""
                << strokeWidth << "\" fill=\"none\"/>\n";
            ++report.renderedEntities;
        }
    }

    out << "</svg>\n";
    out.close();

    report.ok = true;
    report.message = "Model space plotted to " + outputPath.string();
    return report;
}

} // namespace mydesk::exchange
