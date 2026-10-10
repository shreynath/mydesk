#include "mydesk/exchange/native_format.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

namespace mydesk::exchange {

namespace {

constexpr const char* kSchemaVersion = "6";

std::string escapeJson(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(c);
            break;
        }
    }
    return out;
}

std::string unitName(units::LengthUnit unit) {
    switch (unit) {
    case units::LengthUnit::Millimeter:
        return "mm";
    case units::LengthUnit::Centimeter:
        return "cm";
    case units::LengthUnit::Meter:
        return "m";
    case units::LengthUnit::Inch:
        return "in";
    case units::LengthUnit::Foot:
        return "ft";
    }
    return "mm";
}

units::LengthUnit unitFromName(std::string_view name) {
    if (name == "cm") {
        return units::LengthUnit::Centimeter;
    }
    if (name == "m") {
        return units::LengthUnit::Meter;
    }
    if (name == "in") {
        return units::LengthUnit::Inch;
    }
    if (name == "ft") {
        return units::LengthUnit::Foot;
    }
    return units::LengthUnit::Millimeter;
}

// Extremely small JSON helpers for our controlled schema (not a general parser).
bool extractStringField(std::string_view json, std::string_view key, std::string& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return false;
    }
    const auto colon = json.find(':', pos + needle.size());
    if (colon == std::string_view::npos) {
        return false;
    }
    const auto q1 = json.find('"', colon + 1);
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

bool extractNumberAfterKey(std::string_view json, std::string_view key, double& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return false;
    }
    const auto colon = json.find(':', pos + needle.size());
    if (colon == std::string_view::npos) {
        return false;
    }
    std::size_t i = colon + 1;
    while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) {
        ++i;
    }
    std::size_t end = i;
    while (end < json.size() &&
           (std::isdigit(static_cast<unsigned char>(json[end])) || json[end] == '-' ||
            json[end] == '+' || json[end] == '.' || json[end] == 'e' || json[end] == 'E')) {
        ++end;
    }
    try {
        out = std::stod(std::string(json.substr(i, end - i)));
        return true;
    } catch (...) {
        return false;
    }
}

bool extractUintAfterKey(std::string_view json, std::string_view key, std::uint64_t& out) {
    double d = 0.0;
    if (!extractNumberAfterKey(json, key, d)) {
        return false;
    }
    if (d < 0.0) {
        return false;
    }
    out = static_cast<std::uint64_t>(d);
    return true;
}

bool extractBoolAfterKey(std::string_view json, std::string_view key, bool& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return false;
    }
    const auto colon = json.find(':', pos + needle.size());
    if (colon == std::string_view::npos) {
        return false;
    }
    std::size_t i = colon + 1;
    while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) {
        ++i;
    }
    if (json.substr(i, 4) == "true") {
        out = true;
        return true;
    }
    if (json.substr(i, 5) == "false") {
        out = false;
        return true;
    }
    return false;
}

std::vector<std::string_view> splitTopLevelObjects(std::string_view arrayBody) {
    std::vector<std::string_view> objects;
    int depth = 0;
    std::size_t start = std::string_view::npos;
    for (std::size_t i = 0; i < arrayBody.size(); ++i) {
        const char c = arrayBody[i];
        if (c == '{') {
            if (depth == 0) {
                start = i;
            }
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0 && start != std::string_view::npos) {
                objects.push_back(arrayBody.substr(start, i - start + 1));
                start = std::string_view::npos;
            }
        }
    }
    return objects;
}

std::string_view extractArrayBody(std::string_view json, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return {};
    }
    const auto lb = json.find('[', pos + needle.size());
    if (lb == std::string_view::npos) {
        return {};
    }
    int depth = 0;
    for (std::size_t i = lb; i < json.size(); ++i) {
        if (json[i] == '[') {
            ++depth;
        } else if (json[i] == ']') {
            --depth;
            if (depth == 0) {
                return json.substr(lb + 1, i - lb - 1);
            }
        }
    }
    return {};
}

} // namespace

