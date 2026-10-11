#include "mydesk/document/document.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace mydesk::document {

namespace {

bool layerNamesEqual(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto ca = static_cast<unsigned char>(a[i]);
        const auto cb = static_cast<unsigned char>(b[i]);
        if (std::toupper(ca) != std::toupper(cb)) {
            return false;
        }
    }
    return true;
}

} // namespace

Document::Document() {
    const LayerId id = allocateLayerId();
    Layer layer;
    layer.id = id;
    layer.name = "0";
    layers_.push_back(layer);
    activeLayerId_ = id;

    // Default layouts: Model and Layout1
    Layout model;
    model.name = "Model";
    model.isModelSpace = true;
    layouts_.push_back(model);

    Layout layout1;
    layout1.name = "Layout1";
    layout1.isModelSpace = false;
    Viewport vp;
    vp.id = nextViewportId_++;
    vp.center = {130.0, 100.0};
    vp.width = 240.0;
    vp.height = 180.0;
    vp.viewCenter = {0.0, 0.0};
    vp.viewHeight = 100.0;
    layout1.viewports.push_back(vp);
    layouts_.push_back(layout1);

    activeLayoutName_ = "Model";
    tileMode_ = true;
    isPaperSpace_ = false;
}

bool Document::setActiveLayer(LayerId id) {
    if (!findLayer(id)) {
        return false;
    }
    activeLayerId_ = id;
    return true;
}

const Layer* Document::findLayer(LayerId id) const {
    for (const auto& layer : layers_) {
        if (layer.id == id) {
            return &layer;
        }
    }
    return nullptr;
}

Layer* Document::findLayer(LayerId id) {
    return const_cast<Layer*>(static_cast<const Document*>(this)->findLayer(id));
}

const Layer* Document::findLayerByName(std::string_view name) const {
    for (const auto& layer : layers_) {
        if (layerNamesEqual(layer.name, name)) {
            return &layer;
        }
    }
    return nullptr;
}

Layer* Document::findLayerByName(std::string_view name) {
    return const_cast<Layer*>(static_cast<const Document*>(this)->findLayerByName(name));
}

const Entity* Document::findEntity(EntityId id) const {
    for (const auto& entity : entities_) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}

Entity* Document::findEntity(EntityId id) {
    return const_cast<Entity*>(static_cast<const Document*>(this)->findEntity(id));
}

LayerId Document::addLayer(std::string name) {
    if (name.empty()) {
        throw std::invalid_argument("layer name must not be empty");
    }
    if (const Layer* existing = findLayerByName(name)) {
        return existing->id;
    }
    Layer layer;
    layer.id = allocateLayerId();
    layer.name = std::move(name);
    layers_.push_back(layer);
    dirty_ = true;
    return layer.id;
}

bool Document::setLayerVisible(LayerId id, bool visible) {
    Layer* layer = findLayer(id);
    if (!layer) {
        return false;
    }
    if (layer->visible == visible) {
        return true;
    }
    layer->visible = visible;
    dirty_ = true;
    return true;
}

bool Document::setLayerLocked(LayerId id, bool locked) {
    Layer* layer = findLayer(id);
    if (!layer) {
        return false;
    }
    if (layer->locked == locked) {
        return true;
    }
    layer->locked = locked;
    dirty_ = true;
    return true;
}

bool Document::setLayerColor(LayerId id, std::uint32_t colorArgb) {
    Layer* layer = findLayer(id);
    if (!layer) {
        return false;
    }
    if (layer->colorArgb == colorArgb) {
        return true;
    }
    layer->colorArgb = colorArgb;
    dirty_ = true;
    return true;
}

bool Document::setLayerFrozen(LayerId id, bool frozen) {
    Layer* layer = findLayer(id);
    if (!layer) {
        return false;
    }
    if (frozen && id == activeLayerId_) {
        return false;
    }
    if (layer->frozen == frozen) {
        return true;
    }
    layer->frozen = frozen;
    dirty_ = true;
    return true;
}

bool Document::setLayerPlottable(LayerId id, bool plottable) {
    Layer* layer = findLayer(id);
    if (!layer) {
        return false;
    }
    if (layer->plottable == plottable) {
        return true;
    }
    layer->plottable = plottable;
    dirty_ = true;
    return true;
}

bool Document::setLayerLinetype(LayerId id, Linetype linetype) {
    Layer* layer = findLayer(id);
    if (!layer || linetype == Linetype::ByLayer) {
        return false;
    }
    if (layer->linetype == linetype) {
        return true;
    }
    layer->linetype = linetype;
    dirty_ = true;
    return true;
}

bool Document::setLayerLineweight(LayerId id, double millimeters) {
    Layer* layer = findLayer(id);
    if (!layer || !validLineweightMm(millimeters)) {
        return false;
    }
    if (layer->lineweightMm == millimeters) {
        return true;
    }
    layer->lineweightMm = millimeters;
    dirty_ = true;
    return true;
}

bool Document::setLayerTransparency(LayerId id, float transparency) {
    Layer* layer = findLayer(id);
    if (!layer || transparency < 0.0f || transparency > 1.0f) {
        return false;
    }
    if (layer->transparency == transparency) {
        return true;
    }
    layer->transparency = transparency;
    dirty_ = true;
    return true;
}

bool Document::setLayerThickness(LayerId id, double thickness) {
    Layer* layer = findLayer(id);
    if (!layer || thickness < 0.0) {
        return false;
    }
    if (layer->thickness == thickness) {
        return true;
    }
    layer->thickness = thickness;
    dirty_ = true;
    return true;
}

const LayerState* Document::findLayerState(std::string_view name) const {
    const auto it = std::find_if(layerStates_.begin(), layerStates_.end(), [&](const LayerState& state) {
        return layerNamesEqual(state.name, name);
    });
    return it == layerStates_.end() ? nullptr : &*it;
}

