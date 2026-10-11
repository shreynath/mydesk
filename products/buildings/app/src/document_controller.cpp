#include "mydesk/buildings/document_controller.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <system_error>

namespace mydesk::buildings {
namespace {

bool writeAtomically(const std::filesystem::path& path, const std::string& contents,
                     std::string* error) {
    if (path.empty()) {
        if (error) *error = "No file path was selected";
        return false;
    }
    const auto temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output || !output.write(contents.data(), static_cast<std::streamsize>(contents.size()))) {
            if (error) *error = "Could not write " + temporary;
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(temporary, path, ec);
    }
    if (ec) {
        if (error) *error = "Could not replace " + path.string() + ": " + ec.message();
        std::filesystem::remove(temporary, ec);
        return false;
    }
    return true;
}

} // namespace

DocumentController::DocumentController() { newProject(); }

void DocumentController::ensureDefaults() {
    if (document_.levels.empty()) document_.addLevel("Level 1", 0.0);
    if (document_.wallTypes.empty()) document_.addWallType("Generic - 200 mm", 0.2, 3.0);
    if (document_.doorTypes.empty()) document_.addDoorType("Single - 900 x 2100 mm", 0.9, 2.1);
    if (document_.windowTypes.empty()) document_.addWindowType("Fixed - 1200 x 1500 mm", 1.2, 1.5, 0.9);
}

void DocumentController::newProject(std::string name) {
    document_ = ProjectDocument{};
    document_.name = std::move(name);
    ensureDefaults();
    selection_ = {};
    filePath_.clear();
    undo_.clear();
    redo_.clear();
    modified_ = false;
}

bool DocumentController::open(const std::filesystem::path& path, std::string* error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (error) *error = "Could not open " + path.string();
        return false;
    }
    const std::string contents((std::istreambuf_iterator<char>(input)), {});
    std::string parseError;
    auto loaded = ProjectDocument::deserializeNative(contents, &parseError);
    if (!parseError.empty()) {
        if (error) *error = parseError;
        return false;
    }
    document_ = std::move(loaded);
    ensureDefaults();
    filePath_ = path;
    selection_ = {};
    undo_.clear();
    redo_.clear();
    modified_ = false;
    return true;
}

bool DocumentController::save(std::string* error) {
    if (filePath_.empty()) {
        if (error) *error = "Save As is required for a new project";
        return false;
    }
    if (!writeAtomically(filePath_, document_.serializeNative(), error)) return false;
    modified_ = false;
    return true;
}

bool DocumentController::saveAs(const std::filesystem::path& path, std::string* error) {
    if (!writeAtomically(path, document_.serializeNative(), error)) return false;
    filePath_ = path;
    modified_ = false;
    return true;
}

bool DocumentController::exportIfc(const std::filesystem::path& path, std::string* error) const {
    return writeAtomically(path, document_.exportIfc4(), error);
}

void DocumentController::checkpoint() {
    undo_.push_back(document_.serializeNative());
    if (undo_.size() > 100) undo_.erase(undo_.begin());
    redo_.clear();
}

bool DocumentController::restore(const std::string& state) {
    std::string error;
    auto restored = ProjectDocument::deserializeNative(state, &error);
    if (!error.empty()) return false;
    document_ = std::move(restored);
    selection_ = {};
    modified_ = true;
    return true;
}

LevelId DocumentController::createLevel(std::string name, double elevation) {
    if (name.empty() || !std::isfinite(elevation)) return 0;
    checkpoint();
    const auto id = document_.addLevel(std::move(name), elevation);
    modified_ = true;
    selection_ = {SelectionKind::Level, id};
    return id;
}

ElementId DocumentController::createWall(LevelId level, double x0, double y0, double x1, double y1) {
    if (!document_.findLevel(level) || std::hypot(x1 - x0, y1 - y0) < 0.01) return 0;
    checkpoint();
    const auto id = document_.addWall(document_.wallTypes.front().id, level, x0, y0, x1, y1);
    modified_ = true;
    selection_ = {SelectionKind::Wall, id};
    return id;
}

ElementId DocumentController::createDoor(ElementId hostWall, double offset) {
    if (!document_.findWall(hostWall) || offset < 0.0 || offset > 1.0) return 0;
    checkpoint();
    const auto id = document_.addDoor(document_.doorTypes.front().id, hostWall, offset);
    if (!id) { undo_.pop_back(); return 0; }
    modified_ = true;
    selection_ = {SelectionKind::Door, id};
    return id;
}

ElementId DocumentController::createWindow(ElementId hostWall, double offset) {
    if (!document_.findWall(hostWall) || offset < 0.0 || offset > 1.0) return 0;
    checkpoint();
    const auto id = document_.addWindow(document_.windowTypes.front().id, hostWall, offset);
    if (!id) { undo_.pop_back(); return 0; }
    modified_ = true;
    selection_ = {SelectionKind::Window, id};
    return id;
}

bool DocumentController::setLevelElevation(LevelId level, double elevation) {
    checkpoint();
    if (!document_.setLevelElevation(level, elevation)) { undo_.pop_back(); return false; }
    modified_ = true;
    return true;
}

bool DocumentController::moveWall(ElementId wall, double x0, double y0, double x1, double y1) {
    checkpoint();
    if (!document_.moveWall(wall, x0, y0, x1, y1)) { undo_.pop_back(); return false; }
    modified_ = true;
    return true;
}

bool DocumentController::setOpeningOffset(SelectionKind kind, ElementId opening, double offset) {
    if (!std::isfinite(offset) || offset < 0.0 || offset > 1.0) return false;
    checkpoint();
    if (kind == SelectionKind::Door) {
        auto it = std::find_if(document_.doors.begin(), document_.doors.end(),
                               [opening](const Door& value) { return value.id == opening; });
        if (it == document_.doors.end()) { undo_.pop_back(); return false; }
        it->offsetAlongWall = offset;
    } else if (kind == SelectionKind::Window) {
        auto it = std::find_if(document_.windows.begin(), document_.windows.end(),
                               [opening](const Window& value) { return value.id == opening; });
        if (it == document_.windows.end()) { undo_.pop_back(); return false; }
        it->offsetAlongWall = offset;
    } else {
        undo_.pop_back();
        return false;
    }
    modified_ = true;
    return true;
}

bool DocumentController::undo() {
    if (undo_.empty()) return false;
    const auto current = document_.serializeNative();
    const auto state = undo_.back();
    undo_.pop_back();
    if (!restore(state)) return false;
    redo_.push_back(current);
    return true;
}

bool DocumentController::redo() {
    if (redo_.empty()) return false;
    const auto current = document_.serializeNative();
    const auto state = redo_.back();
    redo_.pop_back();
    if (!restore(state)) return false;
    undo_.push_back(current);
    return true;
}

} // namespace mydesk::buildings
