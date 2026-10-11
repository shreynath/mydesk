// SPDX-License-Identifier: MPL-2.0
#include "mydesk/fusion/fusion_document.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>

#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

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

std::size_t countUnique(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind) {
    TopTools_IndexedMapOfShape items;
    TopExp::MapShapes(shape, kind, items);
    return static_cast<std::size_t>(items.Extent());
}

SolidBody makeBody(FeatureId featureId, std::string name, const TopoDS_Shape& shape) {
    SolidBody body;
    body.featureId = featureId;
    body.name = std::move(name);
    body.shape = shape;
    body.isValid = !shape.IsNull() && BRepCheck_Analyzer(shape).IsValid();
    body.solidCount = countUnique(shape, TopAbs_SOLID);
    body.shellCount = countUnique(shape, TopAbs_SHELL);
    body.faceCount = countUnique(shape, TopAbs_FACE);
    body.edgeCount = countUnique(shape, TopAbs_EDGE);
    body.vertexCount = countUnique(shape, TopAbs_VERTEX);

    GProp_GProps volumeProperties;
    BRepGProp::VolumeProperties(shape, volumeProperties);
    body.volumeMm3 = volumeProperties.Mass();
    GProp_GProps surfaceProperties;
    BRepGProp::SurfaceProperties(shape, surfaceProperties);
    body.surfaceAreaMm2 = surfaceProperties.Mass();

    Bnd_Box bounds;
    BRepBndLib::AddOptimal(shape, bounds, false, false);
    bounds.Get(body.bboxMin[0], body.bboxMin[1], body.bboxMin[2],
               body.bboxMax[0], body.bboxMax[1], body.bboxMax[2]);
    return body;
}

std::string coordinateWord(char axis, double value) {
    std::ostringstream out;
    out << axis << std::fixed << std::setprecision(3) << value;
    return out.str();
}

void appendLinearMove(CAMSetup& setup, double x, double y, double z,
                      std::string_view feedSuffix = {}) {
    setup.toolpath.push_back({x, y, z, false});
    std::string line = "G01 " + coordinateWord('X', x) + " " +
                       coordinateWord('Y', y) + " " + coordinateWord('Z', z);
    if (!feedSuffix.empty()) {
        line += " ";
        line += feedSuffix;
    }
    setup.gcodeLines.push_back(std::move(line));
}

std::vector<double> passCoordinates(double minimum, double maximum, double spacing) {
    std::vector<double> values{minimum};
    while (values.back() + spacing < maximum - 1e-9) {
        values.push_back(values.back() + spacing);
    }
    if (maximum > minimum + 1e-9) {
        values.push_back(maximum);
    }
    return values;
}

} // namespace

FusionDocument::FusionDocument() = default;

FusionDocument::Snapshot FusionDocument::snapshot() const {
    return {name_, timeline_, timelineCursor_, nextId_};
}

void FusionDocument::restore(const Snapshot& state) {
    name_ = state.name;
    timeline_ = state.timeline;
    timelineCursor_ = state.timelineCursor;
    nextId_ = state.nextId;
    regenerate();
}

void FusionDocument::checkpoint() {
    undo_.push_back(snapshot());
    redo_.clear();
}

void FusionDocument::setName(std::string name) {
    if (name == name_) return;
    checkpoint();
    name_ = std::move(name);
}

void FusionDocument::clearHistory() {
    undo_.clear();
    redo_.clear();
}

bool FusionDocument::undo() {
    if (undo_.empty()) return false;
    redo_.push_back(snapshot());
    const auto state = undo_.back();
    undo_.pop_back();
    restore(state);
    return true;
}

bool FusionDocument::redo() {
    if (redo_.empty()) return false;
    undo_.push_back(snapshot());
    const auto state = redo_.back();
    redo_.pop_back();
    restore(state);
    return true;
}

