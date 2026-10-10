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

    // First pass: collect all pairs into sections for a stateful walk.
    std::string section;
    std::string currentBlockName;
    bool inBlock = false;
    std::vector<document::BlockDefinition> dxfBlocks;

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

        if (section == "HEADER" && pair.code == 9 && pair.value == "$LTSCALE" && i + 1 < pairs.size() &&
            pairs[i + 1].code == 40) {
            try {
                double scale = std::stod(pairs[i + 1].value);
                if (scale > 0.0) {
                    loaded.setLinetypeScale(scale);
                }
            } catch (...) {}
            ++i;
            continue;
        }

        if (section == "TABLES" && pair.code == 0 && pair.value == "LAYER") {
            std::string name = "0";
            std::string linetype = "CONTINUOUS";
            double lineweightMm = 0.25;
            bool hasLineweight = false;
            std::size_t j = i + 1;
            for (; j < pairs.size() && pairs[j].code != 0; ++j) {
                if (pairs[j].code == 2) {
                    name = pairs[j].value;
                } else if (pairs[j].code == 6) {
                    linetype = pairs[j].value;
                } else if (pairs[j].code == 370) {
                    try {
                        int lwVal = std::stoi(pairs[j].value);
                        if (lwVal >= 0) {
                            double lwMm = static_cast<double>(lwVal) / 100.0;
                            if (document::validLineweightMm(lwMm)) {
                                lineweightMm = lwMm;
                                hasLineweight = true;
                            }
                        }
                    } catch (...) {}
                }
            }
            if (layerIds.find(name) == layerIds.end()) {
                layerIds[name] = loaded.addLayer(name);
            }
            document::Linetype parsed = document::Linetype::Continuous;
            if (document::parseLinetype(linetype, parsed) && parsed != document::Linetype::ByLayer) {
                loaded.setLayerLinetype(layerIds[name], parsed);
            }
            if (hasLineweight) {
                loaded.setLayerLineweight(layerIds[name], lineweightMm);
            }
            i = j - 1;
            continue;
        }

        // --- BLOCKS section parsing ---
        if (section == "BLOCKS") {
            if (pair.code == 0 && pair.value == "BLOCK") {
                inBlock = true;
                currentBlockName.clear();
                // Read block header
                std::size_t j = i + 1;
                for (; j < pairs.size() && pairs[j].code != 0; ++j) {
                    if (pairs[j].code == 2) {
                        currentBlockName = pairs[j].value;
                    }
                }
                document::BlockDefinition bd;
                bd.name = currentBlockName;
                dxfBlocks.push_back(std::move(bd));
                i = j - 1;
                continue;
            }
            if (pair.code == 0 && pair.value == "ENDBLK") {
                inBlock = false;
                // skip ENDBLK groups
                std::size_t j = i + 1;
                for (; j < pairs.size() && pairs[j].code != 0; ++j) {}
                i = j - 1;
                continue;
            }
            if (inBlock && pair.code == 0 && !dxfBlocks.empty()) {
                const std::string type = pair.value;
                double x10 = 0, y20 = 0, x11 = 0, y21 = 0, r40 = 0;
                std::string tag, prompt, defaultVal;
                int flags70 = 0;
                double height = 2.5, rot50 = 0;
                std::size_t j = i + 1;
                for (; j < pairs.size() && pairs[j].code != 0; ++j) {
                    try {
                        switch (pairs[j].code) {
                        case 10: x10 = std::stod(pairs[j].value); break;
                        case 20: y20 = std::stod(pairs[j].value); break;
                        case 11: x11 = std::stod(pairs[j].value); break;
                        case 21: y21 = std::stod(pairs[j].value); break;
                        case 40: r40 = std::stod(pairs[j].value); height = r40; break;
                        case 50: rot50 = std::stod(pairs[j].value); break;
                        case 70: flags70 = std::stoi(pairs[j].value); break;
                        case 1: defaultVal = pairs[j].value; break;
                        case 2: tag = pairs[j].value; break;
                        case 3: prompt = pairs[j].value; break;
                        default: break;
                        }
                    } catch (...) {}
                }
                i = j - 1;
                auto& blk = dxfBlocks.back();
                if (type == "LINE") {
                    document::BlockPrimitive prim;
                    document::LineEntity line;
                    line.start = {x10, y20};
                    line.end = {x11, y21};
                    prim.geometry = line;
                    blk.primitives.push_back(std::move(prim));
                } else if (type == "CIRCLE") {
                    document::BlockPrimitive prim;
                    document::CircleEntity circle;
                    circle.center = {x10, y20};
                    circle.radius = r40;
                    prim.geometry = circle;
                    blk.primitives.push_back(std::move(prim));
                } else if (type == "ATTDEF") {
                    document::AttributeDefinition ad;
                    ad.tag = tag;
                    ad.prompt = prompt;
                    ad.defaultValue = defaultVal;
                    ad.position = {x10, y20};
                    ad.height = height;
                    ad.rotationDeg = rot50;
                    ad.invisible = (flags70 & 1) != 0;
                    blk.attributes.push_back(std::move(ad));
                }
                continue;
            }
            continue;
        }

        if (section != "ENTITIES" || pair.code != 0) {
            continue;
        }

        const std::string type = pair.value;
        if (type == "ENDSEC" || type == "EOF") {
            continue;
        }

        // Skip ATTRIB and SEQEND — they are consumed in INSERT parsing below
        if (type == "ATTRIB" || type == "SEQEND") {
            std::size_t j = i + 1;
            for (; j < pairs.size() && pairs[j].code != 0; ++j) {}
            i = j - 1;
            continue;
        }

        std::string layer = "0";
        std::string entityLinetype;
        int entityLineweight = -1;
        double x10 = 0, y20 = 0, x11 = 0, y21 = 0, r40 = 0, a50 = 0, a51 = 0;
        int flags70 = 0;
        int flags66 = 0;
        std::string blockName2;
        std::string textContents;
        std::string tag, prompt, defaultVal;
        double xScale = 1.0, yScale = 1.0, zScale = 1.0;
        (void)yScale;
        (void)zScale;
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
                case 370:
                    try {
                        entityLineweight = std::stoi(q.value);
                    } catch (...) {}
                    break;
                case 1:
                    textContents = q.value;
                    defaultVal = q.value;
                    break;
                case 2:
                    blockName2 = q.value;
                    tag = q.value;
                    break;
                case 3:
                    prompt = q.value;
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
                case 41:
                    xScale = std::stod(q.value);
                    break;
                case 42:
                    yScale = std::stod(q.value);
                    break;
                case 43:
                    zScale = std::stod(q.value);
                    break;
                case 50:
                    a50 = std::stod(q.value);
                    break;
                case 51:
                    a51 = std::stod(q.value);
                    break;
                case 66:
                    try { flags66 = std::stoi(q.value); } catch (...) {}
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
            } else if (type == "TEXT") {
                created = loaded.addText({x10, y20}, r40 > 0 ? r40 : 2.5, textContents, a50);
            } else if (type == "ATTDEF") {
                created = loaded.addAttdef({x10, y20}, r40 > 0 ? r40 : 2.5, tag, prompt, defaultVal,
                                           a50, (flags70 & 1) != 0);
            } else if (type == "INSERT") {
                // Collect ATTRIB records if attributes-follow flag is set
                std::unordered_map<std::string, std::string> overrides;
                if (flags66 == 1) {
                    // Read ATTRIB records until SEQEND
                    std::size_t k = i + 1;
                    while (k < pairs.size()) {
                        // Find next entity type (code 0)
                        if (pairs[k].code == 0) {
                            if (pairs[k].value == "ATTRIB") {
                                std::string attTag, attVal;
                                std::size_t m = k + 1;
                                for (; m < pairs.size() && pairs[m].code != 0; ++m) {
                                    if (pairs[m].code == 2) attTag = pairs[m].value;
                                    else if (pairs[m].code == 1) attVal = pairs[m].value;
                                }
                                if (!attTag.empty()) {
                                    overrides[attTag] = attVal;
                                }
                                k = m;
                                continue;
                            } else if (pairs[k].value == "SEQEND") {
                                // Skip SEQEND groups
                                std::size_t m = k + 1;
                                for (; m < pairs.size() && pairs[m].code != 0; ++m) {}
                                k = m;
                                break;
                            } else {
                                break; // unexpected entity
                            }
                        }
                        ++k;
                    }
                    i = k - 1;
                }
                // Check if we have a block definition for this
                // First register the block if found in dxfBlocks
                if (!loaded.findBlock(blockName2)) {
                    // If we parsed it from BLOCKS section, define it
                    for (auto& bd : dxfBlocks) {
                        if (bd.name == blockName2) {
                            // Need to add it via restoreSnapshot later — just store for now
                            break;
                        }
                    }
                }
                double scale = xScale; // use X scale as uniform scale
                created = loaded.addInsert(blockName2, {x10, y20}, scale, a50, overrides);
            } else {
                report.warnings.push_back("Unsupported DXF entity: " + type);
            }
        } catch (const std::exception& ex) {
            report.warnings.push_back(std::string("DXF entity error: ") + ex.what());
        }
        if (created != document::kInvalidEntityId) {
            if (!entityLinetype.empty()) {
                document::Linetype parsed = document::Linetype::ByLayer;
                if (document::parseLinetype(entityLinetype, parsed)) {
                    loaded.setEntityLinetype(created, parsed);
                }
            }
            if (entityLineweight >= 0) {
                const double lwMm = static_cast<double>(entityLineweight) / 100.0;
                if (document::validLineweightMm(lwMm)) {
                    loaded.setEntityLineweight(created, lwMm);
                }
            }
        }
    }

    if (loaded.entities().empty() && report.warnings.empty()) {
        report.warnings.push_back("DXF contained no supported entities");
    }

    // Merge dxf blocks with any loaded.blocks() already present (from addInsert)
    auto existingBlocks = loaded.blocks();
    for (auto& bd : dxfBlocks) {
        bool found = false;
        for (const auto& eb : existingBlocks) {
            if (eb.name == bd.name) { found = true; break; }
        }
        if (!found && !bd.name.empty() && (!bd.primitives.empty() || !bd.attributes.empty())) {
            existingBlocks.push_back(std::move(bd));
        }
    }

    // Replace caller's document content.
    doc.restoreSnapshot(loaded.layers(), loaded.entities(), loaded.activeLayerId(),
                        loaded.nextEntityIdPeek(), loaded.nextLayerIdPeek(), loaded.units(),
                        path.stem().string(), std::move(existingBlocks), loaded.linetypeScale(),
                        loaded.attributeDisplayMode());

    report.ok = true;
    report.entityCount = doc.entities().size();
    report.layerCount = doc.layers().size();
    report.warnings.push_back(
        "DXF import: LINE/CIRCLE/ARC/LWPOLYLINE/TEXT/INSERT/ATTDEF/ATTRIB/BLOCKS + LAYER");
    return report;
}

} // namespace mydesk::exchange
