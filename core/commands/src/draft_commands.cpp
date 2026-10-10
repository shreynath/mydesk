#include "mydesk/commands/registry.hpp"

#include <cmath>
#include <cstdint>
#include <sstream>
#include <string_view>

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

bool parseColorToken(std::string_view token, std::uint32_t& outArgb) {
    std::string hex(token);
    if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
        hex = hex.substr(2);
    }
    if (!hex.empty() && hex[0] == '#') {
        hex = hex.substr(1);
    }
    if (hex.size() != 6 && hex.size() != 8) {
        return false;
    }
    try {
        const unsigned long value = std::stoul(hex, nullptr, 16);
        if (hex.size() == 6) {
            outArgb = 0xFF000000u | static_cast<std::uint32_t>(value);
        } else {
            outArgb = static_cast<std::uint32_t>(value);
        }
        return true;
    } catch (...) {
        return false;
    }
}

CommandResult handleLayer(CommandContext& ctx, const std::vector<std::string>& args) {
    auto& doc = ctx.document();
    if (args.empty() || normalizeToken(args[0]) == "?" || normalizeToken(args[0]) == "LIST" ||
        normalizeToken(args[0]) == "?LIST") {
        std::ostringstream oss;
        oss << "Layers (" << doc.layers().size() << "):\n";
        for (const auto& layer : doc.layers()) {
            const bool current = layer.id == doc.activeLayerId();
            oss << (current ? "* " : "  ") << layer.name << "  "
                << (layer.visible ? "ON" : "OFF") << "  " << (layer.locked ? "LOCK" : "UNLOCK")
                << "  #" << std::hex << (layer.colorArgb & 0xFFFFFFu) << std::dec;
            if (current) {
                oss << "  (current)";
            }
            oss << '\n';
        }
        ctx.setPrompt("");
        return okMsg(oss.str());
    }

    const std::string op = normalizeToken(args[0]);
    if (op == "NEW" || op == "MAKE" || op == "SET" || op == "ON" || op == "OFF" || op == "LOCK" ||
        op == "UNLOCK" || op == "COLOR") {
        if (args.size() < 2) {
            return errMsg("LAYER " + op + " requires a layer name");
        }
        const std::string& name = args[1];
        if (op == "NEW") {
            if (doc.findLayerByName(name)) {
                return errMsg("Layer already exists: " + name);
            }
            const auto id = doc.addLayer(name);
            ctx.setPrompt("");
            return okMsg("Created layer " + name + " #" + std::to_string(id));
        }
        if (op == "MAKE") {
            const auto id = doc.addLayer(name);
            doc.setActiveLayer(id);
            ctx.setPrompt("");
            return okMsg("Current layer set to " + name);
        }
        if (op == "SET") {
            const auto* layer = doc.findLayerByName(name);
            if (!layer) {
                return errMsg("Unknown layer: " + name);
            }
            doc.setActiveLayer(layer->id);
            ctx.setPrompt("");
            return okMsg("Current layer set to " + layer->name);
        }
        auto* layer = doc.findLayerByName(name);
        if (!layer) {
            return errMsg("Unknown layer: " + name);
        }
        if (op == "ON") {
            doc.setLayerVisible(layer->id, true);
            return okMsg("Layer " + layer->name + " ON");
        }
        if (op == "OFF") {
            if (layer->id == doc.activeLayerId()) {
                return errMsg("Cannot turn off the current layer");
            }
            doc.setLayerVisible(layer->id, false);
            return okMsg("Layer " + layer->name + " OFF");
        }
        if (op == "LOCK") {
            doc.setLayerLocked(layer->id, true);
            return okMsg("Layer " + layer->name + " locked");
        }
        if (op == "UNLOCK") {
            doc.setLayerLocked(layer->id, false);
            return okMsg("Layer " + layer->name + " unlocked");
        }
        if (op == "COLOR") {
            if (args.size() < 3) {
                return errMsg("LAYER COLOR <name> <RRGGBB>");
            }
            std::uint32_t argb = 0;
            if (!parseColorToken(args[2], argb)) {
                return errMsg("COLOR expects RRGGBB hex (e.g. FF8800)");
            }
            doc.setLayerColor(layer->id, argb);
            return okMsg("Layer " + layer->name + " color updated");
        }
    }

    // Shorthand: LAYER <name> sets current (creates if missing), like MAKE.
    if (args.size() == 1) {
        const auto id = doc.addLayer(args[0]);
        doc.setActiveLayer(id);
        ctx.setPrompt("");
        return okMsg("Current layer set to " + args[0]);
    }

    return errMsg("LAYER usage: NEW|MAKE|SET|ON|OFF|LOCK|UNLOCK|COLOR|LIST|?");
}

