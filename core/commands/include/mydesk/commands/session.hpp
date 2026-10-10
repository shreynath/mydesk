#pragma once

#include "mydesk/commands/draft_modes.hpp"
#include "mydesk/commands/registry.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace mydesk::commands {

// Session owns the active document + command interaction state for UI and tests.
class Session {
public:
    Session();

    [[nodiscard]] document::Document& document() { return document_; }
    [[nodiscard]] const document::Document& document() const { return document_; }
    [[nodiscard]] CommandRegistry& registry() { return registry_; }
    [[nodiscard]] CommandContext& context() { return context_; }
    [[nodiscard]] DraftModes& modes() { return modes_; }
    [[nodiscard]] const DraftModes& modes() const { return modes_; }

    CommandResult executeLine(std::string_view line);

    // Interactive point/number feed for in-progress commands.
    CommandResult providePoint(geometry::Vec2 point);
    CommandResult provideNumber(double value);
    CommandResult cancel();

    [[nodiscard]] bool hasActiveCommand() const { return !activeCommand_.empty(); }
    [[nodiscard]] const std::string& activeCommand() const { return activeCommand_; }
    [[nodiscard]] const std::string& statusMessage() const { return statusMessage_; }

    void setFilePath(std::optional<std::filesystem::path> path) { filePath_ = std::move(path); }
    [[nodiscard]] const std::optional<std::filesystem::path>& filePath() const { return filePath_; }

    // Apply ortho constraint relative to the last committed point of the active command.
    [[nodiscard]] geometry::Vec2 applyOrtho(geometry::Vec2 point) const;

private:
    void syncActiveFromResult(const CommandSpec* spec, const CommandResult& result);

    document::Document document_;
    CommandRegistry registry_;
    CommandContext context_;
    DraftModes modes_{};
    std::string activeCommand_;
    std::string statusMessage_{"Ready"};
    std::optional<std::filesystem::path> filePath_;
};

} // namespace mydesk::commands