bool Document::saveLayerState(std::string name) {
    if (name.empty()) {
        return false;
    }
    LayerState snapshot;
    snapshot.name = std::move(name);
    if (const Layer* active = findLayer(activeLayerId_)) {
        snapshot.activeLayerName = active->name;
    }
    snapshot.entries.reserve(layers_.size());
    for (const auto& layer : layers_) {
        snapshot.entries.push_back(LayerStateEntry{
            layer.name, layer.visible, layer.locked, layer.frozen, layer.plottable,
            layer.colorArgb, layer.linetype, layer.lineweightMm, layer.transparency,
            layer.thickness});
    }
    const auto it = std::find_if(layerStates_.begin(), layerStates_.end(), [&](const LayerState& state) {
        return layerNamesEqual(state.name, snapshot.name);
    });
    if (it == layerStates_.end()) {
        layerStates_.push_back(std::move(snapshot));
    } else {
        *it = std::move(snapshot);
    }
    dirty_ = true;
    return true;
}

bool Document::restoreLayerState(std::string_view name) {
    const LayerState* stored = findLayerState(name);
    if (!stored) {
        return false;
    }
    // Copy because applying the transaction can reallocate unrelated containers
    // in future implementations without invalidating our source snapshot.
    const LayerState state = *stored;
    Transaction tx;
    tx.label = "LAYERSTATE RESTORE " + state.name;
    for (const auto& entry : state.entries) {
        Layer* current = findLayerByName(entry.layerName);
        if (!current) {
            continue;
        }
        Layer after = *current;
        after.visible = entry.visible;
        after.locked = entry.locked;
        after.frozen = entry.frozen;
        after.plottable = entry.plottable;
        after.colorArgb = entry.colorArgb;
        after.linetype = entry.linetype;
        after.lineweightMm = entry.lineweightMm;
        after.transparency = entry.transparency;
        after.thickness = entry.thickness;
        if (after.visible != current->visible || after.locked != current->locked ||
            after.frozen != current->frozen || after.plottable != current->plottable ||
            after.colorArgb != current->colorArgb || after.linetype != current->linetype ||
            after.lineweightMm != current->lineweightMm ||
            after.transparency != current->transparency || after.thickness != current->thickness) {
            HistoryOp op;
            op.kind = HistoryOpKind::ReplaceLayer;
            op.beforeLayer = *current;
            op.afterLayer = after;
            tx.ops.push_back(std::move(op));
        }
    }
    LayerId restoredActive = activeLayerId_;
    if (const Layer* layer = findLayerByName(state.activeLayerName)) {
        restoredActive = layer->id;
    }
    if (restoredActive != activeLayerId_) {
        HistoryOp op;
        op.kind = HistoryOpKind::SetActiveLayer;
        op.beforeActiveLayer = activeLayerId_;
        op.afterActiveLayer = restoredActive;
        tx.ops.push_back(std::move(op));
    }
    if (tx.ops.empty()) {
        return true;
    }
    for (const auto& op : tx.ops) {
        if (op.kind == HistoryOpKind::ReplaceLayer) {
            applyReplaceLayer(op.afterLayer);
        } else if (op.kind == HistoryOpKind::SetActiveLayer) {
            applySetActiveLayer(op.afterActiveLayer);
        }
    }
    commit(std::move(tx));
    return true;
}

bool Document::deleteLayerState(std::string_view name) {
    const auto it = std::find_if(layerStates_.begin(), layerStates_.end(), [&](const LayerState& state) {
        return layerNamesEqual(state.name, name);
    });
    if (it == layerStates_.end()) {
        return false;
    }
    layerStates_.erase(it);
    dirty_ = true;
    return true;
}

void Document::setLinetypeScale(double scale) {
    if (!(scale > 0.0) || !std::isfinite(scale)) {
        throw std::invalid_argument("linetype scale must be positive");
    }
    linetypeScale_ = scale;
    dirty_ = true;
}

bool Document::isEntityVisible(const Entity& entity) const {
    const Layer* layer = findLayer(entity.layerId);
    if (!layer) {
        return true;
    }
    return layer->visible && !layer->frozen;
}

bool Document::isEntitySelectable(const Entity& entity) const {
    const Layer* layer = findLayer(entity.layerId);
    if (!layer) {
        return true;
    }
    return layer->visible && !layer->frozen && !layer->locked;
}

bool Document::setEntityLayer(EntityId id, LayerId layerId) {
    const Entity* existing = findEntity(id);
    if (!existing || !findLayer(layerId)) {
        return false;
    }
    if (existing->layerId == layerId) {
        return true;
    }
    Entity after = *existing;
    after.layerId = layerId;
    Transaction tx;
    tx.label = "CHLAYER";
    tx.ops.push_back(HistoryOp{HistoryOpKind::ReplaceEntity, *existing, after});
    applyReplaceEntity(after);
    commit(std::move(tx));
    return true;
}

bool Document::setEntityColor(EntityId id, std::uint32_t colorArgb) {
    const Entity* existing = findEntity(id);
    if (!existing) {
        return false;
    }
    if (existing->colorArgb == colorArgb) {
        return true;
    }
    Entity after = *existing;
    after.colorArgb = colorArgb;
    Transaction tx;
    tx.label = "CHPROP";
    tx.ops.push_back(HistoryOp{HistoryOpKind::ReplaceEntity, *existing, after});
    applyReplaceEntity(after);
    commit(std::move(tx));
    return true;
}

bool Document::setEntityLinetype(EntityId id, Linetype linetype) {
    const Entity* existing = findEntity(id);
    if (!existing) {
        return false;
    }
    if (existing->linetype == linetype) {
        return true;
    }
    Entity after = *existing;
    after.linetype = linetype;
    Transaction tx;
    tx.label = "CHPROP";
    tx.ops.push_back(HistoryOp{HistoryOpKind::ReplaceEntity, *existing, after});
    applyReplaceEntity(after);
    commit(std::move(tx));
    return true;
}

bool Document::setEntityLineweight(EntityId id, double millimeters) {
    const Entity* existing = findEntity(id);
    if (!existing || !validLineweightMm(millimeters)) {
        return false;
    }
    if (!existing->lineweightByLayer && existing->lineweightMm == millimeters) {
        return true;
    }
    Entity after = *existing;
    after.lineweightByLayer = false;
    after.lineweightMm = millimeters;
    Transaction tx;
    tx.label = "CHPROP";
    tx.ops.push_back(HistoryOp{HistoryOpKind::ReplaceEntity, *existing, after});
    applyReplaceEntity(after);
    commit(std::move(tx));
    return true;
}