CommandResult handleChprop(CommandContext& ctx, const std::vector<std::string>& args) {
    auto selected = ctx.document().selectionOrdered();
    if (selected.empty()) {
        return errMsg("CHPROP: select objects first");
    }
    if (args.empty()) {
        return errMsg("CHPROP usage: COLOR <hex>|BYLAYER | LAYER <name>");
    }
    const std::string op = normalizeToken(args[0]);
    if (op == "COLOR") {
        if (args.size() < 2) {
            return errMsg("CHPROP COLOR requires hex or BYLAYER");
        }
        if (normalizeToken(args[1]) == "BYLAYER") {
            for (const auto id : selected) {
                ctx.document().setEntityColor(id, document::kColorByLayer);
            }
            return okMsg("Color set to ByLayer on " + std::to_string(selected.size()) +
                         " entity(ies)");
        }
        std::uint32_t argb = 0;
        if (!parseColorToken(args[1], argb)) {
            return errMsg("CHPROP COLOR expects RRGGBB or BYLAYER");
        }
        for (const auto id : selected) {
            ctx.document().setEntityColor(id, argb);
        }
        return okMsg("Color updated on " + std::to_string(selected.size()) + " entity(ies)");
    }
    if (op == "LAYER") {
        if (args.size() < 2) {
            return errMsg("CHPROP LAYER requires a layer name");
        }
        auto* layer = ctx.document().findLayerByName(args[1]);
        if (!layer) {
            return errMsg("Unknown layer: " + args[1]);
        }
        for (const auto id : selected) {
            ctx.document().setEntityLayer(id, layer->id);
        }
        return okMsg("Layer set to " + layer->name + " on " + std::to_string(selected.size()) +
                     " entity(ies)");
    }
    return errMsg("CHPROP usage: COLOR <hex>|BYLAYER | LAYER <name>");
}

CommandResult handleDimension(CommandContext& ctx, const std::vector<std::string>& args) {
    ctx.clearInputs();
    for (const auto& arg : args) {
        geometry::Vec2 p;
        if (!parsePointToken(arg, p)) {
            return errMsg("DIMLINEAR expects points as x,y");
        }
        ctx.pushPoint(p);
    }
    if (ctx.points().size() >= 3) {
        try {
            const auto id = ctx.document().addDimensionLinear(ctx.points()[0], ctx.points()[1],
                                                              ctx.points()[2]);
            ctx.clearInputs();
            ctx.setPrompt("");
            return okMsg("DIMLINEAR created #" + std::to_string(id), id);
        } catch (const std::exception& ex) {
            return errMsg(ex.what());
        }
    }
    if (ctx.points().size() == 2) {
        ctx.setPrompt("Specify dimension line location:");
        return okMsg("DIMLINEAR: need dim line point");
    }
    if (ctx.points().size() == 1) {
        ctx.setPrompt("Specify second extension line origin:");
        return okMsg("DIMLINEAR: need second point");
    }
    ctx.setPrompt("Specify first extension line origin:");
    return okMsg("DIMLINEAR: need first point");
}

