#include "mydesk/exchange/dxf_reader.hpp"

#include <fstream>
#include <unordered_map>
#include <vector>

namespace mydesk::exchange {

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Pair {
    int code{0};
    std::string value;
};

bool readPair(std::istream& in, Pair& pair) {
    std::string codeLine;
    std::string valueLine;
    if (!std::getline(in, codeLine) || !std::getline(in, valueLine)) {
        return false;
    }
    if (!codeLine.empty() && codeLine.back() == '\r') {
        codeLine.pop_back();
    }
    if (!valueLine.empty() && valueLine.back() == '\r') {
        valueLine.pop_back();
    }
    try {
        pair.code = std::stoi(codeLine);
    } catch (...) {
        return false;
    }
    pair.value = valueLine;
    return true;
}

} // namespace

ConversionReport importDxf(document::Document& doc, const std::filesystem::path& path) {
    ConversionReport report;
    report.format = "dxf";
    report.formatVersion = "ascii-partial";

    std::ifstream in(path);
    if (!in) {
        report.errors.push_back("Failed to open DXF: " + path.string());
        return report;
    }

    // Slurp all pairs for a simple stateful walk.
    std::vector<Pair> pairs;
    Pair p;
    while (readPair(in, p)) {
        pairs.push_back(p);
    }

    document::Document loaded;
    std::unordered_map<std::string, document::LayerId> layerIds;
    layerIds["0"] = loaded.activeLayerId();

    std::string section;
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        const auto& pair = pairs[i];
        if (pair.code == 0 && pair.value == "SECTION" && i + 1 < pairs.size() &&
            pairs[i + 1].code == 2) {
            section = pairs[i + 1].value;
            ++i;
            continue;
        }
        if (pair.code == 0 && pair.value == "ENDSEC") {
            section.clear();
            continue;
        }
        if (pair.code == 0 && pair.value == "EOF") {
            break;
        }

        if (section == "TABLES" && pair.code == 0 && pair.value == "LAYER") {
            std::string name = "0";
            std::string linetype = "CONTINUOUS";
            std::size_t j = i + 1;
            for (; j < pairs.size() && pairs[j].code != 0; ++j) {
                if (pairs[j].code == 2) {
                    name = pairs[j].value;
                } else if (pairs[j].code == 6) {
                    linetype = pairs[j].value;
                }
            }
            if (layerIds.find(name) == layerIds.end()) {
                layerIds[name] = loaded.addLayer(name);
            }
            document::Linetype parsed = document::Linetype::Continuous;
            if (document::parseLinetype(linetype, parsed) && parsed != document::Linetype::ByLayer) {
                loaded.setLayerLinetype(layerIds[name], parsed);
            }
            i = j - 1;
            continue;
        }

        if (section != "ENTITIES" || pair.code != 0) {
            continue;
        }

        const std::string type = pair.value;
        if (type == "ENDSEC" || type == "EOF") {
            continue;
        }

        std::string layer = "0";
        std::string entityLinetype;
        double x10 = 0, y20 = 0, x11 = 0, y21 = 0, r40 = 0, a50 = 0, a51 = 0;
        int flags70 = 0;
        std::vector<geometry::Vec2> lwVerts;
        double pendingX = 0;
        bool havePendingX = false;

        std::size_t j = i + 1;
        for (; j < pairs.size() && pairs[j].code != 0; ++j) {
            const auto& q = pairs[j];
            try {
                switch (q.code) {
                case 8:
                    layer = q.value;
                    break;
                case 6:
                    entityLinetype = q.value;
                    break;
                case 10:
                    pendingX = std::stod(q.value);
                    havePendingX = true;
                    x10 = pendingX;
                    break;
                case 20:
                    y20 = std::stod(q.value);
                    if (type == "LWPOLYLINE" && havePendingX) {
                        lwVerts.push_back({pendingX, y20});
                        havePendingX = false;
                    }
                    break;
                case 11:
                    x11 = std::stod(q.value);
                    break;
                case 21:
                    y21 = std::stod(q.value);
                    break;
                case 40:
                    r40 = std::stod(q.value);
                    break;
                case 50:
                    a50 = std::stod(q.value);
                    break;
                case 51:
                    a51 = std::stod(q.value);
                    break;
                case 70:
                    flags70 = std::stoi(q.value);
                    break;
                default:
                    break;
                }
            } catch (...) {
                report.warnings.push_back("Bad numeric group in " + type);
            }
        }
        i = j - 1;

        if (layerIds.find(layer) == layerIds.end()) {
            layerIds[layer] = loaded.addLayer(layer);
        }
        loaded.setActiveLayer(layerIds[layer]);

        document::EntityId created = document::kInvalidEntityId;
        try {
            if (type == "LINE") {
                created = loaded.addLine({x10, y20}, {x11, y21});
            } else if (type == "CIRCLE") {
                created = loaded.addCircle({x10, y20}, r40);
            } else if (type == "ARC") {
                created = loaded.addArc({x10, y20}, r40, a50 * kPi / 180.0, a51 * kPi / 180.0);
            } else if (type == "LWPOLYLINE") {
                if (lwVerts.size() >= 2) {
                    created = loaded.addPolyline(lwVerts, (flags70 & 1) != 0);
                } else {
                    report.warnings.push_back("Skipped LWPOLYLINE with <2 vertices");
                }
            } else {
                report.warnings.push_back("Unsupported DXF entity: " + type);
            }
        } catch (const std::exception& ex) {
            report.warnings.push_back(std::string("DXF entity error: ") + ex.what());
        }
        if (created != document::kInvalidEntityId && !entityLinetype.empty()) {
            document::Linetype parsed = document::Linetype::ByLayer;
            if (document::parseLinetype(entityLinetype, parsed)) {
                loaded.setEntityLinetype(created, parsed);
            }
        }
    }

    if (loaded.entities().empty() && report.warnings.empty()) {
        report.warnings.push_back("DXF contained no supported entities");
    }

    // Replace caller's document content.
    doc.restoreSnapshot(loaded.layers(), loaded.entities(), loaded.activeLayerId(),
                        loaded.nextEntityIdPeek(), loaded.nextLayerIdPeek(), loaded.units(),
                        path.stem().string());
    // restoreSnapshot takes by value - need copies. layers()/entities() return const refs —
    // restoreSnapshot moves from vectors - must copy first.
    // Actually we passed loaded.layers() which is const ref - restoreSnapshot takes vector by value
    // so it copies. Good.

    report.ok = true;
    report.entityCount = doc.entities().size();
    report.layerCount = doc.layers().size();
    report.warnings.push_back(
        "DXF import is partial: LINE/CIRCLE/ARC/LWPOLYLINE/LAYER only; dims/blocks/xrefs unsupported");
    return report;
}

} // namespace mydesk::exchange