bool Document::setEntityLineweightByLayer(EntityId id) {
    const Entity* existing = findEntity(id);
    if (!existing) {
        return false;
    }
    if (existing->lineweightByLayer) {
        return true;
    }
    Entity after = *existing;
    after.lineweightByLayer = true;
    Transaction tx;
    tx.label = "CHPROP";
    tx.ops.push_back(HistoryOp{HistoryOpKind::ReplaceEntity, *existing, after});
    applyReplaceEntity(after);
    commit(std::move(tx));
    return true;
}

bool Document::setEntityTransparency(EntityId id, float transparency) {
    const Entity* existing = findEntity(id);
    if (!existing || transparency < 0.0f || transparency > 1.0f) {
        return false;
    }
    if (!existing->transparencyByLayer && existing->transparency == transparency) {
        return true;
    }
    Entity after = *existing;
    after.transparencyByLayer = false;
    after.transparency = transparency;
    Transaction tx;
    tx.label = "CHPROP";
    tx.ops.push_back(HistoryOp{HistoryOpKind::ReplaceEntity, *existing, after});
    applyReplaceEntity(after);
    commit(std::move(tx));
    return true;
}

bool Document::setEntityTransparencyByLayer(EntityId id) {
    const Entity* existing = findEntity(id);
    if (!existing) {
        return false;
    }
    if (existing->transparencyByLayer) {
        return true;
    }
    Entity after = *existing;
    after.transparencyByLayer = true;
    Transaction tx;
    tx.label = "CHPROP";
    tx.ops.push_back(HistoryOp{HistoryOpKind::ReplaceEntity, *existing, after});
    applyReplaceEntity(after);
    commit(std::move(tx));
    return true;
}

bool Document::setEntityThickness(EntityId id, double thickness) {
    const Entity* existing = findEntity(id);
    if (!existing || thickness < 0.0) {
        return false;
    }
    if (!existing->thicknessByLayer && existing->thickness == thickness) {
        return true;
    }
    Entity after = *existing;
    after.thicknessByLayer = false;
    after.thickness = thickness;
    Transaction tx;
    tx.label = "CHPROP";
    tx.ops.push_back(HistoryOp{HistoryOpKind::ReplaceEntity, *existing, after});
    applyReplaceEntity(after);
    commit(std::move(tx));
    return true;
}

bool Document::setEntityThicknessByLayer(EntityId id) {
    const Entity* existing = findEntity(id);
    if (!existing) {
        return false;
    }
    if (existing->thicknessByLayer) {
        return true;
    }
    Entity after = *existing;
    after.thicknessByLayer = true;
    Transaction tx;
    tx.label = "CHPROP";
    tx.ops.push_back(HistoryOp{HistoryOpKind::ReplaceEntity, *existing, after});
    applyReplaceEntity(after);
    commit(std::move(tx));
    return true;
}

std::uint32_t Document::resolvedEntityColor(const Entity& entity) const {
    if (entity.colorArgb != kColorByLayer) {
        return entity.colorArgb;
    }
    if (const Layer* layer = findLayer(entity.layerId)) {
        return layer->colorArgb;
    }
    return 0xFFE0E0E0u;
}

Linetype Document::resolvedLinetype(const Entity& entity) const {
    if (entity.linetype != Linetype::ByLayer) {
        return entity.linetype;
    }
    if (const Layer* layer = findLayer(entity.layerId)) {
        return layer->linetype;
    }
    return Linetype::Continuous;
}

double Document::resolvedLineweightMm(const Entity& entity) const {
    if (!entity.lineweightByLayer) {
        return entity.lineweightMm;
    }
    if (const Layer* layer = findLayer(entity.layerId)) {
        return layer->lineweightMm;
    }
    return 0.25;
}

float Document::resolvedTransparency(const Entity& entity) const {
    if (!entity.transparencyByLayer) {
        return entity.transparency;
    }
    if (const Layer* layer = findLayer(entity.layerId)) {
        return layer->transparency;
    }
    return 1.0f;
}

double Document::resolvedThickness(const Entity& entity) const {
    if (!entity.thicknessByLayer) {
        return entity.thickness;
    }
    if (const Layer* layer = findLayer(entity.layerId)) {
        return layer->thickness;
    }
    return 0.0;
}

EntityId Document::addLine(geometry::Vec2 start, geometry::Vec2 end, std::string txLabel) {
    Entity entity;
    entity.id = allocateEntityId();
    entity.layerId = activeLayerId_;
    entity.geometry = LineEntity{start, end};
    Transaction tx;
    tx.label = std::move(txLabel);
    tx.ops.push_back(HistoryOp{HistoryOpKind::AddEntity, {}, entity});
    applyAddEntity(entity);
    commit(std::move(tx));
    return entity.id;
}

EntityId Document::addCircle(geometry::Vec2 center, double radius, std::string txLabel) {
    if (!(radius > 0.0) || !std::isfinite(radius)) {
        throw std::invalid_argument("circle radius must be a positive finite value");
    }
    Entity entity;
    entity.id = allocateEntityId();
    entity.layerId = activeLayerId_;
    entity.geometry = CircleEntity{center, radius};
    Transaction tx;
    tx.label = std::move(txLabel);
    tx.ops.push_back(HistoryOp{HistoryOpKind::AddEntity, {}, entity});
    applyAddEntity(entity);
    commit(std::move(tx));
    return entity.id;
}

EntityId Document::addArc(geometry::Vec2 center, double radius, double startAngle, double endAngle,
                          std::string txLabel) {
    if (!(radius > 0.0) || !std::isfinite(radius)) {
        throw std::invalid_argument("arc radius must be a positive finite value");
    }
    Entity entity;
    entity.id = allocateEntityId();
    entity.layerId = activeLayerId_;
    entity.geometry = ArcEntity{center, radius, startAngle, endAngle};
    Transaction tx;
    tx.label = std::move(txLabel);
    tx.ops.push_back(HistoryOp{HistoryOpKind::AddEntity, {}, entity});
    applyAddEntity(entity);
    commit(std::move(tx));
    return entity.id;
}

EntityId Document::addPolyline(std::vector<geometry::Vec2> vertices, bool closed,
                               std::string txLabel) {
    if (vertices.size() < 2) {
        throw std::invalid_argument("polyline requires at least two vertices");
    }
    Entity entity;
    entity.id = allocateEntityId();
    entity.layerId = activeLayerId_;
    entity.geometry = PolylineEntity{std::move(vertices), closed};
    Transaction tx;
    tx.label = std::move(txLabel);
    tx.ops.push_back(HistoryOp{HistoryOpKind::AddEntity, {}, entity});
    applyAddEntity(entity);
    commit(std::move(tx));
    return entity.id;
}

