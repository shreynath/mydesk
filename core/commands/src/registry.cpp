#include "mydesk/commands/registry.hpp"

#include <cmath>
#include <cctype>
#include <optional>
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

bool looksLikeCoordinate(std::string_view token) {
    if (token.empty()) {
        return false;
    }
    if (token.front() == '@' || token.front() == '#') {
        return true;
    }
    return token.find(',') != std::string_view::npos || token.find('<') != std::string_view::npos;
}

namespace {

constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;

bool parseFiniteNumber(std::string_view token, double& out) {
    if (token.empty()) {
        return false;
    }
    try {
        std::size_t idx = 0;
        const double value = std::stod(std::string(token), &idx);
        if (idx != token.size() || !std::isfinite(value)) {
            return false;
        }
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

CoordinateParse parseCoordinateToken(std::string_view token,
                                     const std::optional<geometry::Vec2>& lastPoint) {
    CoordinateParse result;
    std::string_view body = token;
    bool relative = false;
    if (!body.empty() && body.front() == '@') {
        relative = true;
        body.remove_prefix(1);
    } else if (!body.empty() && body.front() == '#') {
        body.remove_prefix(1);
    }
    if (body.empty() || (!body.empty() && (body.front() == '@' || body.front() == '#'))) {
        result.error = body.empty() ? "Incomplete coordinate" : "Invalid coordinate prefix";
        return result;
    }

    int commas = 0;
    for (char c : body) {
        if (c == ',') {
            ++commas;
        }
    }
    if (commas > 1) {
        result.error = "3D coordinates are not supported yet";
        return result;
    }

    const auto anglePos = body.find('<');
    if (anglePos != std::string_view::npos) {
        if (body.find(',') != std::string_view::npos) {
            result.error = "Polar coordinate cannot contain a comma";
            return result;
        }
        double distance = 0.0;
        double angleDeg = 0.0;
        if (!parseFiniteNumber(body.substr(0, anglePos), distance) ||
            !parseFiniteNumber(body.substr(anglePos + 1), angleDeg)) {
            result.error = "Polar coordinate expects distance<angle";
            return result;
        }
        const double rad = angleDeg * kDegreesToRadians;
        const geometry::Vec2 delta{distance * std::cos(rad), distance * std::sin(rad)};
        if (relative) {
            if (!lastPoint) {
                result.error = "No previous point for relative coordinate";
                return result;
            }
            result.point = *lastPoint + delta;
        } else {
            result.point = delta;
        }
        result.ok = true;
        return result;
    }

    const auto comma = body.find(',');
    if (comma == std::string_view::npos) {
        result.error = "Coordinate expects x,y or distance<angle";
        return result;
    }
    double x = 0.0;
    double y = 0.0;
    if (!parseFiniteNumber(body.substr(0, comma), x) || !parseFiniteNumber(body.substr(comma + 1), y)) {
        result.error = "Invalid coordinate numbers";
        return result;
    }
    if (relative) {
        if (!lastPoint) {
            result.error = "No previous point for relative coordinate";
            return result;
        }
        result.point = *lastPoint + geometry::Vec2{x, y};
    } else {
        result.point = {x, y};
    }
    result.ok = true;
    return result;
}

CoordinateParse CommandContext::readPoint(std::string_view token) {
    CoordinateParse parsed = parseCoordinateToken(token, lastPoint_);
    if (parsed.ok) {
        lastPoint_ = parsed.point;
    }
    return parsed;
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