CommandResult handleOsnap(CommandContext& ctx, const std::vector<std::string>& args) {
    DraftModes* modes = ctx.modes();
    if (!modes) {
        return errMsg("OSNAP unavailable in this context");
    }
    if (args.empty() || normalizeToken(args[0]) == "?" || normalizeToken(args[0]) == "LIST") {
        std::ostringstream oss;
        oss << "OSNAP master=" << (modes->objectSnap ? "ON" : "OFF")
            << " END=" << (modes->snapEndpoint ? "ON" : "OFF")
            << " MID=" << (modes->snapMidpoint ? "ON" : "OFF")
            << " CEN=" << (modes->snapCenter ? "ON" : "OFF")
            << " INT=" << (modes->snapIntersection ? "ON" : "OFF")
            << " PER=" << (modes->snapPerpendicular ? "ON" : "OFF");
        return okMsg(oss.str());
    }
    const std::string tok = normalizeToken(args[0]);
    if (tok == "ON") {
        modes->objectSnap = true;
        return okMsg("Object snap ON");
    }
    if (tok == "OFF") {
        modes->objectSnap = false;
        return okMsg("Object snap OFF");
    }
    auto setFlag = [&](bool& flag, const std::string& name) -> CommandResult {
        if (args.size() >= 2) {
            const auto v = normalizeToken(args[1]);
            if (v == "ON" || v == "1") {
                flag = true;
            } else if (v == "OFF" || v == "0") {
                flag = false;
            } else {
                return errMsg("OSNAP " + name + " expects ON|OFF");
            }
        } else {
            flag = !flag;
        }
        return okMsg(name + (flag ? " ON" : " OFF"));
    };
    if (tok == "END" || tok == "ENDPOINT") {
        return setFlag(modes->snapEndpoint, "END");
    }
    if (tok == "MID" || tok == "MIDPOINT") {
        return setFlag(modes->snapMidpoint, "MID");
    }
    if (tok == "CEN" || tok == "CENTER") {
        return setFlag(modes->snapCenter, "CEN");
    }
    if (tok == "INT" || tok == "INTERSECTION") {
        return setFlag(modes->snapIntersection, "INT");
    }
    if (tok == "PER" || tok == "PERPENDICULAR") {
        return setFlag(modes->snapPerpendicular, "PER");
    }
    return errMsg("OSNAP usage: ON|OFF|END|MID|CEN|INT|PER [?]");
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
    registry.registerCommand(CommandSpec{
        .id = "LAYER",
        .label = "Layer",
        .aliases = {"LA"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Command-line layer manager: NEW/MAKE/SET/ON/OFF/LOCK/UNLOCK/COLOR/LIST",
        .handler = handleLayer,
    });
    registry.registerCommand(CommandSpec{
        .id = "CHPROP",
        .label = "Change properties",
        .aliases = {"CH"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Change selection COLOR (hex|BYLAYER) or LAYER",
        .handler = handleChprop,
    });
    registry.registerCommand(CommandSpec{
        .id = "DIMLINEAR",
        .label = "Linear dimension",
        .aliases = {"DIM", "DIMENSION"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Aligned linear dimension: two origins + dim-line location",
        .handler = handleDimension,
    });
    registry.registerCommand(CommandSpec{
        .id = "OSNAP",
        .label = "Object snap",
        .aliases = {"OS"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Toggle snap modes END/MID/CEN/INT/PER",
        .handler = handleOsnap,
    });

    const std::vector<std::pair<std::string, std::vector<std::string>>> planned = {
        {"BLOCK", {"B"}},       {"INSERT", {"I"}}, {"HATCH", {"H", "BH"}},
        {"ZOOM", {"Z"}},   {"ARC", {"A"}},    {"MIRROR", {"MI"}},
        {"STRETCH", {"S"}}, {"EXPLODE", {"X"}},    {"XLINE", {"XL"}},
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