EntityId Document::addRectangle(geometry::Vec2 cornerA, geometry::Vec2 cornerB,
                                std::string txLabel) {
    const double minX = std::min(cornerA.x, cornerB.x);
    const double maxX = std::max(cornerA.x, cornerB.x);
    const double minY = std::min(cornerA.y, cornerB.y);
    const double maxY = std::max(cornerA.y, cornerB.y);
    if (!(maxX > minX) || !(maxY > minY)) {
        throw std::invalid_argument("rectangle corners must define a non-zero area");
    }
    std::vector<geometry::Vec2> verts = {
        {minX, minY}, {maxX, minY}, {maxX, maxY}, {minX, maxY},
    };
    return addPolyline(std::move(verts), true, std::move(txLabel));
}

EntityId Document::addDimensionLinear(geometry::Vec2 start, geometry::Vec2 end, geometry::Vec2 dimLine,
                                      std::string txLabel) {
    if (start.nearlyEqual(end)) {
        throw std::invalid_argument("dimension points must be distinct");
    }
    Entity entity;
    entity.id = allocateEntityId();
    entity.layerId = activeLayerId_;
    entity.geometry = DimensionLinearEntity{start, end, dimLine};
    Transaction tx;
    tx.label = std::move(txLabel);
    tx.ops.push_back(HistoryOp{HistoryOpKind::AddEntity, {}, entity});
    applyAddEntity(entity);
    commit(std::move(tx));
    return entity.id;
}

const BlockDefinition* Document::findBlock(std::string_view name) const {
    for (const auto& block : blocks_) {
        if (layerNamesEqual(block.name, name)) {
            return &block;
        }
    }
    return nullptr;
}

BlockDefinition* Document::findBlock(std::string_view name) {
    return const_cast<BlockDefinition*>(static_cast<const Document*>(this)->findBlock(name));
}

bool Document::defineBlockFromSelection(std::string name, geometry::Vec2 base, std::string txLabel) {
    if (name.empty()) {
        throw std::invalid_argument("block name must not be empty");
    }
    const auto selected = selectionOrdered();
    if (selected.empty()) {
        return false;
    }
    BlockDefinition def;
    def.name = std::move(name);
    geometry::BBox localBounds;
    beginComposite(std::move(txLabel));
    for (const auto id : selected) {
        const Entity* existing = findEntity(id);
        if (!existing) {
            continue;
        }
        auto shift = [&](geometry::Vec2 p) { return p - base; };
        EntityGeometry local = existing->geometry;
        if (auto* line = std::get_if<LineEntity>(&local)) {
            line->start = shift(line->start);
            line->end = shift(line->end);
        } else if (auto* circle = std::get_if<CircleEntity>(&local)) {
            circle->center = shift(circle->center);
        } else if (auto* arc = std::get_if<ArcEntity>(&local)) {
            arc->center = shift(arc->center);
        } else if (auto* pl = std::get_if<PolylineEntity>(&local)) {
            for (auto& v : pl->vertices) {
                v = shift(v);
            }
        } else if (auto* dim = std::get_if<DimensionLinearEntity>(&local)) {
            dim->start = shift(dim->start);
            dim->end = shift(dim->end);
            dim->dimLine = shift(dim->dimLine);
        } else if (auto* text = std::get_if<TextEntity>(&local)) {
            text->position = shift(text->position);
        } else if (auto* attdef = std::get_if<AttdefEntity>(&local)) {
            AttributeDefinition ad;
            ad.tag = attdef->tag;
            ad.prompt = attdef->prompt;
            ad.defaultValue = attdef->defaultValue;
            ad.position = shift(attdef->position);
            ad.height = attdef->height;
            ad.rotationDeg = attdef->rotationDeg;
            ad.invisible = attdef->invisible;
            ad.flags = attdef->flags;
            ad.isMultiline = attdef->isMultiline;
            def.attributes.push_back(std::move(ad));
            localBounds.expand(boundsOfGeometry(local));
            compositeRemove(id);
            continue;
        } else if (auto* ins = std::get_if<InsertEntity>(&local)) {
            if (ins->blockName == name) {
                abortComposite();
                throw std::invalid_argument("BLOCK cannot recursively nest an insert of itself: " + name);
            }
            ins->insertion = shift(ins->insertion);
            for (auto& attr : ins->attributes) {
                attr.position = shift(attr.position);
            }
        } else if (auto* hatch = std::get_if<HatchEntity>(&local)) {
            for (auto& v : hatch->boundary) {
                v = shift(v);
            }
        }
        localBounds.expand(boundsOfGeometry(local));
        def.primitives.push_back(BlockPrimitive{local, existing->colorArgb});
        compositeRemove(id);
    }
    if (def.primitives.empty() && def.attributes.empty()) {
        abortComposite();
        return false;
    }
    const std::string blockName = def.name;
    if (auto* existing = findBlock(blockName)) {
        *existing = std::move(def);
    } else {
        blocks_.push_back(std::move(def));
    }
    // Place an insert at the base so the drawing still shows the geometry.
    Entity insertEntity;
    insertEntity.id = allocateEntityId();
    insertEntity.layerId = activeLayerId_;
    InsertEntity insert;
    insert.blockName = blockName;
    insert.insertion = base;
    insert.localBounds = localBounds;
    const BlockDefinition* createdBlock = findBlock(blockName);
    if (createdBlock) {
        for (const auto& attr : createdBlock->attributes) {
            AttributeInstance inst;
            inst.tag = attr.tag;
            inst.value = attr.defaultValue;
            inst.position = base + attr.position;
            inst.height = attr.height;
            inst.rotationDeg = attr.rotationDeg;
            inst.invisible = attr.invisible;
            insert.attributes.push_back(std::move(inst));
        }
    }
    insertEntity.geometry = insert;
    compositeAdd(insertEntity);
    if (!commitComposite()) {
        return false;
    }
    clearSelection();
    select(insertEntity.id);
    dirty_ = true;
    return true;
}

