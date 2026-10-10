#include "mydesk/commands/registry.hpp"

#include <cmath>
#include <cctype>
#include <string>

namespace mydesk::commands {

std::string normalizeToken(std::string_view token) {
    std::string out;
    out.reserve(token.size());
    for (char c : token) {
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return out;
}

bool parsePointToken(std::string_view token, geometry::Vec2& out) {
    const auto comma = token.find(',');
    if (comma == std::string_view::npos) {
        return false;
    }
    try {
        const double x = std::stod(std::string(token.substr(0, comma)));
        const double y = std::stod(std::string(token.substr(comma + 1)));
        out = {x, y};
        return std::isfinite(x) && std::isfinite(y);
    } catch (...) {
        return false;
    }
}

bool parseNumberToken(std::string_view token, double& out) {
    try {
        std::size_t idx = 0;
        out = std::stod(std::string(token), &idx);
        return idx == token.size() && std::isfinite(out);
    } catch (...) {
        return false;
    }
}

std::vector<std::string> splitCommandLine(std::string_view line) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : line) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    return parts;
}

void CommandRegistry::registerCommand(CommandSpec spec) {
    const std::size_t idx = commands_.size();
    index_[normalizeToken(spec.id)] = idx;
    for (const auto& alias : spec.aliases) {
        index_[normalizeToken(alias)] = idx;
    }
    commands_.push_back(std::move(spec));
}

const CommandSpec* CommandRegistry::find(std::string_view token) const {
    const auto it = index_.find(normalizeToken(token));
    if (it == index_.end()) {
        return nullptr;
    }
    return &commands_[it->second];
}

CommandResult CommandRegistry::dispatch(CommandContext& ctx, std::string_view line) {
    auto parts = splitCommandLine(line);
    if (parts.empty()) {
        if (!ctx.lastCommand().empty()) {
            parts.push_back(ctx.lastCommand());
        } else {
            return {false, "Empty command", std::nullopt};
        }
    }

    const CommandSpec* spec = find(parts.front());
    if (!spec || !spec->handler) {
        return {false, "Unknown command: " + parts.front(), std::nullopt};
    }
    if (spec->status == CommandStatus::NotImplemented) {
        return {false, "Command not implemented: " + spec->id, std::nullopt};
    }

    std::vector<std::string> args(parts.begin() + 1, parts.end());
    auto result = spec->handler(ctx, args);
    if (result.ok) {
        ctx.setLastCommand(spec->id);
    }
    return result;
}

CommandRegistry CommandRegistry::createDefault() {
    CommandRegistry registry;
    registerDraftCommands(registry);
    registerModifyCommands(registry);
    return registry;
}

} // namespace mydesk::commands
