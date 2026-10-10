#include "mydesk/exchange/native_format.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

namespace mydesk::exchange {

namespace {

constexpr const char* kSchemaVersion = "2";

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
    oss << "  \"activeLayerId\": " << doc.activeLayerId() << ",\n";
    oss << "  \"nextEntityId\": " << doc.nextEntityIdPeek() << ",\n";
    oss << "  \"nextLayerId\": " << doc.nextLayerIdPeek() << ",\n";
    oss << "  \"layers\": [\n";
    for (std::size_t i = 0; i < doc.layers().size(); ++i) {
        const auto& layer = doc.layers()[i];
        oss << "    {\"id\": " << layer.id << ", \"name\": \"" << escapeJson(layer.name)
            << "\", \"visible\": " << (layer.visible ? "true" : "false")
            << ", \"locked\": " << (layer.locked ? "true" : "false") << ", \"colorArgb\": "
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
        oss << "    {\"id\": " << entity.id << ", \"layerId\": " << entity.layerId << ", \"kind\": \""
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
        }
        oss << "}";
        if (i + 1 < doc.entities().size()) {
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
    if (schema != kSchemaVersion) {
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

    std::vector<document::Layer> layers;
    for (const auto obj : splitTopLevelObjects(extractArrayBody(json, "layers"))) {
        document::Layer layer;
        std::uint64_t id = 0;
        extractUintAfterKey(obj, "id", id);
        layer.id = id;
        extractStringField(obj, "name", layer.name);
        // visible/locked default true/false; keep defaults for MVP parser simplicity
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
        } else {
            report.warnings.push_back("Unsupported entity kind: " + kind);
            continue;
        }
        entities.push_back(entity);
    }

    doc.restoreSnapshot(std::move(layers), std::move(entities), activeLayer, nextEntity, nextLayer,
                        unitSystem, name);
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