EntityId Document::addText(geometry::Vec2 position, double height, std::string contents,
                           double rotationDeg, std::string txLabel) {
    if (!(height > 0.0) || !std::isfinite(height)) {
        throw std::invalid_argument("text height must be positive");
    }
    if (contents.empty()) {
        throw std::invalid_argument("text contents must not be empty");
    }
    Entity entity;
    entity.id = allocateEntityId();
    entity.layerId = activeLayerId_;
    entity.geometry = TextEntity{position, height, rotationDeg, std::move(contents)};
    Transaction tx;
    tx.label = std::move(txLabel);
    tx.ops.push_back(HistoryOp{HistoryOpKind::AddEntity, {}, entity});
    applyAddEntity(entity);
    commit(std::move(tx));
    return entity.id;
}

EntityId Document::addHatch(std::vector<geometry::Vec2> boundary, std::uint32_t fillArgb,
                            std::string txLabel) {
    if (boundary.size() < 3) {
        throw std::invalid_argument("hatch boundary requires at least 3 points");
    }
    Entity entity;
    entity.id = allocateEntityId();
    entity.layerId = activeLayerId_;
    entity.geometry = HatchEntity{std::move(boundary), fillArgb};
    Transaction tx;
    tx.label = std::move(txLabel);
    tx.ops.push_back(HistoryOp{HistoryOpKind::AddEntity, {}, entity});
    applyAddEntity(entity);
    commit(std::move(tx));
    return entity.id;
}

EntityId Document::addInsert(std::string blockName, geometry::Vec2 insertion, double scale,
                             double rotationDeg,
                             const std::unordered_map<std::string, std::string>& attributeOverrides,
                             std::string txLabel) {
    const BlockDefinition* block = findBlock(blockName);
    if (!block) {
        throw std::invalid_argument("unknown block: " + blockName);
    }
    if (!(std::isfinite(scale)) || std::abs(scale) < 1e-12) {
        throw std::invalid_argument("insert scale must be non-zero");
    }
    InsertEntity insert;
    insert.blockName = block->name;
    insert.insertion = insertion;
    insert.scale = scale;
    insert.rotationDeg = rotationDeg;
    for (const auto& prim : block->primitives) {
        insert.localBounds.expand(boundsOfGeometry(prim.geometry));
    }
    for (const auto& attr : block->attributes) {
        AttributeInstance inst;
        inst.tag = attr.tag;
        const auto it = attributeOverrides.find(attr.tag);
        if (it != attributeOverrides.end()) {
            inst.value = it->second;
        } else {
            inst.value = attr.defaultValue;
        }
        inst.position = transformInsertPoint(insert, attr.position);
        inst.height = attr.height * std::abs(scale);
        inst.rotationDeg = attr.rotationDeg + rotationDeg;
        inst.invisible = attr.invisible || ((attr.flags & kAttrFlagInvisible) != 0);
        inst.flags = attr.flags;
        inst.isMultiline = attr.isMultiline;
        insert.attributes.push_back(std::move(inst));
    }
    Entity entity;
    entity.id = allocateEntityId();
    entity.layerId = activeLayerId_;
    entity.geometry = insert;
    Transaction tx;
    tx.label = std::move(txLabel);
    tx.ops.push_back(HistoryOp{HistoryOpKind::AddEntity, {}, entity});
    applyAddEntity(entity);
    commit(std::move(tx));
    return entity.id;
}

EntityId Document::addAttdef(geometry::Vec2 position, double height, std::string tag,
                             std::string prompt, std::string defaultValue,
                             double rotationDeg, bool invisible,
                             std::uint32_t flags, bool isMultiline, std::string txLabel) {
    if (!(height > 0.0) || !std::isfinite(height)) {
        throw std::invalid_argument("attdef height must be positive");
    }
    if (tag.empty()) {
        throw std::invalid_argument("attdef tag must not be empty");
    }
    const bool inv = invisible || ((flags & kAttrFlagInvisible) != 0);
    Entity entity;
    entity.id = allocateEntityId();
    entity.layerId = activeLayerId_;
    entity.geometry = AttdefEntity{std::move(tag), std::move(prompt), std::move(defaultValue),
                                   position, height, rotationDeg, inv, flags, isMultiline};
    Transaction tx;
    tx.label = std::move(txLabel);
    tx.ops.push_back(HistoryOp{HistoryOpKind::AddEntity, {}, entity});
    applyAddEntity(entity);
    commit(std::move(tx));
    return entity.id;
}

