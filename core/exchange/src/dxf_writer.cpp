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
        pair(62, "7");
        pair(6, "CONTINUOUS");
    }
    pair(0, "ENDTAB");
    pair(0, "ENDSEC");

    pair(0, "SECTION");
    pair(2, "ENTITIES");
    for (const auto& entity : doc.entities()) {
        const document::Layer* layer = doc.findLayer(entity.layerId);
        const std::string layerName = layer ? layer->name : "0";
        if (const auto* line = std::get_if<document::LineEntity>(&entity.geometry)) {
            pair(0, "LINE");
            pair(8, layerName);
            pairNum(10, line->start.x);
            pairNum(20, line->start.y);
            pairNum(30, 0.0);
            pairNum(11, line->end.x);
            pairNum(21, line->end.y);
            pairNum(31, 0.0);
        } else if (const auto* circle = std::get_if<document::CircleEntity>(&entity.geometry)) {
            pair(0, "CIRCLE");
            pair(8, layerName);
            pairNum(10, circle->center.x);
            pairNum(20, circle->center.y);
            pairNum(30, 0.0);
            pairNum(40, circle->radius);
        } else if (const auto* arc = std::get_if<document::ArcEntity>(&entity.geometry)) {
            pair(0, "ARC");
            pair(8, layerName);
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
            pair(90, std::to_string(pl->vertices.size()));
            pair(70, pl->closed ? "1" : "0");
            for (const auto& v : pl->vertices) {
                pairNum(10, v.x);
                pairNum(20, v.y);
            }
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
        "DXF export is partial: LINE/CIRCLE/ARC/LWPOLYLINE + LAYER; no dims/blocks/xrefs/styles");
    return report;
}

} // namespace mydesk::exchange
