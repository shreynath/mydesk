#include "mydesk/commands/registry.hpp"

#include <cmath>
#include <sstream>

namespace mydesk::commands {

namespace {

CommandResult okMsg(std::string message, std::optional<document::EntityId> id = std::nullopt) {
    return {true, std::move(message), id};
}

CommandResult errMsg(std::string message) { return {false, std::move(message), std::nullopt}; }

CommandResult handleUndo(CommandContext& ctx, const std::vector<std::string>&) {
    if (!ctx.document().undo()) {
        return errMsg("Nothing to undo");
    }
    return okMsg("Undo");
}

CommandResult handleRedo(CommandContext& ctx, const std::vector<std::string>&) {
    if (!ctx.document().redo()) {
        return errMsg("Nothing to redo");
    }
    return okMsg("Redo");
}

CommandResult handleNew(CommandContext& ctx, const std::vector<std::string>&) {
    ctx.document() = document::Document{};
    ctx.clearInputs();
    ctx.setPrompt("");
    return okMsg("Started new drawing");
}

CommandResult handleLine(CommandContext& ctx, const std::vector<std::string>& args) {
    ctx.clearInputs();
    for (const auto& arg : args) {
        geometry::Vec2 p;
        if (!parsePointToken(arg, p)) {
            return errMsg("LINE expects points as x,y");
        }
        ctx.pushPoint(p);
    }

    if (ctx.points().size() >= 2) {
        const auto& pts = ctx.points();
        const auto id = ctx.document().addLine(pts[0], pts[1]);
        ctx.clearInputs();
        ctx.setPrompt("");
        return okMsg("LINE created #" + std::to_string(id), id);
    }
    if (ctx.points().size() == 1) {
        ctx.setPrompt("Specify next point:");
        return okMsg("LINE: need end point");
    }
    ctx.setPrompt("Specify first point:");
    return okMsg("LINE: need start point");
}

CommandResult handleCircle(CommandContext& ctx, const std::vector<std::string>& args) {
    ctx.clearInputs();
    for (const auto& arg : args) {
        geometry::Vec2 p;
        double n = 0.0;
        if (parsePointToken(arg, p)) {
            ctx.pushPoint(p);
        } else if (parseNumberToken(arg, n)) {
            ctx.pushNumber(n);
        } else {
            return errMsg("CIRCLE expects center x,y and radius");
        }
    }

    if (!ctx.points().empty() && !ctx.numbers().empty()) {
        try {
            const auto id = ctx.document().addCircle(ctx.points().front(), ctx.numbers().front());
            ctx.clearInputs();
            ctx.setPrompt("");
            return okMsg("CIRCLE created #" + std::to_string(id), id);
        } catch (const std::exception& ex) {
            return errMsg(ex.what());
        }
    }
    if (ctx.points().size() >= 2) {
        const double radius = ctx.points()[0].distanceTo(ctx.points()[1]);
        try {
            const auto id = ctx.document().addCircle(ctx.points()[0], radius);
            ctx.clearInputs();
            ctx.setPrompt("");
            return okMsg("CIRCLE created #" + std::to_string(id), id);
        } catch (const std::exception& ex) {
            return errMsg(ex.what());
        }
    }
    if (ctx.points().size() == 1) {
        ctx.setPrompt("Specify radius or second point:");
        return okMsg("CIRCLE: need radius");
    }
    ctx.setPrompt("Specify center point:");
    return okMsg("CIRCLE: need center");
}

CommandResult handleRectangle(CommandContext& ctx, const std::vector<std::string>& args) {
    ctx.clearInputs();
    for (const auto& arg : args) {
        geometry::Vec2 p;
        if (!parsePointToken(arg, p)) {
            return errMsg("RECTANGLE expects corner points as x,y");
        }
        ctx.pushPoint(p);
    }
    if (ctx.points().size() >= 2) {
        try {
            const auto id = ctx.document().addRectangle(ctx.points()[0], ctx.points()[1]);
            ctx.clearInputs();
            ctx.setPrompt("");
            return okMsg("RECTANGLE created #" + std::to_string(id) + " (closed PLINE)", id);
        } catch (const std::exception& ex) {
            return errMsg(ex.what());
        }
    }
    if (ctx.points().size() == 1) {
        ctx.setPrompt("Specify other corner:");
        return okMsg("RECTANGLE: need opposite corner");
    }
    ctx.setPrompt("Specify first corner:");
    return okMsg("RECTANGLE: need first corner");
}

CommandResult handleErase(CommandContext& ctx, const std::vector<std::string>&) {
    const auto selected = ctx.document().selectionOrdered();
    if (selected.empty()) {
        return errMsg("No selection");
    }
    std::size_t count = 0;
    for (const auto id : selected) {
        if (ctx.document().removeEntity(id)) {
            ++count;
        }
    }
    ctx.document().clearSelection();
    return okMsg("Erased " + std::to_string(count) + " entity(ies)");
}

CommandResult handleMove(CommandContext& ctx, const std::vector<std::string>& args) {
    const auto selected = ctx.document().selectionOrdered();
    if (selected.empty()) {
        return errMsg("MOVE: select objects first");
    }
    ctx.clearInputs();
    for (const auto& arg : args) {
        geometry::Vec2 p;
        if (!parsePointToken(arg, p)) {
            return errMsg("MOVE expects points as x,y");
        }
        ctx.pushPoint(p);
    }
    if (ctx.points().size() >= 2) {
        const geometry::Vec2 delta = ctx.points()[1] - ctx.points()[0];
        if (!ctx.document().moveEntities(selected, delta)) {
            return errMsg("MOVE failed");
        }
        ctx.clearInputs();
        ctx.setPrompt("");
        return okMsg("MOVE " + std::to_string(selected.size()) + " entity(ies)");
    }
    if (ctx.points().size() == 1) {
        ctx.setPrompt("Specify second point or displacement:");
        return okMsg("MOVE: need second point");
    }
    ctx.setPrompt("Specify base point:");
    return okMsg("MOVE: need base point");
}

CommandResult handleCopy(CommandContext& ctx, const std::vector<std::string>& args) {
    const auto selected = ctx.document().selectionOrdered();
    if (selected.empty()) {
        return errMsg("COPY: select objects first");
    }
    ctx.clearInputs();
    for (const auto& arg : args) {
        geometry::Vec2 p;
        if (!parsePointToken(arg, p)) {
            return errMsg("COPY expects points as x,y");
        }
        ctx.pushPoint(p);
    }
    if (ctx.points().size() >= 2) {
        const geometry::Vec2 delta = ctx.points()[1] - ctx.points()[0];
        const auto created = ctx.document().copyEntities(selected, delta);
        if (created.empty()) {
            return errMsg("COPY failed");
        }
        ctx.clearInputs();
        ctx.setPrompt("");
        return okMsg("COPY created " + std::to_string(created.size()) + " entity(ies)");
    }
    if (ctx.points().size() == 1) {
        ctx.setPrompt("Specify second point or displacement:");
        return okMsg("COPY: need second point");
    }
    ctx.setPrompt("Specify base point:");
    return okMsg("COPY: need base point");
}

CommandResult handleRotate(CommandContext& ctx, const std::vector<std::string>& args) {
    const auto selected = ctx.document().selectionOrdered();
    if (selected.empty()) {
        return errMsg("ROTATE: select objects first");
    }
    ctx.clearInputs();
    for (const auto& arg : args) {
        geometry::Vec2 p;
        double n = 0.0;
        if (parsePointToken(arg, p)) {
            ctx.pushPoint(p);
        } else if (parseNumberToken(arg, n)) {
            ctx.pushNumber(n);
        } else {
            return errMsg("ROTATE expects base point and angle (degrees)");
        }
    }

    if (!ctx.points().empty() && !ctx.numbers().empty()) {
        if (!ctx.document().rotateEntities(selected, ctx.points().front(), ctx.numbers().front())) {
            return errMsg("ROTATE failed");
        }
        ctx.clearInputs();
        ctx.setPrompt("");
        return okMsg("ROTATE " + std::to_string(selected.size()) + " entity(ies)");
    }
    if (ctx.points().size() >= 2) {
        const auto& base = ctx.points()[0];
        const auto& tip = ctx.points()[1];
        const double degrees = std::atan2(tip.y - base.y, tip.x - base.x) * (180.0 / 3.14159265358979323846);
        if (!ctx.document().rotateEntities(selected, base, degrees)) {
            return errMsg("ROTATE failed");
        }
        ctx.clearInputs();
        ctx.setPrompt("");
        return okMsg("ROTATE " + std::to_string(selected.size()) + " entity(ies)");
    }
    if (ctx.points().size() == 1) {
        ctx.setPrompt("Specify rotation angle or second point:");
        return okMsg("ROTATE: need angle");
    }
    ctx.setPrompt("Specify base point:");
    return okMsg("ROTATE: need base point");
}

CommandResult handleScale(CommandContext& ctx, const std::vector<std::string>& args) {
    const auto selected = ctx.document().selectionOrdered();
    if (selected.empty()) {
        return errMsg("SCALE: select objects first");
    }
    ctx.clearInputs();
    for (const auto& arg : args) {
        geometry::Vec2 p;
        double n = 0.0;
        if (parsePointToken(arg, p)) {
            ctx.pushPoint(p);
        } else if (parseNumberToken(arg, n)) {
            ctx.pushNumber(n);
        } else {
            return errMsg("SCALE expects base point and scale factor");
        }
    }
    if (!ctx.points().empty() && !ctx.numbers().empty()) {
        if (!ctx.document().scaleEntities(selected, ctx.points().front(), ctx.numbers().front())) {
            return errMsg("SCALE failed");
        }
        ctx.clearInputs();
        ctx.setPrompt("");
        return okMsg("SCALE " + std::to_string(selected.size()) + " entity(ies)");
    }
    if (ctx.points().size() == 1) {
        ctx.setPrompt("Specify scale factor:");
        return okMsg("SCALE: need factor");
    }
    ctx.setPrompt("Specify base point:");
    return okMsg("SCALE: need base point");
}

CommandResult handleNotImplemented(CommandContext&, const std::vector<std::string>&) {
    return errMsg("Command registered but not yet implemented — tracked in docs/COMPATIBILITY_MATRIX.md");
}

} // namespace

void registerDraftCommands(CommandRegistry& registry) {
    registry.registerCommand(CommandSpec{
        .id = "LINE",
        .label = "Line",
        .aliases = {"L"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Two-point line (AutoCAD-class LINE)",
        .handler = handleLine,
    });
    registry.registerCommand(CommandSpec{
        .id = "CIRCLE",
        .label = "Circle",
        .aliases = {"C"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Center + radius or center + point",
        .handler = handleCircle,
    });
    registry.registerCommand(CommandSpec{
        .id = "RECTANGLE",
        .label = "Rectangle",
        .aliases = {"REC", "RECTANG"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Two-corner closed polyline (true PLINE)",
        .handler = handleRectangle,
    });
    registry.registerCommand(CommandSpec{
        .id = "MOVE",
        .label = "Move",
        .aliases = {"M"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Move current selection by two-point displacement",
        .handler = handleMove,
    });
    registry.registerCommand(CommandSpec{
        .id = "COPY",
        .label = "Copy",
        .aliases = {"CO", "CP"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Copy current selection by two-point displacement",
        .handler = handleCopy,
    });
    registry.registerCommand(CommandSpec{
        .id = "ROTATE",
        .label = "Rotate",
        .aliases = {"RO"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Rotate selection about base point by degrees or reference point",
        .handler = handleRotate,
    });
    registry.registerCommand(CommandSpec{
        .id = "SCALE",
        .label = "Scale",
        .aliases = {"SC"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Scale selection about base point by factor",
        .handler = handleScale,
    });
    registry.registerCommand(CommandSpec{
        .id = "UNDO",
        .label = "Undo",
        .aliases = {"U"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Undo last transaction",
        .handler = handleUndo,
    });
    registry.registerCommand(CommandSpec{
        .id = "REDO",
        .label = "Redo",
        .aliases = {},
        .status = CommandStatus::ImplementedTested,
        .notes = "Redo last undone transaction",
        .handler = handleRedo,
    });
    registry.registerCommand(CommandSpec{
        .id = "NEW",
        .label = "New",
        .aliases = {},
        .status = CommandStatus::ImplementedTested,
        .notes = "Replace session document with empty drawing",
        .handler = handleNew,
    });
    registry.registerCommand(CommandSpec{
        .id = "ERASE",
        .label = "Erase",
        .aliases = {"E"},
        .status = CommandStatus::Partial,
        .notes = "Deletes current selection only (no window/crossing yet)",
        .handler = handleErase,
    });

    const std::vector<std::pair<std::string, std::vector<std::string>>> planned = {
        {"LAYER", {"LA"}}, {"BLOCK", {"B"}},       {"INSERT", {"I"}}, {"HATCH", {"H", "BH"}},
        {"DIMENSION", {"DIM"}}, {"ZOOM", {"Z"}},   {"ARC", {"A"}},    {"MIRROR", {"MI"}},
        {"STRETCH", {"S"}}, {"EXPLODE", {"X"}},    {"XLINE", {"XL"}}, {"OSNAP", {"OS"}},
        {"PAN", {"P"}},
    };
    for (const auto& [id, aliases] : planned) {
        registry.registerCommand(CommandSpec{
            .id = id,
            .label = id,
            .aliases = aliases,
            .status = CommandStatus::NotImplemented,
            .notes = "Tracked for AutoCAD-class Draft parity",
            .handler = handleNotImplemented,
        });
    }
}

} // namespace mydesk::commands