bool Document::editInsertAttribute(EntityId id, const std::string& tag, const std::string& newValue,
                                   std::string txLabel) {
    const Entity* existing = findEntity(id);
    if (!existing) {
        return false;
    }
    const auto* insert = std::get_if<InsertEntity>(&existing->geometry);
    if (!insert) {
        return false;
    }
    std::string upperTag;
    upperTag.reserve(tag.size());
    for (char c : tag) {
        upperTag.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }

    bool found = false;
    InsertEntity afterInsert = *insert;
    for (auto& attr : afterInsert.attributes) {
        std::string curUpper;
        curUpper.reserve(attr.tag.size());
        for (char c : attr.tag) {
            curUpper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
        if (curUpper == upperTag) {
            attr.value = newValue;
            found = true;
            break;
        }
    }
    if (!found) {
        return false;
    }
    Entity after = *existing;
    after.geometry = afterInsert;
    Transaction tx;
    tx.label = std::move(txLabel);
    tx.ops.push_back(HistoryOp{HistoryOpKind::ReplaceEntity, *existing, after});
    applyReplaceEntity(after);
    commit(std::move(tx));
    return true;
}

void Document::setAttributeDisplayMode(AttributeDisplayMode mode) {
    attdisp_ = mode;
    dirty_ = true;
}

ExplodeResult Document::explodeEntities(const std::vector<EntityId>& ids, std::string txLabel) {
    ExplodeResult result;
    Transaction tx;
    tx.label = std::move(txLabel);

    for (const auto id : ids) {
        const Entity* entity = findEntity(id);
        if (!entity) {
            ++result.skipped;
            continue;
        }

        std::vector<Entity> created;
        if (const auto* polyline = std::get_if<PolylineEntity>(&entity->geometry)) {
            const std::size_t segments = polylineSegmentCount(*polyline);
            if (segments == 0) {
                ++result.skipped;
                continue;
            }
            for (std::size_t i = 0; i < segments; ++i) {
                const geometry::Vec2 start = polylineSegmentStart(*polyline, i);
                const geometry::Vec2 end = polylineSegmentEnd(*polyline, i);
                if (start.nearlyEqual(end)) {
                    continue;
                }
                Entity line;
                line.id = allocateEntityId();
                line.layerId = entity->layerId;
                line.colorArgb = entity->colorArgb;
                line.geometry = LineEntity{start, end};
                created.push_back(line);
            }
        } else if (const auto* insert = std::get_if<InsertEntity>(&entity->geometry)) {
            const BlockDefinition* block = findBlock(insert->blockName);
            if (!block || (block->primitives.empty() && insert->attributes.empty())) {
                ++result.skipped;
                continue;
            }
            for (const auto& primitive : block->primitives) {
                EntityGeometry world = transformInsertGeometry(*insert, primitive.geometry);
                Entity piece;
                piece.id = allocateEntityId();
                piece.layerId = entity->layerId;
                piece.colorArgb = primitive.colorArgb;
                piece.geometry = std::move(world);
                created.push_back(piece);
            }
            for (const auto& attr : insert->attributes) {
                Entity piece;
                piece.id = allocateEntityId();
                piece.layerId = entity->layerId;
                piece.colorArgb = entity->colorArgb;
                piece.geometry = TextEntity{attr.position, attr.height, attr.rotationDeg, attr.value};
                created.push_back(piece);
            }
        } else {
            ++result.skipped;
            continue;
        }

        if (created.empty()) {
            ++result.skipped;
            continue;
        }

        tx.ops.push_back(HistoryOp{HistoryOpKind::RemoveEntity, *entity, {}});
        for (const auto& piece : created) {
            tx.ops.push_back(HistoryOp{HistoryOpKind::AddEntity, {}, piece});
            result.created.push_back(piece.id);
        }
        ++result.exploded;
    }

    if (result.exploded == 0) {
        result.created.clear();
        return result;
    }

    for (const auto& op : tx.ops) {
        if (op.kind == HistoryOpKind::RemoveEntity) {
            applyRemoveEntity(op.before.id);
        } else if (op.kind == HistoryOpKind::AddEntity) {
            applyAddEntity(op.after);
        }
    }
    commit(std::move(tx));
    return result;
}

bool Document::removeEntity(EntityId id, std::string txLabel) {
    const Entity* existing = findEntity(id);
    if (!existing) {
        return false;
    }
    Transaction tx;
    tx.label = std::move(txLabel);
    tx.ops.push_back(HistoryOp{HistoryOpKind::RemoveEntity, *existing, {}});
    applyRemoveEntity(id);
    commit(std::move(tx));
    return true;
}

bool Document::replaceEntity(EntityId id, EntityGeometry geometry, std::string txLabel) {
    const Entity* existing = findEntity(id);
    if (!existing) {
        return false;
    }
    Entity after = *existing;
    after.geometry = std::move(geometry);
    Transaction tx;
    tx.label = std::move(txLabel);
    tx.ops.push_back(HistoryOp{HistoryOpKind::ReplaceEntity, *existing, after});
    applyReplaceEntity(after);
    commit(std::move(tx));
    return true;
}

void Document::beginComposite(std::string label) {
    composite_ = Transaction{};
    composite_->label = std::move(label);
}

void Document::compositeAdd(const Entity& entity) {
    if (!composite_) {
        throw std::runtime_error("no composite transaction");
    }
    composite_->ops.push_back(HistoryOp{HistoryOpKind::AddEntity, {}, entity});
    applyAddEntity(entity);
}

void Document::compositeRemove(EntityId id) {
    if (!composite_) {
        throw std::runtime_error("no composite transaction");
    }
    const Entity* existing = findEntity(id);
    if (!existing) {
        return;
    }
    composite_->ops.push_back(HistoryOp{HistoryOpKind::RemoveEntity, *existing, {}});
    applyRemoveEntity(id);
}

void Document::compositeReplace(const Entity& before, const Entity& after) {
    if (!composite_) {
        throw std::runtime_error("no composite transaction");
    }
    composite_->ops.push_back(HistoryOp{HistoryOpKind::ReplaceEntity, before, after});
    applyReplaceEntity(after);
}

bool Document::commitComposite() {
    if (!composite_ || composite_->ops.empty()) {
        composite_.reset();
        return false;
    }
    commit(std::move(*composite_));
    composite_.reset();
    return true;
}

void Document::abortComposite() {
    composite_.reset();
}

namespace {

geometry::Vec2 rotatePoint(geometry::Vec2 p, geometry::Vec2 base, double radians) {
    const geometry::Vec2 d = p - base;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    return {base.x + d.x * c - d.y * s, base.y + d.x * s + d.y * c};
}

geometry::Vec2 scalePoint(geometry::Vec2 p, geometry::Vec2 base, double factor) {
    return base + (p - base) * factor;
}

Entity transformEntity(Entity entity, const std::function<geometry::Vec2(geometry::Vec2)>& mapPoint,
                       double radiusScale = 1.0, double angleDelta = 0.0) {
    if (auto* line = std::get_if<LineEntity>(&entity.geometry)) {
        line->start = mapPoint(line->start);
        line->end = mapPoint(line->end);
    } else if (auto* circle = std::get_if<CircleEntity>(&entity.geometry)) {
        circle->center = mapPoint(circle->center);
        circle->radius *= radiusScale;
    } else if (auto* arc = std::get_if<ArcEntity>(&entity.geometry)) {
        arc->center = mapPoint(arc->center);
        arc->radius *= radiusScale;
        arc->startAngle += angleDelta;
        arc->endAngle += angleDelta;
    } else if (auto* pl = std::get_if<PolylineEntity>(&entity.geometry)) {
        for (auto& v : pl->vertices) {
            v = mapPoint(v);
        }
    } else if (auto* dim = std::get_if<DimensionLinearEntity>(&entity.geometry)) {
        dim->start = mapPoint(dim->start);
        dim->end = mapPoint(dim->end);
        dim->dimLine = mapPoint(dim->dimLine);
    } else if (auto* insert = std::get_if<InsertEntity>(&entity.geometry)) {
        insert->insertion = mapPoint(insert->insertion);
        insert->scale *= radiusScale;
        insert->rotationDeg += angleDelta * (180.0 / 3.14159265358979323846);
        for (auto& attr : insert->attributes) {
            attr.position = mapPoint(attr.position);
            attr.height *= radiusScale;
            attr.rotationDeg += angleDelta * (180.0 / 3.14159265358979323846);
        }
    } else if (auto* text = std::get_if<TextEntity>(&entity.geometry)) {
        text->position = mapPoint(text->position);
        text->height *= radiusScale;
        text->rotationDeg += angleDelta * (180.0 / 3.14159265358979323846);
    } else if (auto* attdef = std::get_if<AttdefEntity>(&entity.geometry)) {
        attdef->position = mapPoint(attdef->position);
        attdef->height *= radiusScale;
        attdef->rotationDeg += angleDelta * (180.0 / 3.14159265358979323846);
    } else if (auto* hatch = std::get_if<HatchEntity>(&entity.geometry)) {
        for (auto& v : hatch->boundary) {
            v = mapPoint(v);
        }
    }
    return entity;
}

} // namespace

bool Document::moveEntities(const std::vector<EntityId>& ids, geometry::Vec2 delta,
                            std::string txLabel) {
    if (ids.empty()) {
        return false;
    }
    Transaction tx;
    tx.label = std::move(txLabel);
    for (const auto id : ids) {
        const Entity* existing = findEntity(id);
        if (!existing) {
            continue;
        }
        Entity after = transformEntity(*existing, [&](geometry::Vec2 p) { return p + delta; });
        tx.ops.push_back(HistoryOp{HistoryOpKind::ReplaceEntity, *existing, after});
        applyReplaceEntity(after);
    }
    if (tx.ops.empty()) {
        return false;
    }
    commit(std::move(tx));
    return true;
}

bool Document::rotateEntities(const std::vector<EntityId>& ids, geometry::Vec2 base, double degrees,
                              std::string txLabel) {
    if (ids.empty() || !std::isfinite(degrees)) {
        return false;
    }
    const double radians = degrees * (3.14159265358979323846 / 180.0);
    Transaction tx;
    tx.label = std::move(txLabel);
    for (const auto id : ids) {
        const Entity* existing = findEntity(id);
        if (!existing) {
            continue;
        }
        Entity after = transformEntity(
            *existing, [&](geometry::Vec2 p) { return rotatePoint(p, base, radians); }, 1.0,
            radians);
        tx.ops.push_back(HistoryOp{HistoryOpKind::ReplaceEntity, *existing, after});
        applyReplaceEntity(after);
    }
    if (tx.ops.empty()) {
        return false;
    }
    commit(std::move(tx));
    return true;
}

bool Document::scaleEntities(const std::vector<EntityId>& ids, geometry::Vec2 base, double factor,
                             std::string txLabel) {
    if (ids.empty() || !(factor > 0.0) || !std::isfinite(factor)) {
        return false;
    }
    Transaction tx;
    tx.label = std::move(txLabel);
    for (const auto id : ids) {
        const Entity* existing = findEntity(id);
        if (!existing) {
            continue;
        }
        Entity after = transformEntity(
            *existing, [&](geometry::Vec2 p) { return scalePoint(p, base, factor); }, factor);
        tx.ops.push_back(HistoryOp{HistoryOpKind::ReplaceEntity, *existing, after});
        applyReplaceEntity(after);
    }
    if (tx.ops.empty()) {
        return false;
    }
    commit(std::move(tx));
    return true;
}

std::vector<EntityId> Document::copyEntities(const std::vector<EntityId>& ids, geometry::Vec2 delta,
                                             std::string txLabel) {
    std::vector<EntityId> created;
    if (ids.empty()) {
        return created;
    }
    Transaction tx;
    tx.label = std::move(txLabel);
    for (const auto id : ids) {
        const Entity* existing = findEntity(id);
        if (!existing) {
            continue;
        }
        Entity copy = transformEntity(*existing, [&](geometry::Vec2 p) { return p + delta; });
        copy.id = allocateEntityId();
        tx.ops.push_back(HistoryOp{HistoryOpKind::AddEntity, {}, copy});
        applyAddEntity(copy);
        created.push_back(copy.id);
    }
    if (!tx.ops.empty()) {
        commit(std::move(tx));
    }
    return created;
}

void Document::select(EntityId id) {
    if (findEntity(id)) {
        selection_.insert(id);
    }
}

void Document::deselect(EntityId id) { selection_.erase(id); }

void Document::toggleSelect(EntityId id) {
    if (!findEntity(id)) {
        return;
    }
    if (selection_.count(id) != 0U) {
        selection_.erase(id);
    } else {
        selection_.insert(id);
    }
}

void Document::clearSelection() { selection_.clear(); }

void Document::selectInRect(const geometry::BBox& rect, bool crossing, bool additive) {
    if (!additive) {
        selection_.clear();
    }
    for (const auto& entity : entities_) {
        if (!isEntitySelectable(entity)) {
            continue;
        }
        if (entity.intersectsRect(rect, crossing)) {
            selection_.insert(entity.id);
        }
    }
}

EntityId Document::hitTestTop(geometry::Vec2 point, double tol) const {
    for (auto it = entities_.rbegin(); it != entities_.rend(); ++it) {
        if (!isEntitySelectable(*it)) {
            continue;
        }
        if (const auto* insert = std::get_if<InsertEntity>(&it->geometry)) {
            const BlockDefinition* block = findBlock(insert->blockName);
            if (!block) {
                continue;
            }
            bool hit = false;
            for (const auto& prim : block->primitives) {
                Entity probe;
                probe.geometry = transformInsertGeometry(*insert, prim.geometry);
                if (probe.hitTest(point, tol)) {
                    hit = true;
                    break;
                }
            }
            if (hit) {
                return it->id;
            }
            continue;
        }
        if (it->hitTest(point, tol)) {
            return it->id;
        }
    }
    return kInvalidEntityId;
}

std::vector<EntityId> Document::selectionOrdered() const {
    std::vector<EntityId> ordered;
    ordered.reserve(selection_.size());
    for (const auto& entity : entities_) {
        if (selection_.count(entity.id) != 0U) {
            ordered.push_back(entity.id);
        }
    }
    return ordered;
}

bool Document::undo() {
    if (!history_.undo(*this)) {
        return false;
    }
    dirty_ = true;
    return true;
}

bool Document::redo() {
    if (!history_.redo(*this)) {
        return false;
    }
    dirty_ = true;
    return true;
}

void Document::applyAddEntity(const Entity& entity) {
    if (findEntity(entity.id)) {
        throw std::runtime_error("entity id already exists");
    }
    entities_.push_back(entity);
    nextEntityId_ = std::max(nextEntityId_, entity.id + 1);
}

void Document::applyRemoveEntity(EntityId id) {
    const auto it = std::find_if(entities_.begin(), entities_.end(),
                                 [id](const Entity& e) { return e.id == id; });
    if (it == entities_.end()) {
        return;
    }
    selection_.erase(id);
    entities_.erase(it);
}

void Document::applyReplaceEntity(const Entity& entity) {
    Entity* existing = findEntity(entity.id);
    if (!existing) {
        throw std::runtime_error("replace target missing");
    }
    *existing = entity;
}

void Document::applyReplaceLayer(const Layer& layer) {
    Layer* existing = findLayer(layer.id);
    if (!existing) {
        throw std::runtime_error("replace layer target missing");
    }
    *existing = layer;
}

void Document::applySetActiveLayer(LayerId id) {
    if (!findLayer(id)) {
        throw std::runtime_error("active layer target missing");
    }
    activeLayerId_ = id;
}

void Document::restoreSnapshot(std::vector<Layer> layers, std::vector<Entity> entities, LayerId active,
                               EntityId nextEntityId, LayerId nextLayerId, units::UnitSystem units,
                               std::string name, std::vector<BlockDefinition> blocks,
                               double linetypeScale, AttributeDisplayMode attdisp,
                               std::vector<LayerState> layerStates) {
    layers_ = std::move(layers);
    entities_ = std::move(entities);
    blocks_ = std::move(blocks);
    layerStates_ = std::move(layerStates);
    activeLayerId_ = active;
    nextEntityId_ = nextEntityId;
    nextLayerId_ = nextLayerId;
    units_ = units;
    name_ = std::move(name);
    linetypeScale_ = linetypeScale > 0.0 ? linetypeScale : 1.0;
    attdisp_ = attdisp;
    selection_.clear();
    history_.clear();
    dirty_ = false;
    composite_.reset();
}

void Document::markClean() { dirty_ = false; }

EntityId Document::allocateEntityId() { return nextEntityId_++; }

LayerId Document::allocateLayerId() { return nextLayerId_++; }

void Document::commit(Transaction tx) {
    history_.push(std::move(tx));
    dirty_ = true;
}

const Layout* Document::findLayout(const std::string& name) const {
    for (const auto& l : layouts_) {
        if (layerNamesEqual(l.name, name)) {
            return &l;
        }
    }
    return nullptr;
}

Layout* Document::findLayout(const std::string& name) {
    for (auto& l : layouts_) {
        if (layerNamesEqual(l.name, name)) {
            return &l;
        }
    }
    return nullptr;
}

bool Document::setActiveLayout(const std::string& name) {
    const auto* l = findLayout(name);
    if (!l) {
        return false;
    }
    activeLayoutName_ = l->name;
    if (l->isModelSpace) {
        tileMode_ = true;
        isPaperSpace_ = false;
    } else {
        tileMode_ = false;
        isPaperSpace_ = true;
    }
    dirty_ = true;
    return true;
}

bool Document::addLayout(std::string name, const PageSetup& setup) {
    if (name.empty() || findLayout(name)) {
        return false;
    }
    Layout l;
    l.name = std::move(name);
    l.isModelSpace = false;
    l.pageSetup = setup;
    Viewport vp;
    vp.id = nextViewportId_++;
    vp.center = {setup.paperWidthMm * 0.5, setup.paperHeightMm * 0.5};
    vp.width = setup.printableWidthMm;
    vp.height = setup.printableHeightMm;
    vp.viewCenter = {0.0, 0.0};
    vp.viewHeight = 100.0;
    l.viewports.push_back(vp);
    layouts_.push_back(std::move(l));
    dirty_ = true;
    return true;
}

bool Document::removeLayout(const std::string& name) {
    auto it = std::find_if(layouts_.begin(), layouts_.end(),
                           [&](const Layout& l) { return layerNamesEqual(l.name, name); });
    if (it == layouts_.end() || it->isModelSpace) {
        return false;
    }
    if (layerNamesEqual(activeLayoutName_, name)) {
        setActiveLayout("Model");
    }
    layouts_.erase(it);
    dirty_ = true;
    return true;
}

bool Document::renameLayout(const std::string& oldName, const std::string& newName) {
    if (newName.empty() || findLayout(newName)) {
        return false;
    }
    auto* l = findLayout(oldName);
    if (!l || l->isModelSpace) {
        return false;
    }
    l->name = newName;
    if (layerNamesEqual(activeLayoutName_, oldName)) {
        activeLayoutName_ = newName;
    }
    dirty_ = true;
    return true;
}

void Document::setTileMode(bool tm) {
    tileMode_ = tm;
    if (tm) {
        activeLayoutName_ = "Model";
        isPaperSpace_ = false;
    } else {
        for (const auto& l : layouts_) {
            if (!l.isModelSpace) {
                activeLayoutName_ = l.name;
                isPaperSpace_ = true;
                break;
            }
        }
    }
    dirty_ = true;
}

std::uint64_t Document::addViewportToLayout(const std::string& layoutName, const Viewport& vp) {
    auto* l = findLayout(layoutName);
    if (!l || l->isModelSpace) {
        return 0;
    }
    Viewport newVp = vp;
    newVp.id = nextViewportId_++;
    l->viewports.push_back(newVp);
    dirty_ = true;
    return newVp.id;
}

bool Document::setViewportFrozen(const std::string& layoutName, std::uint64_t vpId, LayerId layerId, bool frozen) {
    auto* l = findLayout(layoutName);
    if (!l) {
        return false;
    }
    for (auto& vp : l->viewports) {
        if (vp.id == vpId) {
            if (frozen) {
                vp.frozenLayers.insert(layerId);
            } else {
                vp.frozenLayers.erase(layerId);
            }
            dirty_ = true;
            return true;
        }
    }
    return false;
}

} // namespace mydesk::document
