// SPDX-License-Identifier: MPL-2.0
#include "mydesk/fusion/fusion_document.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace mydesk::fusion {

namespace {

std::string escapeJson(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

bool extractString(std::string_view json, std::string_view key, std::string& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) return false;
    const auto q1 = json.find('"', json.find(':', pos));
    if (q1 == std::string_view::npos) return false;
    const auto q2 = json.find('"', q1 + 1);
    if (q2 == std::string_view::npos) return false;
    out = std::string(json.substr(q1 + 1, q2 - q1 - 1));
    return true;
}

bool extractNumber(std::string_view json, std::string_view key, double& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) return false;
    const auto colon = json.find(':', pos);
    if (colon == std::string_view::npos) return false;
    try {
        out = std::stod(std::string(json.substr(colon + 1)));
        return std::isfinite(out);
    } catch (...) {
        return false;
    }
}

} // namespace

FusionDocument::FusionDocument() = default;

FeatureId FusionDocument::addSketchRectangle(std::string name, double width, double height) {
    TimelineEntry e;
    e.id = nextId_++;
    e.kind = TimelineKind::Sketch;
    e.name = std::move(name);
    e.parameters["width"] = width;
    e.parameters["height"] = height;
    timeline_.push_back(std::move(e));
    timelineCursor_ = timeline_.size();
    regenerate();
    return timeline_.back().id;
}

FeatureId FusionDocument::addExtrude(std::string name, FeatureId sketchId, double depth) {
    TimelineEntry e;
    e.id = nextId_++;
    e.kind = TimelineKind::Extrude;
    e.name = std::move(name);
    e.parameters["sketchId"] = static_cast<double>(sketchId);
    e.parameters["depth"] = depth;
    timeline_.push_back(std::move(e));
    timelineCursor_ = timeline_.size();
    regenerate();
    return timeline_.back().id;
}

FeatureId FusionDocument::addDirectPushPull(std::string name, FeatureId solidId, double offset) {
    TimelineEntry e;
    e.id = nextId_++;
    e.kind = TimelineKind::DirectPushPull;
    e.name = std::move(name);
    e.parameters["targetSolidId"] = static_cast<double>(solidId);
    e.parameters["offset"] = offset;
    timeline_.push_back(std::move(e));
    timelineCursor_ = timeline_.size();
    regenerate();
    return timeline_.back().id;
}

FeatureId FusionDocument::addCAMPocket(std::string name, double toolDia, double depth, double stepover) {
    TimelineEntry e;
    e.id = nextId_++;
    e.kind = TimelineKind::CAMOperation;
    e.name = std::move(name);
    e.parameters["toolDiameter"] = toolDia;
    e.parameters["depth"] = depth;
    e.parameters["stepover"] = stepover;
    timeline_.push_back(std::move(e));
    timelineCursor_ = timeline_.size();
    regenerate();
    return timeline_.back().id;
}

FeatureId FusionDocument::addECADBoard(std::string name, double width, double height, double thickness) {
    TimelineEntry e;
    e.id = nextId_++;
    e.kind = TimelineKind::ECADBoard;
    e.name = std::move(name);
    e.parameters["width"] = width;
    e.parameters["height"] = height;
    e.parameters["thickness"] = thickness;
    timeline_.push_back(std::move(e));
    timelineCursor_ = timeline_.size();
    regenerate();
    return timeline_.back().id;
}

void FusionDocument::setTimelineCursor(std::size_t cursor) {
    timelineCursor_ = std::min(cursor, timeline_.size());
    regenerate();
}

void FusionDocument::suppressFeature(FeatureId id, bool suppress) {
    for (auto& e : timeline_) {
        if (e.id == id) {
            e.suppressed = suppress;
            break;
        }
    }
    regenerate();
}

