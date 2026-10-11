#pragma once

#include "mydesk/buildings/project_document.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mydesk::buildings {

enum class SelectionKind { None, Level, Wall, Door, Window, Room };

struct Selection {
    SelectionKind kind{SelectionKind::None};
    std::uint64_t id{0};
};

class DocumentController {
public:
    DocumentController();

    [[nodiscard]] const ProjectDocument& document() const { return document_; }
    [[nodiscard]] ProjectDocument& document() { return document_; }
    [[nodiscard]] Selection selection() const { return selection_; }
    [[nodiscard]] const std::filesystem::path& filePath() const { return filePath_; }
    [[nodiscard]] bool isModified() const { return modified_; }
    [[nodiscard]] bool canUndo() const { return !undo_.empty(); }
    [[nodiscard]] bool canRedo() const { return !redo_.empty(); }

    void newProject(std::string name = "Untitled Building");
    bool open(const std::filesystem::path& path, std::string* error = nullptr);
    bool save(std::string* error = nullptr);
    bool saveAs(const std::filesystem::path& path, std::string* error = nullptr);
    bool exportIfc(const std::filesystem::path& path, std::string* error = nullptr) const;

    void select(Selection selection) { selection_ = selection; }
    LevelId createLevel(std::string name, double elevation);
    ElementId createWall(LevelId level, double x0, double y0, double x1, double y1);
    ElementId createDoor(ElementId hostWall, double offset);
    ElementId createWindow(ElementId hostWall, double offset);
    bool setLevelElevation(LevelId level, double elevation);
    bool moveWall(ElementId wall, double x0, double y0, double x1, double y1);
    bool setOpeningOffset(SelectionKind kind, ElementId opening, double offset);
    bool undo();
    bool redo();

private:
    ProjectDocument document_;
    Selection selection_;
    std::filesystem::path filePath_;
    bool modified_{false};
    std::vector<std::string> undo_;
    std::vector<std::string> redo_;

    void ensureDefaults();
    void checkpoint();
    bool restore(const std::string& state);
};

} // namespace mydesk::buildings
