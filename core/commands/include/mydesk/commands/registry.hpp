#pragma once

#include "mydesk/commands/draft_modes.hpp"
#include "mydesk/document/document.hpp"
#include "mydesk/geometry/vec2.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mydesk::commands {

enum class CommandStatus {
    ImplementedTested,
    Partial,
    NotImplemented,
    NotApplicable,
};

struct CommandResult {
    bool ok{false};
    std::string message;
    std::optional<document::EntityId> entityId;
};

struct PromptRequest {
    std::string text;
    bool expectsPoint{false};
    bool expectsDistance{false};
};

struct CoordinateParse {
    bool ok{false};
    geometry::Vec2 point{};
    std::string error;
};

class CommandContext {
public:
    explicit CommandContext(document::Document& doc) : document_(doc) {}

    [[nodiscard]] document::Document& document() { return document_; }
    [[nodiscard]] const document::Document& document() const { return document_; }

    void bindModes(DraftModes* modes) { modes_ = modes; }
    [[nodiscard]] DraftModes* modes() { return modes_; }
    [[nodiscard]] const DraftModes* modes() const { return modes_; }

    void setPrompt(std::string prompt) { prompt_ = std::move(prompt); }
    [[nodiscard]] const std::string& prompt() const { return prompt_; }

    void pushPoint(geometry::Vec2 p) { points_.push_back(p); }
    void pushNumber(double v) { numbers_.push_back(v); }
    void clearInputs() {
        points_.clear();
        numbers_.clear();
    }

    [[nodiscard]] const std::vector<geometry::Vec2>& points() const { return points_; }
    [[nodiscard]] const std::vector<double>& numbers() const { return numbers_; }

    void setLastCommand(std::string name) { lastCommand_ = std::move(name); }
    [[nodiscard]] const std::string& lastCommand() const { return lastCommand_; }

    void setScratch(std::string value) { scratch_ = std::move(value); }
    [[nodiscard]] const std::string& scratch() const { return scratch_; }

    [[nodiscard]] const std::optional<geometry::Vec2>& lastPoint() const { return lastPoint_; }
    void setLastPoint(geometry::Vec2 point) { lastPoint_ = point; }
    void clearLastPoint() { lastPoint_.reset(); }

    // Resolves x,y / @dx,dy / dist<angle / @dist<angle / #x,y and stores LASTPOINT on success.
    [[nodiscard]] CoordinateParse readPoint(std::string_view token);

private:
    document::Document& document_;
    DraftModes* modes_{nullptr};
    std::string prompt_;
    std::vector<geometry::Vec2> points_;
    std::vector<double> numbers_;
    std::string lastCommand_;
    std::string scratch_;
    std::optional<geometry::Vec2> lastPoint_;
};

using CommandHandler = std::function<CommandResult(CommandContext&, const std::vector<std::string>&)>;

struct CommandSpec {
    std::string id;
    std::string label;
    std::vector<std::string> aliases;
    CommandStatus status{CommandStatus::NotImplemented};
    std::string notes;
    CommandHandler handler;
};

class CommandRegistry {
public:
    void registerCommand(CommandSpec spec);
    [[nodiscard]] const CommandSpec* find(std::string_view token) const;
    [[nodiscard]] const std::vector<CommandSpec>& all() const { return commands_; }

    CommandResult dispatch(CommandContext& ctx, std::string_view line);

    static CommandRegistry createDefault();

private:
    std::vector<CommandSpec> commands_;
    std::unordered_map<std::string, std::size_t> index_;
};

[[nodiscard]] std::string normalizeToken(std::string_view token);
[[nodiscard]] bool parsePointToken(std::string_view token, geometry::Vec2& out);
[[nodiscard]] bool parseNumberToken(std::string_view token, double& out);
[[nodiscard]] bool looksLikeCoordinate(std::string_view token);
[[nodiscard]] CoordinateParse parseCoordinateToken(std::string_view token,
                                                   const std::optional<geometry::Vec2>& lastPoint);
[[nodiscard]] std::vector<std::string> splitCommandLine(std::string_view line);

inline bool readUserPoint(CommandContext& ctx, std::string_view token, geometry::Vec2& out,
                          std::string& error) {
    const CoordinateParse parsed = ctx.readPoint(token);
    if (!parsed.ok) {
        error = parsed.error.empty() ? "Invalid coordinate" : parsed.error;
        return false;
    }
    out = parsed.point;
    return true;
}

inline bool pushCoordinate(CommandContext& ctx, std::string_view token, std::string& error) {
    geometry::Vec2 point;
    if (!readUserPoint(ctx, token, point, error)) {
        return false;
    }
    ctx.pushPoint(point);
    return true;
}

inline bool pushCoordinateOrNumber(CommandContext& ctx, std::string_view token, std::string& error) {
    if (looksLikeCoordinate(token)) {
        return pushCoordinate(ctx, token, error);
    }
    double number = 0.0;
    if (parseNumberToken(token, number)) {
        ctx.pushNumber(number);
        return true;
    }
    error = "expected a coordinate (x,y, @dx,dy, dist<angle, @dist<angle) or a number";
    return false;
}

void registerDraftCommands(CommandRegistry& registry);
void registerModifyCommands(CommandRegistry& registry);

} // namespace mydesk::commands
