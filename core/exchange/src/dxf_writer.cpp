#include "mydesk/exchange/dxf_writer.hpp"

#include <fstream>

namespace mydesk::exchange {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

ConversionReport exportDxfR12(const document::Document& doc, const std::filesystem::path& path) {
    ConversionReport report;
    report.format = "dxf-r12";
    report.formatVersion = "AC1009";

    std::ofstream out(path);
    if (!out) {
        report.errors.push_back("Failed to open DXF path: " + path.string());
        return report;
    }

    auto pair = [&](int code, const std::string& value) { out << code << "\n" << value << "\n"; };
    auto pairNum = [&](int code, double value) { out << code << "\n" << value << "\n"; };

    pair(0, "SECTION");
    pair(2, "HEADER");
    pair(9, "$ACADVER");
    pair(1, "AC1009");
    pair(9, "$LTSCALE");
    pairNum(40, doc.linetypeScale());
    pair(0, "ENDSEC");

    pair(0, "SECTION");
    pair(2, "TABLES");
    pair(0, "TABLE");
    pair(2, "LAYER");
    pair(70, std::to_string(doc.layers().size()));
    for (const auto& layer : doc.layers()) {
        pair(0, "LAYER");
        pair(2, layer.name);
        pair(70, "0");
        // Map RGB luminance to a coarse ACI index (honest partial — not full ACI palette).
        const auto rgb = layer.colorArgb & 0xFFFFFFu;
        const int aci = (rgb == 0xFF0000u)   ? 1
                        : (rgb == 0x00FF00u) ? 3
                        : (rgb == 0x0000FFu) ? 5
                        : (rgb == 0xFFFFFFu) ? 7
                                             : 7;
        pair(62, std::to_string(aci));
        pair(6, std::string(document::linetypeName(layer.linetype)));
        pair(370, std::to_string(static_cast<int>(std::round(layer.lineweightMm * 100.0))));
    }
    pair(0, "ENDTAB");
    pair(0, "ENDSEC");

    pair(0, "SECTION");
    pair(2, "ENTITIES");
    for (const auto& entity : doc.entities()) {
        const document::Layer* layer = doc.findLayer(entity.layerId);
        const std::string layerName = layer ? layer->name : "0";
        const std::string linetype = std::string(document::linetypeName(entity.linetype));
        if (const auto* line = std::get_if<document::LineEntity>(&entity.geometry)) {
            pair(0, "LINE");
            pair(8, layerName);
            if (entity.linetype != document::Linetype::ByLayer) {
                pair(6, linetype);
            }
            if (!entity.lineweightByLayer) {
                pair(370, std::to_string(static_cast<int>(std::round(entity.lineweightMm * 100.0))));
            }
            pairNum(10, line->start.x);
            pairNum(20, line->start.y);
            pairNum(30, 0.0);
            pairNum(11, line->end.x);
            pairNum(21, line->end.y);
            pairNum(31, 0.0);
        } else if (const auto* circle = std::get_if<document::CircleEntity>(&entity.geometry)) {
            pair(0, "CIRCLE");
            pair(8, layerName);
            if (entity.linetype != document::Linetype::ByLayer) {
                pair(6, linetype);
            }
            if (!entity.lineweightByLayer) {
                pair(370, std::to_string(static_cast<int>(std::round(entity.lineweightMm * 100.0))));
            }
            pairNum(10, circle->center.x);
            pairNum(20, circle->center.y);
            pairNum(30, 0.0);
            pairNum(40, circle->radius);
        } else if (const auto* arc = std::get_if<document::ArcEntity>(&entity.geometry)) {
            pair(0, "ARC");
            pair(8, layerName);
            if (entity.linetype != document::Linetype::ByLayer) {
                pair(6, linetype);
            }
            if (!entity.lineweightByLayer) {
                pair(370, std::to_string(static_cast<int>(std::round(entity.lineweightMm * 100.0))));
            }
            pairNum(10, arc->center.x);
            pairNum(20, arc->center.y);
            pairNum(30, 0.0);
            pairNum(40, arc->radius);
            pairNum(50, arc->startAngle * 180.0 / kPi);
            pairNum(51, arc->endAngle * 180.0 / kPi);
        } else if (const auto* pl = std::get_if<document::PolylineEntity>(&entity.geometry)) {
            // Emit as LWPOLYLINE (still widely readable); note format label remains partial.
            pair(0, "LWPOLYLINE");
            pair(8, layerName);
            if (entity.linetype != document::Linetype::ByLayer) {
                pair(6, linetype);
            }
            if (!entity.lineweightByLayer) {
                pair(370, std::to_string(static_cast<int>(std::round(entity.lineweightMm * 100.0))));
            }
            pair(90, std::to_string(pl->vertices.size()));
            pair(70, pl->closed ? "1" : "0");
            for (const auto& v : pl->vertices) {
                pairNum(10, v.x);
                pairNum(20, v.y);
            }
        } else if (std::holds_alternative<document::DimensionLinearEntity>(entity.geometry)) {
            report.warnings.push_back("Skipped DIMLINEAR (not in DXF export subset yet)");
        } else {
            report.warnings.push_back("Skipped unsupported entity during DXF export");
        }
    }
    pair(0, "ENDSEC");
    pair(0, "EOF");

    if (!out) {
        report.errors.push_back("DXF write failed");
        return report;
    }

    report.ok = true;
    report.entityCount = doc.entities().size();
    report.layerCount = doc.layers().size();
    report.warnings.push_back(
        "DXF export is partial: LINE/CIRCLE/ARC/LWPOLYLINE + LAYER; dims/blocks/xrefs unsupported");
    return report;
}

} // namespace mydesk::exchange
