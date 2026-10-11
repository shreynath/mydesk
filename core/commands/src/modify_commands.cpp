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
        std::string error;
        if (!pushCoordinate(ctx, arg, error)) {
            return errMsg("PLINE: " + error);
        }
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
        if (looksLikeCoordinate(arg)) {
            std::string error;
            if (!readUserPoint(ctx, arg, pick, error)) {
                return errMsg("TRIM: " + error);
            }
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
        if (looksLikeCoordinate(arg)) {
            std::string error;
            if (!readUserPoint(ctx, arg, pick, error)) {
                return errMsg("EXTEND: " + error);
            }
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
        std::string error;
        if (!pushCoordinateOrNumber(ctx, arg, error)) {
            return errMsg("OFFSET: " + error);
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

CommandResult handleMirror(CommandContext& ctx, const std::vector<std::string>& args) {
    ctx.clearInputs();
    bool eraseSource = false;
    for (const auto& arg : args) {
        const auto tok = normalizeToken(arg);
        if (tok == "Y" || tok == "YES") {
            eraseSource = true;
            continue;
        }
        if (tok == "N" || tok == "NO") {
            eraseSource = false;
            continue;
        }
        std::string err;
        if (!pushCoordinate(ctx, arg, err)) {
            return errMsg("MIRROR: " + err);
        }
    }
    auto selected = ctx.document().selectionOrdered();
    if (selected.empty()) {
        for (const auto& ent : ctx.document().entities()) {
            if (ctx.document().isEntitySelectable(ent)) {
                selected.push_back(ent.id);
            }
        }
    }
    if (selected.empty()) {
        return errMsg("MIRROR: no objects selected");
    }
    if (ctx.points().size() < 2) {
        ctx.setPrompt(ctx.points().empty() ? "Specify first point of mirror line:"
                                           : "Specify second point of mirror line:");
        return okMsg("MIRROR: need 2 points for mirror line");
    }
    auto result = modify::mirrorEntities(ctx.document(), selected, ctx.points()[0],
                                         ctx.points()[1], eraseSource);
    ctx.clearInputs();
    ctx.setPrompt("");
    return {result.ok, result.message, std::nullopt};
}

CommandResult handleArray(CommandContext& ctx, const std::vector<std::string>& args) {
    bool isPolar = false;
    std::vector<double> numbers;
    std::vector<geometry::Vec2> points;
    for (const auto& arg : args) {
        const auto tok = normalizeToken(arg);
        if (tok == "P" || tok == "POLAR") {
            isPolar = true;
            continue;
        }
        if (tok == "R" || tok == "RECT" || tok == "RECTANGULAR") {
            isPolar = false;
            continue;
        }
        if (looksLikeCoordinate(arg)) {
            geometry::Vec2 pt;
            std::string err;
            if (readUserPoint(ctx, arg, pt, err)) {
                points.push_back(pt);
            }
        } else {
            double n = 0.0;
            if (parseNumberToken(arg, n)) {
                numbers.push_back(n);
            }
        }
    }
    auto selected = ctx.document().selectionOrdered();
    if (selected.empty()) {
        for (const auto& ent : ctx.document().entities()) {
            if (ctx.document().isEntitySelectable(ent)) {
                selected.push_back(ent.id);
            }
        }
    }
    if (selected.empty()) {
        return errMsg("ARRAY: no objects selected");
    }

    if (isPolar) {
        geometry::Vec2 center = points.empty() ? geometry::Vec2{0, 0} : points.front();
        int count = numbers.empty() ? 4 : static_cast<int>(numbers[0]);
        double angle = numbers.size() >= 2 ? numbers[1] : 360.0;
        auto res = modify::arrayPolar(ctx.document(), selected, center, count, angle);
        return {res.ok, res.message, std::nullopt};
    } else {
        int rows = numbers.size() >= 1 ? static_cast<int>(numbers[0]) : 2;
        int cols = numbers.size() >= 2 ? static_cast<int>(numbers[1]) : 2;
        double rowSpacing = numbers.size() >= 3 ? numbers[2] : 10.0;
        double colSpacing = numbers.size() >= 4 ? numbers[3] : 10.0;
        auto res = modify::arrayRectangular(ctx.document(), selected, rows, cols, rowSpacing, colSpacing);
        return {res.ok, res.message, std::nullopt};
    }
}

CommandResult handleStretch(CommandContext& ctx, const std::vector<std::string>& args) {
    std::vector<geometry::Vec2> pts;
    for (const auto& arg : args) {
        if (looksLikeCoordinate(arg)) {
            geometry::Vec2 pt;
            std::string err;
            if (readUserPoint(ctx, arg, pt, err)) {
                pts.push_back(pt);
            }
        }
    }
    if (pts.size() < 3) {
        ctx.setPrompt("Specify first corner, second corner, displacement vector:");
        return okMsg("STRETCH: specify window min, window max, and displacement point");
    }
    auto selected = ctx.document().selectionOrdered();
    if (selected.empty()) {
        for (const auto& ent : ctx.document().entities()) {
            if (ctx.document().isEntitySelectable(ent)) {
                selected.push_back(ent.id);
            }
        }
    }
    auto res = modify::stretchEntities(ctx.document(), selected, pts[0], pts[1], pts[2]);
    return {res.ok, res.message, std::nullopt};
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
    registry.registerCommand(CommandSpec{
        .id = "MIRROR",
        .label = "Mirror",
        .aliases = {"MI"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Reflect selection across mirror line",
        .handler = handleMirror,
    });
    registry.registerCommand(CommandSpec{
        .id = "ARRAY",
        .label = "Array",
        .aliases = {"AR", "ARRAYRECT", "ARRAYPOLAR"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Rectangular or polar array copies of selection",
        .handler = handleArray,
    });
    registry.registerCommand(CommandSpec{
        .id = "STRETCH",
        .label = "Stretch",
        .aliases = {"S"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Stretch vertices within crossing window",
        .handler = handleStretch,
    });
}

} // namespace mydesk::commands
