#include "mydesk/commands/session.hpp"

#include <cmath>

namespace mydesk::commands {

Session::Session()
    : document_(), registry_(CommandRegistry::createDefault()), context_(document_) {
    context_.bindModes(&modes_);
}

geometry::Vec2 Session::applyOrtho(geometry::Vec2 point) const {
    if (!modes_.ortho || context_.points().empty()) {
        return point;
    }
    const auto& base = context_.points().back();
    const double dx = std::abs(point.x - base.x);
    const double dy = std::abs(point.y - base.y);
    if (dx >= dy) {
        return {point.x, base.y};
    }
    return {base.x, point.y};
}

void Session::syncActiveFromResult(const CommandSpec* spec, const CommandResult& result) {
    if (result.ok && !context_.prompt().empty() && spec) {
        activeCommand_ = spec->id;
    } else if (result.ok) {
        activeCommand_.clear();
    }
}

CommandResult Session::executeLine(std::string_view line) {
    auto parts = splitCommandLine(line);
    if (parts.empty()) {
        if (hasActiveCommand() && activeCommand_ == "PLINE") {
            std::vector<std::string> args;
            for (const auto& p : context_.points()) {
                args.push_back(std::to_string(p.x) + "," + std::to_string(p.y));
            }
            args.emplace_back("END");
            const CommandSpec* spec = registry_.find("PLINE");
            if (!spec || !spec->handler) {
                return cancel();
            }
            auto result = spec->handler(context_, args);
            statusMessage_ = result.message;
            if (result.ok && context_.prompt().empty()) {
                activeCommand_.clear();
                context_.setLastCommand("PLINE");
            }
            return result;
        }
        if (hasActiveCommand()) {
            statusMessage_ = "Command still active: " + activeCommand_;
            return {false, statusMessage_, std::nullopt};
        }
        auto result = registry_.dispatch(context_, line);
        statusMessage_ = result.message;
        return result;
    }

    const auto token = normalizeToken(parts.front());
    if (token == "ESC" || token == "CANCEL") {
        return cancel();
    }

    if (hasActiveCommand()) {
        geometry::Vec2 p;
        double n = 0.0;
        if (parts.size() == 1 && parsePointToken(parts[0], p)) {
            return providePoint(p);
        }
        if (parts.size() == 1 && parseNumberToken(parts[0], n)) {
            return provideNumber(n);
        }
        if (registry_.find(parts.front()) != nullptr) {
            cancel();
        } else {
            statusMessage_ = "Provide point (x,y), number, or ESC";
            return {false, statusMessage_, std::nullopt};
        }
    }

    auto result = registry_.dispatch(context_, line);
    statusMessage_ = result.message;
    const CommandSpec* spec = registry_.find(parts.front());
    if (!spec && !context_.lastCommand().empty()) {
        spec = registry_.find(context_.lastCommand());
    }
    syncActiveFromResult(spec, result);
    return result;
}

CommandResult Session::providePoint(geometry::Vec2 point) {
    if (!hasActiveCommand()) {
        statusMessage_ = "No active command";
        return {false, statusMessage_, std::nullopt};
    }

    point = applyOrtho(point);
    context_.pushPoint(point);
    std::vector<std::string> args;
    for (const auto& p : context_.points()) {
        args.push_back(std::to_string(p.x) + "," + std::to_string(p.y));
    }
    for (double n : context_.numbers()) {
        args.push_back(std::to_string(n));
    }

    const CommandSpec* spec = registry_.find(activeCommand_);
    if (!spec || !spec->handler) {
        return cancel();
    }
    auto result = spec->handler(context_, args);
    statusMessage_ = result.message;
    if (result.ok && context_.prompt().empty()) {
        activeCommand_.clear();
        context_.setLastCommand(spec->id);
    }
    return result;
}

CommandResult Session::provideNumber(double value) {
    if (!hasActiveCommand()) {
        statusMessage_ = "No active command";
        return {false, statusMessage_, std::nullopt};
    }
    context_.pushNumber(value);
    std::vector<std::string> args;
    for (const auto& p : context_.points()) {
        args.push_back(std::to_string(p.x) + "," + std::to_string(p.y));
    }
    for (double n : context_.numbers()) {
        args.push_back(std::to_string(n));
    }
    const CommandSpec* spec = registry_.find(activeCommand_);
    if (!spec || !spec->handler) {
        return cancel();
    }
    auto result = spec->handler(context_, args);
    statusMessage_ = result.message;
    if (result.ok && context_.prompt().empty()) {
        activeCommand_.clear();
        context_.setLastCommand(spec->id);
    }
    return result;
}

CommandResult Session::cancel() {
    activeCommand_.clear();
    context_.clearInputs();
    context_.setPrompt("");
    statusMessage_ = "Command cancelled";
    return {true, statusMessage_, std::nullopt};
}

} // namespace mydesk::commands