std::string serializeNative(const document::Document& doc) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"format\": \"mydesk-native\",\n";
    oss << "  \"schemaVersion\": \"" << kSchemaVersion << "\",\n";
    oss << "  \"name\": \"" << escapeJson(doc.name()) << "\",\n";
    oss << "  \"units\": {\n";
    oss << "    \"length\": \"" << unitName(doc.units().length) << "\",\n";
    oss << "    \"linearPrecision\": " << doc.units().linearPrecision << ",\n";
    oss << "    \"modelSpaceTolerance\": " << doc.units().modelSpaceTolerance << "\n";
    oss << "  },\n";
    // Attribute display mode
    {
        const char* modeStr = "normal";
        switch (doc.attributeDisplayMode()) {
        case document::AttributeDisplayMode::On:  modeStr = "on";  break;
        case document::AttributeDisplayMode::Off: modeStr = "off"; break;
        default: break;
        }
        oss << "  \"attdisp\": \"" << modeStr << "\",\n";
    }
    oss << "  \"activeLayerId\": " << doc.activeLayerId() << ",\n";
    oss << "  \"nextEntityId\": " << doc.nextEntityIdPeek() << ",\n";
    oss << "  \"nextLayerId\": " << doc.nextLayerIdPeek() << ",\n";
    oss << "  \"linetypeScale\": " << doc.linetypeScale() << ",\n";
    oss << "  \"layers\": [\n";
    for (std::size_t i = 0; i < doc.layers().size(); ++i) {
        const auto& layer = doc.layers()[i];
        oss << "    {\"id\": " << layer.id << ", \"name\": \"" << escapeJson(layer.name)
            << "\", \"visible\": " << (layer.visible ? "true" : "false")
            << ", \"locked\": " << (layer.locked ? "true" : "false")
            << ", \"frozen\": " << (layer.frozen ? "true" : "false")
            << ", \"plottable\": " << (layer.plottable ? "true" : "false")
            << ", \"linetype\": \"" << document::linetypeName(layer.linetype)
            << "\", \"lineweightMm\": " << layer.lineweightMm << ", \"colorArgb\": "
            << layer.colorArgb << "}";
        if (i + 1 < doc.layers().size()) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << "  ],\n";
    oss << "  \"entities\": [\n";
    for (std::size_t i = 0; i < doc.entities().size(); ++i) {
        const auto& entity = doc.entities()[i];
        oss << "    {\"id\": " << entity.id << ", \"layerId\": " << entity.layerId
            << ", \"colorArgb\": " << entity.colorArgb << ", \"linetype\": \""
            << document::linetypeName(entity.linetype) << "\", \"lineweightByLayer\": "
            << (entity.lineweightByLayer ? "true" : "false") << ", \"lineweightMm\": "
            << entity.lineweightMm << ", \"kind\": \""
            << document::entityKindName(entity.kind()) << "\", ";
        if (const auto* line = std::get_if<document::LineEntity>(&entity.geometry)) {
            oss << "\"start\": {\"x\": " << line->start.x << ", \"y\": " << line->start.y
                << "}, \"end\": {\"x\": " << line->end.x << ", \"y\": " << line->end.y << "}";
        } else if (const auto* circle = std::get_if<document::CircleEntity>(&entity.geometry)) {
            oss << "\"center\": {\"x\": " << circle->center.x << ", \"y\": " << circle->center.y
                << "}, \"radius\": " << circle->radius;
        } else if (const auto* arc = std::get_if<document::ArcEntity>(&entity.geometry)) {
            oss << "\"center\": {\"x\": " << arc->center.x << ", \"y\": " << arc->center.y
                << "}, \"radius\": " << arc->radius << ", \"startAngle\": " << arc->startAngle
                << ", \"endAngle\": " << arc->endAngle;
        } else if (const auto* pl = std::get_if<document::PolylineEntity>(&entity.geometry)) {
            oss << "\"closed\": " << (pl->closed ? "true" : "false") << ", \"vertices\": [";
            for (std::size_t vi = 0; vi < pl->vertices.size(); ++vi) {
                oss << "{\"x\": " << pl->vertices[vi].x << ", \"y\": " << pl->vertices[vi].y << "}";
                if (vi + 1 < pl->vertices.size()) {
                    oss << ", ";
                }
            }
            oss << "]";
        } else if (const auto* dim = std::get_if<document::DimensionLinearEntity>(&entity.geometry)) {
            oss << "\"start\": {\"x\": " << dim->start.x << ", \"y\": " << dim->start.y
                << "}, \"end\": {\"x\": " << dim->end.x << ", \"y\": " << dim->end.y
                << "}, \"dimLine\": {\"x\": " << dim->dimLine.x << ", \"y\": " << dim->dimLine.y
                << "}";
        } else if (const auto* insert = std::get_if<document::InsertEntity>(&entity.geometry)) {
            oss << "\"blockName\": \"" << escapeJson(insert->blockName) << "\", \"insertion\": {\"x\": "
                << insert->insertion.x << ", \"y\": " << insert->insertion.y
                << "}, \"scale\": " << insert->scale << ", \"rotationDeg\": " << insert->rotationDeg
                << ", \"localBounds\": {\"minX\": " << insert->localBounds.minX << ", \"minY\": "
                << insert->localBounds.minY << ", \"maxX\": " << insert->localBounds.maxX
                << ", \"maxY\": " << insert->localBounds.maxY << "}";
            if (!insert->attributes.empty()) {
                oss << ", \"attributes\": [";
                for (std::size_t ai = 0; ai < insert->attributes.size(); ++ai) {
                    const auto& att = insert->attributes[ai];
                    oss << "{\"tag\": \"" << escapeJson(att.tag) << "\", \"value\": \"" << escapeJson(att.value)
                        << "\", \"position\": {\"x\": " << att.position.x << ", \"y\": " << att.position.y
                        << "}, \"height\": " << att.height << ", \"rotationDeg\": " << att.rotationDeg
                        << ", \"invisible\": " << (att.invisible ? "true" : "false") << "}";
                    if (ai + 1 < insert->attributes.size()) oss << ", ";
                }
                oss << "]";
            }
        } else if (const auto* attdef = std::get_if<document::AttdefEntity>(&entity.geometry)) {
            oss << "\"tag\": \"" << escapeJson(attdef->tag) << "\", \"prompt\": \"" << escapeJson(attdef->prompt)
                << "\", \"defaultValue\": \"" << escapeJson(attdef->defaultValue)
                << "\", \"position\": {\"x\": " << attdef->position.x << ", \"y\": " << attdef->position.y
                << "}, \"height\": " << attdef->height << ", \"rotationDeg\": " << attdef->rotationDeg
                << ", \"invisible\": " << (attdef->invisible ? "true" : "false")
                << ", \"flags\": " << attdef->flags
                << ", \"isMultiline\": " << (attdef->isMultiline ? "true" : "false");
        }
        oss << "}";
        if (i + 1 < doc.entities().size()) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << "  ],\n";
    oss << "  \"blocks\": [\n";
    for (std::size_t bi = 0; bi < doc.blocks().size(); ++bi) {
        const auto& block = doc.blocks()[bi];
        oss << "    {\"name\": \"" << escapeJson(block.name) << "\", \"primitives\": [\n";
        for (std::size_t pi = 0; pi < block.primitives.size(); ++pi) {
            const auto& prim = block.primitives[pi];
            document::Entity tmp;
            tmp.geometry = prim.geometry;
            oss << "      {\"colorArgb\": " << prim.colorArgb << ", \"kind\": \""
                << document::entityKindName(tmp.kind()) << "\", ";
            if (const auto* line = std::get_if<document::LineEntity>(&prim.geometry)) {
                oss << "\"start\": {\"x\": " << line->start.x << ", \"y\": " << line->start.y
                    << "}, \"end\": {\"x\": " << line->end.x << ", \"y\": " << line->end.y << "}";
            } else if (const auto* circle = std::get_if<document::CircleEntity>(&prim.geometry)) {
                oss << "\"center\": {\"x\": " << circle->center.x << ", \"y\": " << circle->center.y
                    << "}, \"radius\": " << circle->radius;
            } else if (const auto* pl = std::get_if<document::PolylineEntity>(&prim.geometry)) {
                oss << "\"closed\": " << (pl->closed ? "true" : "false") << ", \"vertices\": [";
                for (std::size_t vi = 0; vi < pl->vertices.size(); ++vi) {
                    oss << "{\"x\": " << pl->vertices[vi].x << ", \"y\": " << pl->vertices[vi].y
                        << "}";
                    if (vi + 1 < pl->vertices.size()) {
                        oss << ", ";
                    }
                }
                oss << "]";
            } else if (const auto* nestedIns = std::get_if<document::InsertEntity>(&prim.geometry)) {
                oss << "\"blockName\": \"" << escapeJson(nestedIns->blockName)
                    << "\", \"insertion\": {\"x\": " << nestedIns->insertion.x << ", \"y\": " << nestedIns->insertion.y
                    << "}, \"scale\": " << nestedIns->scale << ", \"rotationDeg\": " << nestedIns->rotationDeg;
            } else {
                oss << "\"unsupported\": true";
            }
            oss << "}";
            if (pi + 1 < block.primitives.size()) {
                oss << ",";
            }
            oss << "\n";
        }
        oss << "    ]";
        // Serialize block attribute definitions
        if (!block.attributes.empty()) {
            oss << ", \"attributes\": [\n";
            for (std::size_t ai = 0; ai < block.attributes.size(); ++ai) {
                const auto& ad = block.attributes[ai];
                oss << "      {\"tag\": \"" << escapeJson(ad.tag) << "\", \"prompt\": \"" << escapeJson(ad.prompt)
                    << "\", \"defaultValue\": \"" << escapeJson(ad.defaultValue)
                    << "\", \"position\": {\"x\": " << ad.position.x << ", \"y\": " << ad.position.y
                    << "}, \"height\": " << ad.height << ", \"rotationDeg\": " << ad.rotationDeg
                    << ", \"invisible\": " << (ad.invisible ? "true" : "false")
                    << ", \"flags\": " << ad.flags
                    << ", \"isMultiline\": " << (ad.isMultiline ? "true" : "false") << "}";
                if (ai + 1 < block.attributes.size()) oss << ",";
                oss << "\n";
            }
            oss << "    ]";
        }
        oss << "}";
        if (bi + 1 < doc.blocks().size()) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
}

ConversionReport deserializeNative(document::Document& doc, std::string_view json) {
    ConversionReport report;
    report.formatVersion = kSchemaVersion;

    std::string format;
    if (!extractStringField(json, "format", format) || format != "mydesk-native") {
        report.errors.push_back("Not a mydesk-native document");
        return report;
    }
    std::string schema;
    if (!extractStringField(json, "schemaVersion", schema)) {
        report.errors.push_back("Missing schemaVersion");
        return report;
    }
    if (schema != "3" && schema != "4" && schema != "5" && schema != "6") {
        report.warnings.push_back("schemaVersion " + schema + " differs from supported " +
                                  kSchemaVersion);
    }

    std::string name = "Drawing1";
    extractStringField(json, "name", name);

    units::UnitSystem unitSystem;
    std::string lengthName = "mm";
    extractStringField(json, "length", lengthName);
    unitSystem.length = unitFromName(lengthName);
    double precision = 4;
    if (extractNumberAfterKey(json, "linearPrecision", precision)) {
        unitSystem.linearPrecision = static_cast<int>(precision);
    }
    extractNumberAfterKey(json, "modelSpaceTolerance", unitSystem.modelSpaceTolerance);

    std::uint64_t activeLayer = 1;
    std::uint64_t nextEntity = 1;
    std::uint64_t nextLayer = 1;
    extractUintAfterKey(json, "activeLayerId", activeLayer);
    extractUintAfterKey(json, "nextEntityId", nextEntity);
    extractUintAfterKey(json, "nextLayerId", nextLayer);
    double linetypeScale = 1.0;
    extractNumberAfterKey(json, "linetypeScale", linetypeScale);

    // Attribute display mode
    document::AttributeDisplayMode attdisp = document::AttributeDisplayMode::Normal;
    std::string attdispStr;
    if (extractStringField(json, "attdisp", attdispStr)) {
        if (attdispStr == "on") attdisp = document::AttributeDisplayMode::On;
        else if (attdispStr == "off") attdisp = document::AttributeDisplayMode::Off;
    }

    std::vector<document::Layer> layers;
    for (const auto obj : splitTopLevelObjects(extractArrayBody(json, "layers"))) {
        document::Layer layer;
        std::uint64_t id = 0;
        extractUintAfterKey(obj, "id", id);
        layer.id = id;
        extractStringField(obj, "name", layer.name);
        extractBoolAfterKey(obj, "visible", layer.visible);
        extractBoolAfterKey(obj, "locked", layer.locked);
        extractBoolAfterKey(obj, "frozen", layer.frozen);
        extractBoolAfterKey(obj, "plottable", layer.plottable);
        std::string linetypeName;
        if (extractStringField(obj, "linetype", linetypeName)) {
            document::Linetype parsed = document::Linetype::Continuous;
            if (document::parseLinetype(linetypeName, parsed) &&
                parsed != document::Linetype::ByLayer) {
                layer.linetype = parsed;
            }
        }
        double weight = layer.lineweightMm;
        if (extractNumberAfterKey(obj, "lineweightMm", weight) &&
            document::validLineweightMm(weight)) {
            layer.lineweightMm = weight;
        }
        double color = static_cast<double>(layer.colorArgb);
        if (extractNumberAfterKey(obj, "colorArgb", color)) {
            layer.colorArgb = static_cast<std::uint32_t>(color);
        }
        layers.push_back(layer);
    }
    if (layers.empty()) {
        report.errors.push_back("Document has no layers");
        return report;
    }

    std::vector<document::Entity> entities;
    for (const auto obj : splitTopLevelObjects(extractArrayBody(json, "entities"))) {
        document::Entity entity;
        std::uint64_t id = 0;
        std::uint64_t layerId = 0;
        extractUintAfterKey(obj, "id", id);
        extractUintAfterKey(obj, "layerId", layerId);
        entity.id = id;
        entity.layerId = layerId;
        double color = 0;
        if (extractNumberAfterKey(obj, "colorArgb", color)) {
            entity.colorArgb = static_cast<std::uint32_t>(color);
        }
        std::string linetypeName;
        if (extractStringField(obj, "linetype", linetypeName)) {
            document::Linetype parsed = document::Linetype::ByLayer;
            if (document::parseLinetype(linetypeName, parsed)) {
                entity.linetype = parsed;
            }
        }
        bool byLayerWeight = true;
        if (extractBoolAfterKey(obj, "lineweightByLayer", byLayerWeight)) {
            entity.lineweightByLayer = byLayerWeight;
        }
        double weight = entity.lineweightMm;
        if (extractNumberAfterKey(obj, "lineweightMm", weight) &&
            document::validLineweightMm(weight)) {
            entity.lineweightMm = weight;
        }
        std::string kind;
        extractStringField(obj, "kind", kind);
        if (kind == "LINE") {
            document::LineEntity line;
            // Nested objects: search inside object text for start/end x/y in order.
            // Pattern: "start": {"x": N, "y": N}, "end": {"x": N, "y": N}
            const auto startPos = obj.find("\"start\"");
            const auto endPos = obj.find("\"end\"");
            if (startPos == std::string_view::npos || endPos == std::string_view::npos) {
                report.warnings.push_back("Skipping LINE without start/end");
                continue;
            }
            const auto startSlice = obj.substr(startPos, endPos - startPos);
            const auto endSlice = obj.substr(endPos);
            extractNumberAfterKey(startSlice, "x", line.start.x);
            extractNumberAfterKey(startSlice, "y", line.start.y);
            extractNumberAfterKey(endSlice, "x", line.end.x);
            extractNumberAfterKey(endSlice, "y", line.end.y);
            entity.geometry = line;
        } else if (kind == "CIRCLE") {
            document::CircleEntity circle;
            const auto centerPos = obj.find("\"center\"");
            if (centerPos == std::string_view::npos) {
                report.warnings.push_back("Skipping CIRCLE without center");
                continue;
            }
            const auto centerSlice = obj.substr(centerPos);
            extractNumberAfterKey(centerSlice, "x", circle.center.x);
            extractNumberAfterKey(centerSlice, "y", circle.center.y);
            extractNumberAfterKey(obj, "radius", circle.radius);
            if (!(circle.radius > 0.0)) {
                report.warnings.push_back("Skipping CIRCLE with non-positive radius");
                continue;
            }
            entity.geometry = circle;
        } else if (kind == "ARC") {
            document::ArcEntity arc;
            const auto centerPos = obj.find("\"center\"");
            if (centerPos == std::string_view::npos) {
                report.warnings.push_back("Skipping ARC without center");
                continue;
            }
            const auto centerSlice = obj.substr(centerPos);
            extractNumberAfterKey(centerSlice, "x", arc.center.x);
            extractNumberAfterKey(centerSlice, "y", arc.center.y);
            extractNumberAfterKey(obj, "radius", arc.radius);
            extractNumberAfterKey(obj, "startAngle", arc.startAngle);
            extractNumberAfterKey(obj, "endAngle", arc.endAngle);
            if (!(arc.radius > 0.0)) {
                report.warnings.push_back("Skipping ARC with non-positive radius");
                continue;
            }
            entity.geometry = arc;
        } else if (kind == "PLINE") {
            document::PolylineEntity pl;
            pl.closed = obj.find("\"closed\": true") != std::string_view::npos;
            for (const auto vobj : splitTopLevelObjects(extractArrayBody(obj, "vertices"))) {
                geometry::Vec2 v;
                extractNumberAfterKey(vobj, "x", v.x);
                extractNumberAfterKey(vobj, "y", v.y);
                pl.vertices.push_back(v);
            }
            if (pl.vertices.size() < 2) {
                report.warnings.push_back("Skipping PLINE with <2 vertices");
                continue;
            }
            entity.geometry = pl;
        } else if (kind == "DIMLINEAR") {
            document::DimensionLinearEntity dim;
            const auto startPos = obj.find("\"start\"");
            const auto endPos = obj.find("\"end\"");
            const auto dimPos = obj.find("\"dimLine\"");
            if (startPos == std::string_view::npos || endPos == std::string_view::npos ||
                dimPos == std::string_view::npos) {
                report.warnings.push_back("Skipping DIMLINEAR without points");
                continue;
            }
            const auto startSlice = obj.substr(startPos, endPos - startPos);
            const auto endSlice = obj.substr(endPos, dimPos - endPos);
            const auto dimSlice = obj.substr(dimPos);
            extractNumberAfterKey(startSlice, "x", dim.start.x);
            extractNumberAfterKey(startSlice, "y", dim.start.y);
            extractNumberAfterKey(endSlice, "x", dim.end.x);
            extractNumberAfterKey(endSlice, "y", dim.end.y);
            extractNumberAfterKey(dimSlice, "x", dim.dimLine.x);
            extractNumberAfterKey(dimSlice, "y", dim.dimLine.y);
            entity.geometry = dim;
        } else if (kind == "INSERT") {
            document::InsertEntity insert;
            extractStringField(obj, "blockName", insert.blockName);
            const auto insPos = obj.find("\"insertion\"");
            if (insPos != std::string_view::npos) {
                const auto slice = obj.substr(insPos);
                extractNumberAfterKey(slice, "x", insert.insertion.x);
                extractNumberAfterKey(slice, "y", insert.insertion.y);
            }
            extractNumberAfterKey(obj, "scale", insert.scale);
            extractNumberAfterKey(obj, "rotationDeg", insert.rotationDeg);
            const auto lb = obj.find("\"localBounds\"");
            if (lb != std::string_view::npos) {
                const auto slice = obj.substr(lb);
                extractNumberAfterKey(slice, "minX", insert.localBounds.minX);
                extractNumberAfterKey(slice, "minY", insert.localBounds.minY);
                extractNumberAfterKey(slice, "maxX", insert.localBounds.maxX);
                extractNumberAfterKey(slice, "maxY", insert.localBounds.maxY);
            }
            // Deserialize insert attributes
            for (const auto aobj : splitTopLevelObjects(extractArrayBody(obj, "attributes"))) {
                document::AttributeInstance ai;
                extractStringField(aobj, "tag", ai.tag);
                extractStringField(aobj, "value", ai.value);
                const auto posPos = aobj.find("\"position\"");
                if (posPos != std::string_view::npos) {
                    const auto pslice = aobj.substr(posPos);
                    extractNumberAfterKey(pslice, "x", ai.position.x);
                    extractNumberAfterKey(pslice, "y", ai.position.y);
                }
                extractNumberAfterKey(aobj, "height", ai.height);
                extractNumberAfterKey(aobj, "rotationDeg", ai.rotationDeg);
                extractBoolAfterKey(aobj, "invisible", ai.invisible);
                double fNum = 0.0;
                if (extractNumberAfterKey(aobj, "flags", fNum)) {
                    ai.flags = static_cast<std::uint32_t>(fNum);
                }
                extractBoolAfterKey(aobj, "isMultiline", ai.isMultiline);
                insert.attributes.push_back(std::move(ai));
            }
            entity.geometry = insert;
        } else if (kind == "ATTDEF") {
            document::AttdefEntity attdef;
            extractStringField(obj, "tag", attdef.tag);
            extractStringField(obj, "prompt", attdef.prompt);
            extractStringField(obj, "defaultValue", attdef.defaultValue);
            const auto posPos = obj.find("\"position\"");
            if (posPos != std::string_view::npos) {
                const auto pslice = obj.substr(posPos);
                extractNumberAfterKey(pslice, "x", attdef.position.x);
                extractNumberAfterKey(pslice, "y", attdef.position.y);
            }
            extractNumberAfterKey(obj, "height", attdef.height);
            extractNumberAfterKey(obj, "rotationDeg", attdef.rotationDeg);
            extractBoolAfterKey(obj, "invisible", attdef.invisible);
            double fNum = 0.0;
            if (extractNumberAfterKey(obj, "flags", fNum)) {
                attdef.flags = static_cast<std::uint32_t>(fNum);
            }
            extractBoolAfterKey(obj, "isMultiline", attdef.isMultiline);
            entity.geometry = attdef;
        } else {
            report.warnings.push_back("Unsupported entity kind: " + kind);
            continue;
        }
        entities.push_back(entity);
    }

    std::vector<document::BlockDefinition> blocks;
    for (const auto bobj : splitTopLevelObjects(extractArrayBody(json, "blocks"))) {
        document::BlockDefinition block;
        extractStringField(bobj, "name", block.name);
        for (const auto pobj : splitTopLevelObjects(extractArrayBody(bobj, "primitives"))) {
            document::BlockPrimitive prim;
            double color = 0;
            if (extractNumberAfterKey(pobj, "colorArgb", color)) {
                prim.colorArgb = static_cast<std::uint32_t>(color);
            }
            std::string pkind;
            extractStringField(pobj, "kind", pkind);
            if (pkind == "LINE") {
                document::LineEntity line;
                const auto startPos = pobj.find("\"start\"");
                const auto endPos = pobj.find("\"end\"");
                if (startPos == std::string_view::npos || endPos == std::string_view::npos) {
                    continue;
                }
                const auto startSlice = pobj.substr(startPos, endPos - startPos);
                const auto endSlice = pobj.substr(endPos);
                extractNumberAfterKey(startSlice, "x", line.start.x);
                extractNumberAfterKey(startSlice, "y", line.start.y);
                extractNumberAfterKey(endSlice, "x", line.end.x);
                extractNumberAfterKey(endSlice, "y", line.end.y);
                prim.geometry = line;
            } else if (pkind == "CIRCLE") {
                document::CircleEntity circle;
                const auto centerPos = pobj.find("\"center\"");
                if (centerPos == std::string_view::npos) {
                    continue;
                }
                const auto centerSlice = pobj.substr(centerPos);
                extractNumberAfterKey(centerSlice, "x", circle.center.x);
                extractNumberAfterKey(centerSlice, "y", circle.center.y);
                extractNumberAfterKey(pobj, "radius", circle.radius);
                prim.geometry = circle;
            } else if (pkind == "PLINE") {
                document::PolylineEntity pl;
                pl.closed = pobj.find("\"closed\": true") != std::string_view::npos;
                for (const auto vobj : splitTopLevelObjects(extractArrayBody(pobj, "vertices"))) {
                    geometry::Vec2 v;
                    extractNumberAfterKey(vobj, "x", v.x);
                    extractNumberAfterKey(vobj, "y", v.y);
                    pl.vertices.push_back(v);
                }
                if (pl.vertices.size() < 2) {
                    continue;
                }
                prim.geometry = pl;
            } else if (pkind == "INSERT") {
                document::InsertEntity nestedIns;
                extractStringField(pobj, "blockName", nestedIns.blockName);
                const auto ipos = pobj.find("\"insertion\"");
                if (ipos != std::string_view::npos) {
                    const auto islice = pobj.substr(ipos);
                    extractNumberAfterKey(islice, "x", nestedIns.insertion.x);
                    extractNumberAfterKey(islice, "y", nestedIns.insertion.y);
                }
                extractNumberAfterKey(pobj, "scale", nestedIns.scale);
                extractNumberAfterKey(pobj, "rotationDeg", nestedIns.rotationDeg);
                prim.geometry = nestedIns;
            } else {
                report.warnings.push_back("Skipping unsupported block primitive: " + pkind);
                continue;
            }
            block.primitives.push_back(std::move(prim));
        }
        // Deserialize block attribute definitions
        for (const auto aobj : splitTopLevelObjects(extractArrayBody(bobj, "attributes"))) {
            document::AttributeDefinition ad;
            extractStringField(aobj, "tag", ad.tag);
            extractStringField(aobj, "prompt", ad.prompt);
            extractStringField(aobj, "defaultValue", ad.defaultValue);
            const auto posPos = aobj.find("\"position\"");
            if (posPos != std::string_view::npos) {
                const auto pslice = aobj.substr(posPos);
                extractNumberAfterKey(pslice, "x", ad.position.x);
                extractNumberAfterKey(pslice, "y", ad.position.y);
            }
            extractNumberAfterKey(aobj, "height", ad.height);
            extractNumberAfterKey(aobj, "rotationDeg", ad.rotationDeg);
            extractBoolAfterKey(aobj, "invisible", ad.invisible);
            double fNum = 0.0;
            if (extractNumberAfterKey(aobj, "flags", fNum)) {
                ad.flags = static_cast<std::uint32_t>(fNum);
            }
            extractBoolAfterKey(aobj, "isMultiline", ad.isMultiline);
            block.attributes.push_back(std::move(ad));
        }
        if (!block.name.empty() && (!block.primitives.empty() || !block.attributes.empty())) {
            blocks.push_back(std::move(block));
        }
    }

    doc.restoreSnapshot(std::move(layers), std::move(entities), activeLayer, nextEntity, nextLayer,
                        unitSystem, name, std::move(blocks), linetypeScale, attdisp);
    report.ok = true;
    report.entityCount = doc.entities().size();
    report.layerCount = doc.layers().size();
    return report;
}

ConversionReport saveNative(const document::Document& doc, const std::filesystem::path& path) {
    ConversionReport report;
    report.formatVersion = kSchemaVersion;
    std::ofstream out(path);
    if (!out) {
        report.errors.push_back("Failed to open path for write: " + path.string());
        return report;
    }
    out << serializeNative(doc);
    if (!out) {
        report.errors.push_back("Write failed: " + path.string());
        return report;
    }
    report.ok = true;
    report.entityCount = doc.entities().size();
    report.layerCount = doc.layers().size();
    return report;
}

ConversionReport loadNative(document::Document& doc, const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        ConversionReport report;
        report.errors.push_back("Failed to open path for read: " + path.string());
        return report;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return deserializeNative(doc, buffer.str());
}

} // namespace mydesk::exchange