FeatureId FusionDocument::addSketchRectangle(std::string name, double width, double height) {
    checkpoint();
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
    checkpoint();
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
    checkpoint();
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

FeatureId FusionDocument::addCAMPocket(std::string name, FeatureId solidId, double toolDia,
                                       double depth, double stepover, double stepdown) {
    checkpoint();
    TimelineEntry e;
    e.id = nextId_++;
    e.kind = TimelineKind::CAMOperation;
    e.name = std::move(name);
    e.parameters["targetSolidId"] = static_cast<double>(solidId);
    e.parameters["toolDiameter"] = toolDia;
    e.parameters["depth"] = depth;
    e.parameters["stepover"] = stepover;
    e.parameters["stepdown"] = stepdown;
    timeline_.push_back(std::move(e));
    timelineCursor_ = timeline_.size();
    regenerate();
    return timeline_.back().id;
}

FeatureId FusionDocument::addECADBoard(std::string name, double width, double height, double thickness) {
    checkpoint();
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
    const auto bounded = std::min(cursor, timeline_.size());
    if (bounded == timelineCursor_) return;
    checkpoint();
    timelineCursor_ = bounded;
    regenerate();
}

void FusionDocument::suppressFeature(FeatureId id, bool suppress) {
    for (auto& e : timeline_) {
        if (e.id == id) {
            if (e.suppressed == suppress) return;
            checkpoint();
            e.suppressed = suppress;
            break;
        }
    }
    regenerate();
}

bool FusionDocument::renameFeature(FeatureId id, std::string name) {
    if (name.empty()) return false;
    for (auto& entry : timeline_) {
        if (entry.id != id) continue;
        if (entry.name == name) return true;
        checkpoint();
        entry.name = std::move(name);
        regenerate();
        return true;
    }
    return false;
}

bool FusionDocument::setFeatureParameter(FeatureId id, std::string_view parameter, double value) {
    if (!std::isfinite(value)) return false;
    for (auto& entry : timeline_) {
        if (entry.id != id) continue;
        const auto found = entry.parameters.find(std::string(parameter));
        if (found == entry.parameters.end()) return false;
        if (found->second == value) return true;
        const auto before = snapshot();
        found->second = value;
        if (!regenerate()) {
            const std::string error = lastError_;
            restore(before);
            lastError_ = error;
            return false;
        }
        undo_.push_back(before);
        redo_.clear();
        return true;
    }
    return false;
}

bool FusionDocument::regenerate() {
    bodies_.clear();
    camSetups_.clear();
    ecadBoard_.reset();
    lastError_.clear();

    std::unordered_map<FeatureId, const TimelineEntry*> entryMap;
    std::unordered_map<FeatureId, std::size_t> bodyIndexByFeature;
    try {
        for (std::size_t i = 0; i < timelineCursor_; ++i) {
            const auto& entry = timeline_[i];
            if (entry.suppressed) continue;
            entryMap[entry.id] = &entry;

            if (entry.kind == TimelineKind::Sketch) {
                if (!(entry.parameters.at("width") > 0.0) ||
                    !(entry.parameters.at("height") > 0.0)) {
                    lastError_ = "sketch dimensions must be positive";
                    return false;
                }
            } else if (entry.kind == TimelineKind::Extrude) {
                const auto sketchId = static_cast<FeatureId>(entry.parameters.at("sketchId"));
                const auto skIt = entryMap.find(sketchId);
                if (skIt == entryMap.end() || skIt->second->kind != TimelineKind::Sketch) {
                    lastError_ = "extrude references an unavailable rectangle sketch";
                    return false;
                }
                const double width = skIt->second->parameters.at("width");
                const double height = skIt->second->parameters.at("height");
                const double depth = entry.parameters.at("depth");
                if (!(depth > 0.0)) {
                    lastError_ = "extrude depth must be positive";
                    return false;
                }

                const TopoDS_Shape shape = BRepPrimAPI_MakeBox(width, height, depth).Shape();
                SolidBody body = makeBody(entry.id, entry.name + "_Body", shape);
                if (!body.isValid || body.solidCount != 1) {
                    lastError_ = "OCCT produced an invalid extrude solid";
                    return false;
                }
                bodyIndexByFeature[entry.id] = bodies_.size();
                bodies_.push_back(std::move(body));
            } else if (entry.kind == TimelineKind::DirectPushPull) {
                const auto targetId = static_cast<FeatureId>(entry.parameters.at("targetSolidId"));
                const auto target = bodyIndexByFeature.find(targetId);
                if (target == bodyIndexByFeature.end()) {
                    lastError_ = "push/pull references an unavailable solid";
                    return false;
                }
                const std::size_t bodyIndex = target->second;
                auto& body = bodies_[bodyIndex];
                const double width = body.bboxMax[0] - body.bboxMin[0];
                const double height = body.bboxMax[1] - body.bboxMin[1];
                const double newDepth = body.bboxMax[2] - body.bboxMin[2] +
                                        entry.parameters.at("offset");
                if (!(newDepth > 0.0)) {
                    lastError_ = "push/pull offset collapses or inverts the solid";
                    return false;
                }
                const TopoDS_Shape shape = BRepPrimAPI_MakeBox(width, height, newDepth).Shape();
                body = makeBody(entry.id, entry.name + "_Body", shape);
                if (!body.isValid) {
                    lastError_ = "OCCT produced an invalid push/pull solid";
                    return false;
                }
                // The direct feature supersedes its source body, while either persistent
                // feature reference continues to resolve to the regenerated result.
                bodyIndexByFeature[targetId] = bodyIndex;
                bodyIndexByFeature[entry.id] = bodyIndex;
            } else if (entry.kind == TimelineKind::CAMOperation) {
                const auto targetId = static_cast<FeatureId>(entry.parameters.at("targetSolidId"));
                const auto target = bodyIndexByFeature.find(targetId);
                if (target == bodyIndexByFeature.end()) {
                    lastError_ = "CAM pocket references an unavailable solid";
                    return false;
                }
                const SolidBody& body = bodies_[target->second];
                const double toolDiameter = entry.parameters.at("toolDiameter");
                const double depth = entry.parameters.at("depth");
                const double stepover = entry.parameters.at("stepover");
                const double stepdown = entry.parameters.at("stepdown");
                const double bodyDepth = body.bboxMax[2] - body.bboxMin[2];
                if (!(toolDiameter > 0.0) || !(depth > 0.0) ||
                    !(stepover > 0.0) || stepover > toolDiameter ||
                    !(stepdown > 0.0) || depth > bodyDepth + 1e-9) {
                    lastError_ = "invalid pocket tool, depth, stepover, or stepdown";
                    return false;
                }

                const double radius = toolDiameter * 0.5;
                const double minX = body.bboxMin[0] + radius;
                const double maxX = body.bboxMax[0] - radius;
                const double minY = body.bboxMin[1] + radius;
                const double maxY = body.bboxMax[1] - radius;
                if (maxX < minX || maxY < minY) {
                    lastError_ = "pocket boundary is smaller than the selected cutter";
                    return false;
                }

                CAMSetup setup;
                setup.operationName = entry.name;
                setup.targetBodyFeatureId = targetId;
                setup.toolDiameterMm = toolDiameter;
                setup.pocketDepthMm = depth;
                setup.stepoverMm = stepover;
                setup.stepdownMm = stepdown;
                setup.spindleRpm = 10000.0;
                setup.feedRateMmMin = 1200.0;
                setup.boundaryMin[0] = minX;
                setup.boundaryMin[1] = minY;
                setup.boundaryMax[0] = maxX;
                setup.boundaryMax[1] = maxY;

                const double safeZ = body.bboxMax[2] + 5.0;
                setup.gcodeLines.push_back("G90 G21 (Absolute metric)");
                setup.gcodeLines.push_back("S10000 M03 (Spindle CW 10k RPM)");
                setup.gcodeLines.push_back("G00 " + coordinateWord('Z', safeZ));
                setup.gcodeLines.push_back("G00 " + coordinateWord('X', minX) + " " +
                                           coordinateWord('Y', minY));
                setup.toolpath.push_back({minX, minY, safeZ, true});

                const auto rows = passCoordinates(minY, maxY, stepover);
                double removedDepth = 0.0;
                while (removedDepth < depth - 1e-9) {
                    removedDepth = std::min(depth, removedDepth + stepdown);
                    const double z = body.bboxMax[2] - removedDepth;
                    appendLinearMove(setup, minX, minY, z, "F300");

                    double currentX = minX;
                    double currentY = minY;
                    for (std::size_t row = 0; row < rows.size(); ++row) {
                        const double targetX = (row % 2 == 0) ? maxX : minX;
                        setup.totalPathLengthMm += std::abs(targetX - currentX);
                        appendLinearMove(setup, targetX, rows[row], z,
                                         row == 0 ? "F1200" : std::string_view{});
                        currentX = targetX;
                        currentY = rows[row];
                        if (row + 1 < rows.size()) {
                            setup.totalPathLengthMm += rows[row + 1] - currentY;
                            appendLinearMove(setup, currentX, rows[row + 1], z);
                            currentY = rows[row + 1];
                        }
                    }
                    setup.gcodeLines.push_back("G00 " + coordinateWord('Z', safeZ));
                    setup.gcodeLines.push_back("G00 " + coordinateWord('X', minX) + " " +
                                               coordinateWord('Y', minY));
                    setup.toolpath.push_back({minX, minY, safeZ, true});
                }
                setup.gcodeLines.push_back("M05 M30");
                camSetups_.push_back(std::move(setup));
            } else if (entry.kind == TimelineKind::ECADBoard) {
                ECADBoard board;
                board.widthMm = entry.parameters.at("width");
                board.heightMm = entry.parameters.at("height");
                board.thicknessMm = entry.parameters.at("thickness");
                if (!(board.widthMm > 0.0) || !(board.heightMm > 0.0) ||
                    !(board.thicknessMm > 0.0)) {
                    lastError_ = "ECAD board dimensions must be positive";
                    return false;
                }
                ecadBoard_ = board;
            }
        }
    } catch (const Standard_Failure& failure) {
        lastError_ = std::string("OCCT regeneration failed: ") + failure.GetMessageString();
        return false;
    } catch (const std::exception& exception) {
        lastError_ = std::string("timeline regeneration failed: ") + exception.what();
        return false;
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
    oss << "  \"timelineCursor\": " << timelineCursor_ << ",\n";
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
    double cursor = std::numeric_limits<double>::quiet_NaN();
    extractNumber(json, "timelineCursor", cursor);

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
        const auto paramsEnd = json.find('}', idPos);
        const auto objectEnd = paramsEnd == std::string_view::npos
            ? std::string_view::npos : json.find('}', paramsEnd + 1);
        const auto entryJson = json.substr(idPos, objectEnd == std::string_view::npos
            ? std::string_view::npos : objectEnd - idPos + 1);
        entry.suppressed = (entryJson.find("\"suppressed\": true") != std::string_view::npos);

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
    doc.timelineCursor_ = std::isfinite(cursor)
        ? std::min(static_cast<std::size_t>(std::max(0.0, cursor)), doc.timeline_.size())
        : doc.timeline_.size();
    doc.regenerate();
    doc.clearHistory();
    return doc;
}

} // namespace mydesk::fusion