bool FusionDocument::regenerate() {
    bodies_.clear();
    camSetups_.clear();
    ecadBoard_.reset();

    std::unordered_map<FeatureId, const TimelineEntry*> entryMap;
    for (std::size_t i = 0; i < timelineCursor_; ++i) {
        const auto& entry = timeline_[i];
        if (entry.suppressed) continue;
        entryMap[entry.id] = &entry;

        if (entry.kind == TimelineKind::Extrude) {
            auto skIt = entryMap.find(static_cast<FeatureId>(entry.parameters.at("sketchId")));
            if (skIt != entryMap.end()) {
                const double w = skIt->second->parameters.at("width");
                const double h = skIt->second->parameters.at("height");
                const double d = entry.parameters.at("depth");

                SolidBody body;
                body.name = entry.name + "_Body";
                body.volumeMm3 = w * h * d;
                body.surfaceAreaMm2 = 2.0 * (w * h + w * d + h * d);
                body.bboxMin[0] = 0.0; body.bboxMin[1] = 0.0; body.bboxMin[2] = 0.0;
                body.bboxMax[0] = w; body.bboxMax[1] = h; body.bboxMax[2] = d;
                bodies_.push_back(body);
            }
        } else if (entry.kind == TimelineKind::DirectPushPull) {
            if (!bodies_.empty()) {
                const double offset = entry.parameters.at("offset");
                auto& body = bodies_.back();
                body.bboxMax[2] += offset;
                const double w = body.bboxMax[0] - body.bboxMin[0];
                const double h = body.bboxMax[1] - body.bboxMin[1];
                const double d = body.bboxMax[2] - body.bboxMin[2];
                body.volumeMm3 = w * h * d;
                body.surfaceAreaMm2 = 2.0 * (w * h + w * d + h * d);
            }
        } else if (entry.kind == TimelineKind::CAMOperation) {
            const double toolDia = entry.parameters.at("toolDiameter");
            const double depth = entry.parameters.at("depth");
            const double stepover = entry.parameters.at("stepover");

            CAMSetup setup;
            setup.operationName = entry.name;
            setup.toolDiameterMm = toolDia;
            setup.spindleRpm = 10000.0;
            setup.feedRateMmMin = 1200.0;

            setup.gcodeLines.push_back("G90 G21 (Absolute metric)");
            setup.gcodeLines.push_back("S10000 M03 (Spindle CW 10k RPM)");
            setup.gcodeLines.push_back("G00 Z5.000");

            double cutLen = 0.0;
            for (double z = -stepover; z >= -depth - 1e-6; z -= stepover) {
                setup.gcodeLines.push_back("G01 Z" + std::to_string(z) + " F300");
                setup.gcodeLines.push_back("G01 X50.000 F1200");
                setup.gcodeLines.push_back("G01 Y50.000");
                setup.gcodeLines.push_back("G01 X0.000");
                setup.gcodeLines.push_back("G01 Y0.000");
                cutLen += 200.0;
            }
            setup.gcodeLines.push_back("G00 Z10.000");
            setup.gcodeLines.push_back("M05 M30");
            setup.totalPathLengthMm = cutLen;
            camSetups_.push_back(setup);
        } else if (entry.kind == TimelineKind::ECADBoard) {
            ECADBoard board;
            board.widthMm = entry.parameters.at("width");
            board.heightMm = entry.parameters.at("height");
            board.thicknessMm = entry.parameters.at("thickness");
            ecadBoard_ = board;
        }
    }
    return true;
}

std::string FusionDocument::exportGCode() const {
    std::ostringstream ss;
    ss << "% (Generated by MyDesk Fusion CAM)\n";
    for (const auto& setup : camSetups_) {
        ss << "(Op: " << setup.operationName << ")\n";
        for (const auto& line : setup.gcodeLines) {
            ss << line << "\n";
        }
    }
    ss << "%\n";
    return ss.str();
}

std::string FusionDocument::serializeNative() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"format\": \"mydesk-fusion\",\n";
    oss << "  \"name\": \"" << escapeJson(name_) << "\",\n";
    oss << "  \"timeline\": [\n";
    for (std::size_t i = 0; i < timeline_.size(); ++i) {
        const auto& e = timeline_[i];
        oss << "    {\"id\": " << e.id << ", \"kind\": " << static_cast<int>(e.kind)
            << ", \"name\": \"" << escapeJson(e.name) << "\", \"suppressed\": "
            << (e.suppressed ? "true" : "false") << ", \"params\": {";
        bool first = true;
        for (const auto& [k, v] : e.parameters) {
            if (!first) oss << ", ";
            first = false;
            oss << "\"" << escapeJson(k) << "\": " << v;
        }
        oss << "}}";
        if (i + 1 < timeline_.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n}\n";
    return oss.str();
}

FusionDocument FusionDocument::deserializeNative(std::string_view json, std::string* error) {
    FusionDocument doc;
    std::string format;
    if (!extractString(json, "format", format) || format != "mydesk-fusion") {
        if (error) *error = "not a mydesk-fusion document";
        return doc;
    }
    extractString(json, "name", doc.name_);

    // Parse timeline entries
    std::size_t search = 0;
    while (true) {
        const auto idPos = json.find("\"id\":", search);
        if (idPos == std::string_view::npos) break;
        TimelineEntry entry;
        double id = 0;
        extractNumber(json.substr(idPos, 30), "id", id);
        entry.id = static_cast<FeatureId>(id);
        const auto kindPos = json.find("\"kind\":", idPos);
        if (kindPos != std::string_view::npos) {
            double kind = 0;
            extractNumber(json.substr(kindPos, 30), "kind", kind);
            entry.kind = static_cast<TimelineKind>(kind);
        }
        extractString(json.substr(idPos, 120), "name", entry.name);
        entry.suppressed = (json.find("\"suppressed\": true", idPos) != std::string_view::npos);

        const auto paramsPos = json.find("\"params\":", idPos);
        if (paramsPos != std::string_view::npos) {
            const auto lb = json.find('{', paramsPos);
            const auto rb = json.find('}', lb);
            if (lb != std::string_view::npos && rb != std::string_view::npos) {
                std::string body(json.substr(lb + 1, rb - lb - 1));
                std::stringstream ss(body);
                std::string token;
                while (std::getline(ss, token, ',')) {
                    const auto q1 = token.find('"');
                    const auto q2 = token.find('"', q1 + 1);
                    const auto colon = token.find(':');
                    if (q1 != std::string_view::npos && q2 != std::string_view::npos && colon != std::string_view::npos) {
                        const std::string key = token.substr(q1 + 1, q2 - q1 - 1);
                        const double val = std::stod(token.substr(colon + 1));
                        entry.parameters[key] = val;
                    }
                }
            }
        }
        doc.timeline_.push_back(entry);
        doc.nextId_ = std::max(doc.nextId_, entry.id + 1);
        search = idPos + 10;
    }
    doc.timelineCursor_ = doc.timeline_.size();
    doc.regenerate();
    return doc;
}

} // namespace mydesk::fusion
