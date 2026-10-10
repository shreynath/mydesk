#include "mydesk/commands/registry.hpp"
#include "mydesk/modify/modify_ops.hpp"

#include <sstream>

namespace mydesk::commands {

namespace {

CommandResult okMsg(std::string message) { return {true, std::move(message), std::nullopt}; }
CommandResult errMsg(std::string message) { return {false, std::move(message), std::nullopt}; }

CommandResult handlePline(CommandContext& ctx, const std::vector<std::string>& args) {
    ctx.clearInputs();
    bool close = false;
    bool finish = false;
    for (const auto& arg : args) {
        const auto tok = normalizeToken(arg);
        if (tok == "C" || tok == "CLOSE") {
            close = true;
            continue;
        }
        if (tok == "END" || tok == "DONE") {
            finish = true;
            continue;
        }
        geometry::Vec2 p;
        if (!parsePointToken(arg, p)) {
            return errMsg("PLINE expects points as x,y (C=close, END=finish)");
        }
        ctx.pushPoint(p);
    }

    if (close) {
        if (ctx.points().size() < 2) {
            return errMsg("PLINE: need at least two points to close");
        }
        try {
            const auto id = ctx.document().addPolyline(ctx.points(), true, "PLINE");
            ctx.clearInputs();
            ctx.setPrompt("");
            return {true, "PLINE closed #" + std::to_string(id), id};
        } catch (const std::exception& ex) {
            return errMsg(ex.what());
        }
    }

    if (finish) {
        if (ctx.points().size() < 2) {
            return errMsg("PLINE: need at least two points");
        }
        try {
            const auto id = ctx.document().addPolyline(ctx.points(), false, "PLINE");
            ctx.clearInputs();
            ctx.setPrompt("");
            return {true, "PLINE created #" + std::to_string(id), id};
        } catch (const std::exception& ex) {
            return errMsg(ex.what());
        }
    }

    if (ctx.points().empty()) {
        ctx.setPrompt("Specify start point:");
        return okMsg("PLINE: need start point");
    }
    ctx.setPrompt("Specify next point (C=close, Enter/END=finish):");
    return okMsg("PLINE: " + std::to_string(ctx.points().size()) + " vertex(ices)");
}

CommandResult handleTrim(CommandContext& ctx, const std::vector<std::string>& args) {
    geometry::Vec2 pick;
    bool havePick = false;
    for (const auto& arg : args) {
        if (parsePointToken(arg, pick)) {
            havePick = true;
        }
    }
    if (!havePick) {
        ctx.setPrompt("Select object to trim:");
        return okMsg("TRIM: pick object");
    }
    auto cutters = ctx.document().selectionOrdered();
    // Don't use target as only cutter set exclusively — empty means all.
    auto result = modify::trimAtPoint(ctx.document(), pick, cutters);
    ctx.setPrompt(result.ok ? "Select object to trim:" : "");
    return {result.ok, result.message, std::nullopt};
}

CommandResult handleExtend(CommandContext& ctx, const std::vector<std::string>& args) {
    geometry::Vec2 pick;
    bool havePick = false;
    for (const auto& arg : args) {
        if (parsePointToken(arg, pick)) {
            havePick = true;
        }
    }
    if (!havePick) {
        ctx.setPrompt("Select object to extend:");
        return okMsg("EXTEND: pick object");
    }
    auto boundaries = ctx.document().selectionOrdered();
    auto result = modify::extendAtPoint(ctx.document(), pick, boundaries);
    ctx.setPrompt(result.ok ? "Select object to extend:" : "");
    return {result.ok, result.message, std::nullopt};
}

CommandResult handleOffset(CommandContext& ctx, const std::vector<std::string>& args) {
    ctx.clearInputs();
    for (const auto& arg : args) {
        geometry::Vec2 p;
        double n = 0.0;
        if (parsePointToken(arg, p)) {
            ctx.pushPoint(p);
        } else if (parseNumberToken(arg, n)) {
            ctx.pushNumber(n);
        } else {
            return errMsg("OFFSET expects distance, then side point (select object first)");
        }
    }
    const auto selected = ctx.document().selectionOrdered();
    if (selected.empty()) {
        return errMsg("OFFSET: select object first");
    }
    if (ctx.numbers().empty()) {
        ctx.setPrompt("Specify offset distance:");
        return okMsg("OFFSET: need distance");
    }
    if (ctx.points().empty()) {
        ctx.setPrompt("Specify point on side to offset:");
        return okMsg("OFFSET: need side point");
    }
    auto result = modify::offsetEntity(ctx.document(), selected.front(), ctx.numbers().front(),
                                       ctx.points().front());
    ctx.clearInputs();
    ctx.setPrompt("");
    return {result.ok, result.message, std::nullopt};
}

CommandResult handleFillet(CommandContext& ctx, const std::vector<std::string>& args) {
    double radius = 0.0;
    bool haveRadius = false;
    for (const auto& arg : args) {
        double n = 0.0;
        if (parseNumberToken(arg, n)) {
            radius = n;
            haveRadius = true;
        }
    }
    auto selected = ctx.document().selectionOrdered();
    if (selected.size() < 2) {
        ctx.setPrompt(haveRadius ? "Select two lines (then re-run FILLET):"
                                 : "Enter radius or select two lines:");
        if (!haveRadius) {
            return okMsg("FILLET: provide radius (optional) and select two LINE entities");
        }
        return errMsg("FILLET: select exactly two LINE entities");
    }
    if (!haveRadius) {
        radius = 0.0;
    }
    auto result = modify::filletLines(ctx.document(), selected[0], selected[1], radius);
    ctx.setPrompt("");
    return {result.ok, result.message, std::nullopt};
}

CommandResult handleChamfer(CommandContext& ctx, const std::vector<std::string>& args) {
    std::vector<double> dists;
    for (const auto& arg : args) {
        double n = 0.0;
        if (parseNumberToken(arg, n)) {
            dists.push_back(n);
        }
    }
    double d1 = dists.empty() ? 0.0 : dists[0];
    double d2 = dists.size() >= 2 ? dists[1] : d1;
    auto selected = ctx.document().selectionOrdered();
    if (selected.size() < 2) {
        return errMsg("CHAMFER: select two LINE entities (optional distances as args)");
    }
    auto result = modify::chamferLines(ctx.document(), selected[0], selected[1], d1, d2);
    return {result.ok, result.message, std::nullopt};
}

} // namespace

void registerModifyCommands(CommandRegistry& registry) {
    registry.registerCommand(CommandSpec{
        .id = "PLINE",
        .label = "Polyline",
        .aliases = {"PL"},
        .status = CommandStatus::ImplementedTested,
        .notes = "True polyline entity; C closes, END finishes open",
        .handler = handlePline,
    });
    registry.registerCommand(CommandSpec{
        .id = "TRIM",
        .label = "Trim",
        .aliases = {"TR"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Pick-trim against selection cutters or all linear entities",
        .handler = handleTrim,
    });
    registry.registerCommand(CommandSpec{
        .id = "EXTEND",
        .label = "Extend",
        .aliases = {"EX"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Extend LINE to boundary (selection or all)",
        .handler = handleExtend,
    });
    registry.registerCommand(CommandSpec{
        .id = "OFFSET",
        .label = "Offset",
        .aliases = {"O"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Offset LINE/CIRCLE/PLINE by distance toward side point",
        .handler = handleOffset,
    });
    registry.registerCommand(CommandSpec{
        .id = "FILLET",
        .label = "Fillet",
        .aliases = {"F"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Fillet two LINEs with radius (creates ARC)",
        .handler = handleFillet,
    });
    registry.registerCommand(CommandSpec{
        .id = "CHAMFER",
        .label = "Chamfer",
        .aliases = {"CHA"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Chamfer two LINEs with distances",
        .handler = handleChamfer,
    });
}

} // namespace mydesk::commands
