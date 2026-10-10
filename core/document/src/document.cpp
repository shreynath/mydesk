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

std::uint32_t Document::resolvedEntityColor(const Entity& entity) const {
    if (entity.colorArgb != kColorByLayer) {
        return entity.colorArgb;
    }
    if (const Layer* layer = findLayer(entity.layerId)) {
        return layer->colorArgb;
    }
    return 0xFFE0E0E0u;
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

EntityId Document::addDimensionLinear(geometry::Vec2 start, geometry::Vec2 end,
                                      geometry::Vec2 dimLine, std::string txLabel) {
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
    // Not a full rollback helper — callers should only use after failed validation before mutates.
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
        const auto* layer = findLayer(entity.layerId);
        if (layer && (!layer->visible || layer->locked)) {
            continue;
        }
        if (entity.intersectsRect(rect, crossing)) {
            selection_.insert(entity.id);
        }
    }
}

EntityId Document::hitTestTop(geometry::Vec2 point, double tol) const {
    for (auto it = entities_.rbegin(); it != entities_.rend(); ++it) {
        const auto* layer = findLayer(it->layerId);
        if (layer && (!layer->visible || layer->locked)) {
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

void Document::restoreSnapshot(std::vector<Layer> layers, std::vector<Entity> entities,
                               LayerId active, EntityId nextEntityId, LayerId nextLayerId,
                               units::UnitSystem units, std::string name) {
    layers_ = std::move(layers);
    entities_ = std::move(entities);
    activeLayerId_ = active;
    nextEntityId_ = nextEntityId;
    nextLayerId_ = nextLayerId;
    units_ = units;
    name_ = std::move(name);
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

} // namespace mydesk::document
