#include "mydesk/commands/registry.hpp"
#include "mydesk/exchange/plotter.hpp"

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
    ctx.clearLastPoint();
    ctx.setPrompt("");
    return okMsg("Started new drawing");
}

CommandResult handleLine(CommandContext& ctx, const std::vector<std::string>& args) {
    ctx.clearInputs();
    for (const auto& arg : args) {
        std::string error;
        if (!pushCoordinate(ctx, arg, error)) {
            return errMsg("LINE: " + error);
        }
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
        std::string error;
        if (!pushCoordinateOrNumber(ctx, arg, error)) {
            return errMsg("CIRCLE: " + error);
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
        std::string error;
        if (!pushCoordinate(ctx, arg, error)) {
            return errMsg("RECTANGLE: " + error);
        }
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
        std::string error;
        if (!pushCoordinate(ctx, arg, error)) {
            return errMsg("MOVE: " + error);
        }
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
        std::string error;
        if (!pushCoordinate(ctx, arg, error)) {
            return errMsg("COPY: " + error);
        }
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
        std::string error;
        if (!pushCoordinateOrNumber(ctx, arg, error)) {
            return errMsg("ROTATE: " + error);
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
        std::string error;
        if (!pushCoordinateOrNumber(ctx, arg, error)) {
            return errMsg("SCALE: " + error);
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
                << (layer.visible ? "ON" : "OFF") << "  "
                << (layer.frozen ? "FROZEN" : "THAWED") << "  "
                << (layer.locked ? "LOCK" : "UNLOCK") << "  "
                << (layer.plottable ? "PLOT" : "NOPLOT") << "  "
                << document::linetypeName(layer.linetype) << "  " << layer.lineweightMm << "mm  #"
                << std::hex << (layer.colorArgb & 0xFFFFFFu) << std::dec;
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
        op == "UNLOCK" || op == "FREEZE" || op == "THAW" || op == "PLOT" || op == "NOPLOT" ||
        op == "COLOR" || op == "LTYPE" || op == "LWEIGHT") {
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
        if (op == "FREEZE") {
            if (layer->id == doc.activeLayerId()) {
                return errMsg("Cannot freeze the current layer");
            }
            if (!doc.setLayerFrozen(layer->id, true)) {
                return errMsg("Cannot freeze layer " + layer->name);
            }
            return okMsg("Layer " + layer->name + " frozen");
        }
        if (op == "THAW") {
            doc.setLayerFrozen(layer->id, false);
            return okMsg("Layer " + layer->name + " thawed");
        }
        if (op == "PLOT") {
            doc.setLayerPlottable(layer->id, true);
            return okMsg("Layer " + layer->name + " will plot");
        }
        if (op == "NOPLOT") {
            doc.setLayerPlottable(layer->id, false);
            return okMsg("Layer " + layer->name + " will not plot");
        }
        if (op == "LTYPE") {
            if (args.size() < 3) {
                return errMsg("LAYER LTYPE <name> <CONTINUOUS|DASHED|HIDDEN|CENTER|PHANTOM|DOT>");
            }
            document::Linetype linetype = document::Linetype::Continuous;
            if (!document::parseLinetype(args[2], linetype) ||
                linetype == document::Linetype::ByLayer) {
                return errMsg("Unknown linetype: " + args[2]);
            }
            if (!doc.setLayerLinetype(layer->id, linetype)) {
                return errMsg("Could not set linetype");
            }
            return okMsg("Layer " + layer->name + " linetype " +
                         std::string(document::linetypeName(linetype)));
        }
        if (op == "LWEIGHT") {
            if (args.size() < 3) {
                return errMsg("LAYER LWEIGHT <name> <millimeters>");
            }
            double mm = 0.0;
            if (!parseNumberToken(args[2], mm) || !doc.setLayerLineweight(layer->id, mm)) {
                return errMsg("Lineweight must be from 0 to 2.11 mm");
            }
            return okMsg("Layer " + layer->name + " lineweight " + args[2] + " mm");
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

    return errMsg(
        "LAYER usage: NEW|MAKE|SET|ON|OFF|FREEZE|THAW|LOCK|UNLOCK|PLOT|NOPLOT|COLOR|LTYPE|LWEIGHT|LIST|?");
}

CommandResult handleChprop(CommandContext& ctx, const std::vector<std::string>& args) {
    auto selected = ctx.document().selectionOrdered();
    if (selected.empty()) {
        return errMsg("CHPROP: select objects first");
    }
    if (args.empty()) {
        return errMsg("CHPROP usage: COLOR <hex>|BYLAYER | LAYER <name> | LTYPE <name>|BYLAYER | LWEIGHT <mm>|BYLAYER | TRANSPARENCY <0.0-1.0>|BYLAYER | THICKNESS <mm>|BYLAYER");
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
    if (op == "LTYPE" || op == "LINETYPE") {
        if (args.size() < 2) {
            return errMsg("CHPROP LTYPE requires a linetype name or BYLAYER");
        }
        document::Linetype linetype = document::Linetype::ByLayer;
        if (!document::parseLinetype(args[1], linetype)) {
            return errMsg("Unknown linetype: " + args[1]);
        }
        for (const auto id : selected) {
            ctx.document().setEntityLinetype(id, linetype);
        }
        return okMsg("Linetype set on " + std::to_string(selected.size()) + " entity(ies)");
    }
    if (op == "LWEIGHT" || op == "LINEWEIGHT") {
        if (args.size() < 2) {
            return errMsg("CHPROP LWEIGHT requires millimeters or BYLAYER");
        }
        if (normalizeToken(args[1]) == "BYLAYER") {
            for (const auto id : selected) {
                ctx.document().setEntityLineweightByLayer(id);
            }
            return okMsg("Lineweight set to ByLayer");
        }
        double mm = 0.0;
        if (!parseNumberToken(args[1], mm)) {
            return errMsg("Lineweight must be from 0 to 2.11 mm");
        }
        std::size_t changed = 0;
        for (const auto id : selected) {
            if (ctx.document().setEntityLineweight(id, mm)) {
                ++changed;
            }
        }
        if (changed == 0) {
            return errMsg("Lineweight must be from 0 to 2.11 mm");
        }
        return okMsg("Lineweight set on " + std::to_string(changed) + " entity(ies)");
    }
    if (op == "TRANSPARENCY") {
        if (args.size() < 2) {
            return errMsg("CHPROP TRANSPARENCY requires value (0.0-1.0) or BYLAYER");
        }
        if (normalizeToken(args[1]) == "BYLAYER") {
            for (const auto id : selected) {
                ctx.document().setEntityTransparencyByLayer(id);
            }
            return okMsg("Transparency set to ByLayer on " + std::to_string(selected.size()) +
                         " entity(ies)");
        }
        double val = 0.0;
        if (!parseNumberToken(args[1], val)) {
            return errMsg("TRANSPARENCY expects value 0.0-1.0 or BYLAYER");
        }
        for (const auto id : selected) {
            ctx.document().setEntityTransparency(id, static_cast<float>(val));
        }
        return okMsg("Transparency updated on " + std::to_string(selected.size()) + " entity(ies)");
    }
    if (op == "THICKNESS") {
        if (args.size() < 2) {
            return errMsg("CHPROP THICKNESS requires value (mm) or BYLAYER");
        }
        if (normalizeToken(args[1]) == "BYLAYER") {
            for (const auto id : selected) {
                ctx.document().setEntityThicknessByLayer(id);
            }
            return okMsg("Thickness set to ByLayer on " + std::to_string(selected.size()) +
                         " entity(ies)");
        }
        double val = 0.0;
        if (!parseNumberToken(args[1], val)) {
            return errMsg("THICKNESS expects numeric value or BYLAYER");
        }
        for (const auto id : selected) {
            ctx.document().setEntityThickness(id, val);
        }
        return okMsg("Thickness updated on " + std::to_string(selected.size()) + " entity(ies)");
    }
    return errMsg("CHPROP usage: COLOR <hex>|BYLAYER | LAYER <name> | LTYPE <name>|BYLAYER | LWEIGHT <mm>|BYLAYER | TRANSPARENCY <0.0-1.0>|BYLAYER | THICKNESS <mm>|BYLAYER");
}

CommandResult handleDimension(CommandContext& ctx, const std::vector<std::string>& args) {
    ctx.clearInputs();
    for (const auto& arg : args) {
        std::string error;
        if (!pushCoordinate(ctx, arg, error)) {
            return errMsg("DIMLINEAR: " + error);
        }
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

CommandResult handleText(CommandContext& ctx, const std::vector<std::string>& args) {
    // TEXT x,y height content...   (content may be multiple tokens joined)
    if (args.size() < 3) {
        if (args.empty()) {
            ctx.setPrompt("Specify start point of text:");
            return okMsg("TEXT: need point");
        }
        return errMsg("TEXT usage: TEXT <x,y> <height> <string>");
    }
    geometry::Vec2 pos;
    std::string pointError;
    if (!readUserPoint(ctx, args[0], pos, pointError)) {
        return errMsg("TEXT: " + pointError);
    }
    double height = 0.0;
    if (!parseNumberToken(args[1], height)) {
        return errMsg("TEXT expects height number");
    }
    std::ostringstream content;
    for (std::size_t i = 2; i < args.size(); ++i) {
        if (i > 2) {
            content << ' ';
        }
        content << args[i];
    }
    try {
        const auto id = ctx.document().addText(pos, height, content.str());
        ctx.clearInputs();
        ctx.setPrompt("");
        return okMsg("TEXT created #" + std::to_string(id), id);
    } catch (const std::exception& ex) {
        return errMsg(ex.what());
    }
}

CommandResult handleHatch(CommandContext& ctx, const std::vector<std::string>& args) {
    (void)args;
    const auto selected = ctx.document().selectionOrdered();
    if (selected.empty()) {
        return errMsg("HATCH: select a closed polyline first");
    }
    std::size_t created = 0;
    for (const auto id : selected) {
        const auto* entity = ctx.document().findEntity(id);
        if (!entity) {
            continue;
        }
        const auto* pl = std::get_if<document::PolylineEntity>(&entity->geometry);
        if (!pl || !pl->closed || pl->vertices.size() < 3) {
            continue;
        }
        try {
            ctx.document().addHatch(pl->vertices);
            ++created;
        } catch (const std::exception& ex) {
            return errMsg(ex.what());
        }
    }
    if (created == 0) {
        return errMsg("HATCH: selection must include a closed PLINE/RECTANGLE");
    }
    ctx.setPrompt("");
    return okMsg("HATCH created " + std::to_string(created) + " solid fill(s)");
}

CommandResult handleBlock(CommandContext& ctx, const std::vector<std::string>& args) {
    if (ctx.document().selectionOrdered().empty()) {
        return errMsg("BLOCK: select objects first");
    }
    std::string name = ctx.scratch();
    geometry::Vec2 base{};
    bool haveBase = false;
    for (const auto& arg : args) {
        geometry::Vec2 p;
        if (looksLikeCoordinate(arg)) {
            std::string error;
            if (!readUserPoint(ctx, arg, p, error)) {
                return errMsg("BLOCK: " + error);
            }
            base = p;
            haveBase = true;
        } else if (name.empty()) {
            name = arg;
        } else {
            return errMsg("BLOCK usage: BLOCK <name> <baseX,baseY>");
        }
    }
    if (name.empty()) {
        return errMsg("BLOCK usage: BLOCK <name> <baseX,baseY>");
    }
    ctx.setScratch(name);
    if (!haveBase) {
        ctx.setPrompt("Specify insertion base point:");
        return okMsg("BLOCK: need base point");
    }
    try {
        if (!ctx.document().defineBlockFromSelection(name, base)) {
            return errMsg("BLOCK failed");
        }
        ctx.clearInputs();
        ctx.setScratch("");
        ctx.setPrompt("");
        return okMsg("Block " + name + " defined");
    } catch (const std::exception& ex) {
        return errMsg(ex.what());
    }
}

CommandResult handleInsert(CommandContext& ctx, const std::vector<std::string>& args) {
    // INSERT <name> [x,y] [scale] [rotationDeg]
    std::string name = ctx.scratch();
    std::vector<geometry::Vec2> points;
    std::vector<double> numbers;
    for (const auto& arg : args) {
        geometry::Vec2 p;
        double n = 0.0;
        if (looksLikeCoordinate(arg)) {
            std::string error;
            if (!readUserPoint(ctx, arg, p, error)) {
                return errMsg("INSERT: " + error);
            }
            points.push_back(p);
        } else if (parseNumberToken(arg, n)) {
            numbers.push_back(n);
        } else if (name.empty()) {
            name = arg;
        } else {
            return errMsg("INSERT usage: INSERT <name> <x,y> [scale] [rotation]");
        }
    }
    if (name.empty()) {
        return errMsg("INSERT usage: INSERT <name> <x,y> [scale] [rotation]");
    }
    if (!ctx.document().findBlock(name)) {
        return errMsg("Unknown block: " + name);
    }
    ctx.setScratch(name);
    if (points.empty()) {
        ctx.setPrompt("Specify insertion point:");
        return okMsg("INSERT: need insertion point");
    }
    const double scale = numbers.empty() ? 1.0 : numbers[0];
    const double rot = numbers.size() >= 2 ? numbers[1] : 0.0;
    try {
        const auto id = ctx.document().addInsert(name, points.front(), scale, rot);
        ctx.clearInputs();
        ctx.setScratch("");
        ctx.setPrompt("");
        return okMsg("INSERT created #" + std::to_string(id), id);
    } catch (const std::exception& ex) {
        return errMsg(ex.what());
    }
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

CommandResult handleExplode(CommandContext& ctx, const std::vector<std::string>&) {
    const auto selected = ctx.document().selectionOrdered();
    if (selected.empty()) {
        return errMsg("EXPLODE: select objects first");
    }
    const auto result = ctx.document().explodeEntities(selected);
    if (result.exploded == 0) {
        return errMsg("EXPLODE: none of the selected objects can be exploded");
    }
    ctx.document().clearSelection();
    for (const auto id : result.created) {
        ctx.document().select(id);
    }
    ctx.setPrompt("");
    std::ostringstream oss;
    oss << "Exploded " << result.exploded << " object(s)";
    if (result.skipped != 0) {
        oss << "; " << result.skipped << " could not be exploded";
    }
    return okMsg(oss.str());
}

CommandResult handleLinetypeScale(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        return okMsg("LTSCALE " + std::to_string(ctx.document().linetypeScale()));
    }
    double scale = 0.0;
    if (!parseNumberToken(args[0], scale)) {
        return errMsg("LTSCALE expects a positive scale");
    }
    try {
        ctx.document().setLinetypeScale(scale);
    } catch (const std::exception& ex) {
        return errMsg(ex.what());
    }
    return okMsg("LTSCALE " + args[0]);
}

CommandResult handleAttdef(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        ctx.setPrompt("Specify attribute tag name:");
        return okMsg("ATTDEF: need tag");
    }
    std::string tag, prompt, defaultValue;
    geometry::Vec2 pos{0.0, 0.0};
    bool havePos = false;
    double height = 2.5;
    double rotation = 0.0;
    bool invisible = false;
    std::uint32_t flags = 0;
    bool isMultiline = false;

    std::vector<std::string> cleanArgs;
    for (const auto& a : args) {
        if (!a.empty() && (a[0] == '/' || a[0] == '-')) {
            std::string sw = a.substr(1);
            for (char c : sw) {
                switch (std::toupper(static_cast<unsigned char>(c))) {
                case 'I': flags |= document::kAttrFlagInvisible; invisible = true; break;
                case 'C': flags |= document::kAttrFlagConstant; break;
                case 'V': flags |= document::kAttrFlagVerify; break;
                case 'P': flags |= document::kAttrFlagPreset; break;
                case 'L': flags |= document::kAttrFlagLockPosition; break;
                case 'M': flags |= document::kAttrFlagMultiline; isMultiline = true; break;
                default: break;
                }
            }
        } else {
            cleanArgs.push_back(a);
        }
    }

    if (cleanArgs.empty()) {
        ctx.setPrompt("Specify attribute tag name:");
        return okMsg("ATTDEF: need tag");
    }

    if (looksLikeCoordinate(cleanArgs[0])) {
        std::string err;
        if (!readUserPoint(ctx, cleanArgs[0], pos, err)) {
            return errMsg("ATTDEF: " + err);
        }
        havePos = true;
        if (cleanArgs.size() > 1 && parseNumberToken(cleanArgs[1], height)) {
            if (cleanArgs.size() > 2) tag = cleanArgs[2];
            if (cleanArgs.size() > 3) prompt = cleanArgs[3];
            if (cleanArgs.size() > 4) defaultValue = cleanArgs[4];
            if (cleanArgs.size() > 5) {
                (void)parseNumberToken(cleanArgs[5], rotation);
            }
        }
    } else {
        tag = cleanArgs[0];
        if (cleanArgs.size() > 1) prompt = cleanArgs[1];
        if (cleanArgs.size() > 2) defaultValue = cleanArgs[2];
        if (cleanArgs.size() > 3 && looksLikeCoordinate(cleanArgs[3])) {
            std::string err;
            if (!readUserPoint(ctx, cleanArgs[3], pos, err)) {
                return errMsg("ATTDEF: " + err);
            }
            havePos = true;
            if (cleanArgs.size() > 4) {
                (void)parseNumberToken(cleanArgs[4], height);
            }
            if (cleanArgs.size() > 5) {
                (void)parseNumberToken(cleanArgs[5], rotation);
            }
        }
    }

    if (tag.empty()) {
        return errMsg("ATTDEF: tag cannot be empty");
    }
    if (!havePos) {
        ctx.setScratch(tag);
        ctx.setPrompt("Specify start point:");
        return okMsg("ATTDEF: need start point");
    }

    try {
        const auto id = ctx.document().addAttdef(pos, height, tag, prompt, defaultValue,
                                                rotation, invisible, flags, isMultiline);
        ctx.clearInputs();
        ctx.setScratch("");
        ctx.setPrompt("");
        return okMsg("ATTDEF created #" + std::to_string(id), id);
    } catch (const std::exception& ex) {
        return errMsg(ex.what());
    }
}

CommandResult handleAttedit(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        return errMsg("ATTEDIT usage: ATTEDIT [entity_id] <tag> <new_value>");
    }

    document::EntityId targetId = document::kInvalidEntityId;
    std::string tag;
    std::string newValue;

    double idNum = 0.0;
    if (args.size() >= 3 && parseNumberToken(args[0], idNum)) {
        targetId = static_cast<document::EntityId>(idNum);
        tag = args[1];
        for (std::size_t i = 2; i < args.size(); ++i) {
            if (i > 2) newValue += " ";
            newValue += args[i];
        }
    } else {
        const auto selected = ctx.document().selectionOrdered();
        for (const auto id : selected) {
            const auto* ent = ctx.document().findEntity(id);
            if (ent && std::holds_alternative<document::InsertEntity>(ent->geometry)) {
                targetId = id;
                break;
            }
        }
        if (targetId == document::kInvalidEntityId && selected.size() == 1) {
            targetId = selected.front();
        }
        tag = args[0];
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (i > 1) newValue += " ";
            newValue += args[i];
        }
    }

    if (targetId == document::kInvalidEntityId) {
        return errMsg("ATTEDIT: select a block reference (INSERT) first or provide entity ID");
    }

    const auto* ent = ctx.document().findEntity(targetId);
    if (!ent || !std::holds_alternative<document::InsertEntity>(ent->geometry)) {
        return errMsg("ATTEDIT: entity #" + std::to_string(targetId) + " is not an INSERT");
    }

    if (!ctx.document().editInsertAttribute(targetId, tag, newValue)) {
        return errMsg("ATTEDIT: attribute tag '" + tag + "' not found in block reference #" + std::to_string(targetId));
    }
    return okMsg("Attribute " + tag + " updated in #" + std::to_string(targetId));
}

CommandResult handleAttdisp(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        const auto current = ctx.document().attributeDisplayMode();
        std::string curStr = "Normal";
        if (current == document::AttributeDisplayMode::On) curStr = "ON";
        else if (current == document::AttributeDisplayMode::Off) curStr = "OFF";
        return okMsg("ATTDISP is " + curStr + ". Usage: ATTDISP <Normal|ON|OFF>");
    }
    std::string modeStr = args[0];
    for (char& c : modeStr) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (modeStr == "NORMAL" || modeStr == "N") {
        ctx.document().setAttributeDisplayMode(document::AttributeDisplayMode::Normal);
        return okMsg("ATTDISP set to Normal");
    } else if (modeStr == "ON" || modeStr == "1") {
        ctx.document().setAttributeDisplayMode(document::AttributeDisplayMode::On);
        return okMsg("ATTDISP set to ON");
    } else if (modeStr == "OFF" || modeStr == "0") {
        ctx.document().setAttributeDisplayMode(document::AttributeDisplayMode::Off);
        return okMsg("ATTDISP set to OFF");
    }
    return errMsg("ATTDISP mode must be Normal, ON, or OFF");
}

CommandResult handleLayout(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.empty() || args[0] == "?" || args[0] == "LIST" || args[0] == "list") {
        std::ostringstream ss;
        ss << "Layouts: ";
        const auto& all = ctx.document().layouts();
        for (std::size_t i = 0; i < all.size(); ++i) {
            if (i > 0) ss << ", ";
            if (all[i].name == ctx.document().activeLayoutName()) ss << "*";
            ss << all[i].name;
            if (all[i].isModelSpace) ss << " (Model)";
        }
        return okMsg(ss.str());
    }

    std::string sub = args[0];
    for (char& c : sub) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if ((sub == "NEW" || sub == "N") && args.size() >= 2) {
        if (!ctx.document().addLayout(args[1])) {
            return errMsg("LAYOUT: failed to create layout " + args[1]);
        }
        return okMsg("Layout " + args[1] + " created");
    } else if ((sub == "SET" || sub == "S") && args.size() >= 2) {
        if (!ctx.document().setActiveLayout(args[1])) {
            return errMsg("LAYOUT: layout not found " + args[1]);
        }
        return okMsg("Switched to layout " + args[1]);
    } else if ((sub == "RENAME" || sub == "R") && args.size() >= 3) {
        if (!ctx.document().renameLayout(args[1], args[2])) {
            return errMsg("LAYOUT: rename failed");
        }
        return okMsg("Layout " + args[1] + " renamed to " + args[2]);
    } else if ((sub == "DELETE" || sub == "D") && args.size() >= 2) {
        if (!ctx.document().removeLayout(args[1])) {
            return errMsg("LAYOUT: cannot delete " + args[1]);
        }
        return okMsg("Layout " + args[1] + " deleted");
    }
    return errMsg("LAYOUT usage: LAYOUT [New|Set|Rename|Delete|List] [args...]");
}

CommandResult handleTilemode(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        return okMsg("TILEMODE is " + std::string(ctx.document().tileMode() ? "1" : "0"));
    }
    double val = 0.0;
    if (!parseNumberToken(args[0], val)) {
        return errMsg("TILEMODE expects 0 or 1");
    }
    ctx.document().setTileMode(val != 0.0);
    return okMsg("TILEMODE set to " + std::to_string(static_cast<int>(val != 0.0)));
}

CommandResult handleMview(CommandContext& ctx, const std::vector<std::string>& args) {
    if (ctx.document().tileMode()) {
        return errMsg("MVIEW / VPORTS: viewports only supported in paper space layouts (TILEMODE=0)");
    }
    if (args.size() < 3) {
        return errMsg("MVIEW usage: MVIEW <cx,cy> <width_mm> <height_mm> [view_height]");
    }
    geometry::Vec2 center;
    std::string err;
    if (!readUserPoint(ctx, args[0], center, err)) {
        return errMsg("MVIEW: " + err);
    }
    double w = 0.0, h = 0.0, vh = 100.0;
    if (!parseNumberToken(args[1], w) || !parseNumberToken(args[2], h) || w <= 0.0 || h <= 0.0) {
        return errMsg("MVIEW: width and height must be positive numbers");
    }
    if (args.size() > 3) {
        (void)parseNumberToken(args[3], vh);
    }
    document::Viewport vp;
    vp.center = center;
    vp.width = w;
    vp.height = h;
    vp.viewHeight = vh > 0.0 ? vh : 100.0;
    auto vpId = ctx.document().addViewportToLayout(ctx.document().activeLayoutName(), vp);
    if (vpId == 0) {
        return errMsg("MVIEW: failed to add viewport");
    }
    return okMsg("Viewport #" + std::to_string(vpId) + " created in layout " + ctx.document().activeLayoutName(), vpId);
}

CommandResult handlePspace(CommandContext& ctx, const std::vector<std::string>&) {
    if (ctx.document().tileMode()) {
        return errMsg("PSPACE: command not allowed in Model tab (TILEMODE=1)");
    }
    ctx.document().setPaperSpace(true);
    return okMsg("Switched to Paper Space (PSPACE)");
}

CommandResult handleMspace(CommandContext& ctx, const std::vector<std::string>&) {
    if (ctx.document().tileMode()) {
        return okMsg("Already in Model space");
    }
    ctx.document().setPaperSpace(false);
    return okMsg("Switched to Model Space inside Viewport (MSPACE)");
}

CommandResult handlePlot(CommandContext& ctx, const std::vector<std::string>& args) {
    std::string layout = ctx.document().activeLayoutName();
    std::filesystem::path outPath = std::filesystem::current_path() / (layout + ".svg");
    if (!args.empty()) {
        if (args.size() >= 2) {
            layout = args[0];
            outPath = args[1];
        } else {
            outPath = args[0];
        }
    }
    auto report = exchange::plotToSvg(ctx.document(), layout, outPath);
    if (!report.ok) {
        return errMsg("PLOT failed: " + report.message);
    }
    return okMsg("Plotted " + layout + " (" + std::to_string(report.renderedEntities) + " entities) to " + outPath.string());
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
        .notes = "Command-line layer manager: NEW/MAKE/SET/ON/OFF/FREEZE/THAW/LOCK/UNLOCK/PLOT/NOPLOT/COLOR/LIST",
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
    registry.registerCommand(CommandSpec{
        .id = "BLOCK",
        .label = "Block",
        .aliases = {"B"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Define block from selection + base point; replaces selection with INSERT",
        .handler = handleBlock,
    });
    registry.registerCommand(CommandSpec{
        .id = "INSERT",
        .label = "Insert",
        .aliases = {"I"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Insert block reference: name, point, optional scale/rotation",
        .handler = handleInsert,
    });
    registry.registerCommand(CommandSpec{
        .id = "TEXT",
        .label = "Text",
        .aliases = {"T"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Single-line text: point, height, contents",
        .handler = handleText,
    });
    registry.registerCommand(CommandSpec{
        .id = "HATCH",
        .label = "Hatch",
        .aliases = {"H", "BH"},
        .status = CommandStatus::Partial,
        .notes = "Solid fill from selected closed polyline only",
        .handler = handleHatch,
    });
    registry.registerCommand(CommandSpec{
        .id = "EXPLODE",
        .label = "Explode",
        .aliases = {"X"},
        .status = CommandStatus::ImplementedTested,
        .notes = "One level: INSERT to transformed primitives, POLYLINE to lines",
        .handler = handleExplode,
    });
    registry.registerCommand(CommandSpec{
        .id = "LTSCALE",
        .label = "Linetype scale",
        .aliases = {"LTS"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Global linetype scale applied when stroking patterns",
        .handler = handleLinetypeScale,
    });
    registry.registerCommand(CommandSpec{
        .id = "ATTDEF",
        .label = "Define Attribute",
        .aliases = {"ATT"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Create attribute definition tag/prompt/default with position and height",
        .handler = handleAttdef,
    });
    registry.registerCommand(CommandSpec{
        .id = "ATTEDIT",
        .label = "Edit Attribute",
        .aliases = {"ATE"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Edit attribute value in block reference",
        .handler = handleAttedit,
    });
    registry.registerCommand(CommandSpec{
        .id = "EATTEDIT",
        .label = "Enhanced Attribute Editor",
        .aliases = {},
        .status = CommandStatus::ImplementedTested,
        .notes = "Inspect and edit attributes in block reference",
        .handler = handleAttedit,
    });
    registry.registerCommand(CommandSpec{
        .id = "ATTDISP",
        .label = "Attribute Display Mode",
        .aliases = {},
        .status = CommandStatus::ImplementedTested,
        .notes = "Control attribute visibility (Normal, ON, OFF)",
        .handler = handleAttdisp,
    });
    registry.registerCommand(CommandSpec{
        .id = "LAYOUT",
        .label = "Layout management",
        .aliases = {"LO"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Create, switch, rename, delete paper space layouts",
        .handler = handleLayout,
    });
    registry.registerCommand(CommandSpec{
        .id = "TILEMODE",
        .label = "Tile mode",
        .aliases = {},
        .status = CommandStatus::ImplementedTested,
        .notes = "Toggle Model space tabs (1) vs Paper space layout tabs (0)",
        .handler = handleTilemode,
    });
    registry.registerCommand(CommandSpec{
        .id = "MVIEW",
        .label = "Make Viewport",
        .aliases = {"MV", "VPORTS"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Create floating paper space viewport displaying model geometry",
        .handler = handleMview,
    });
    registry.registerCommand(CommandSpec{
        .id = "PSPACE",
        .label = "Paper space",
        .aliases = {"PS"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Switch to paper space mode on active layout",
        .handler = handlePspace,
    });
    registry.registerCommand(CommandSpec{
        .id = "MSPACE",
        .label = "Model space in viewport",
        .aliases = {"MS"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Activate model space editing inside viewport",
        .handler = handleMspace,
    });
    registry.registerCommand(CommandSpec{
        .id = "PLOT",
        .label = "Plot to vector output",
        .aliases = {"PRINT", "EXPORTPDF"},
        .status = CommandStatus::ImplementedTested,
        .notes = "Plot active layout or model space to vector SVG",
        .handler = handlePlot,
    });

    const std::vector<std::pair<std::string, std::vector<std::string>>> planned = {
        {"ZOOM", {"Z"}}, {"ARC", {"A"}}, {"MIRROR", {"MI"}},
        {"STRETCH", {"S"}}, {"XLINE", {"XL"}}, {"PAN", {"P"}},
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
