#pragma once

#include "mydesk/document/entity.hpp"
#include "mydesk/document/history.hpp"
#include "mydesk/geometry/bbox.hpp"
#include "mydesk/units/units.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace mydesk::document {

class Document {
public:
    Document();

    [[nodiscard]] const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    [[nodiscard]] const units::UnitSystem& units() const { return units_; }
    void setUnits(units::UnitSystem units) { units_ = units; }

    [[nodiscard]] LayerId activeLayerId() const { return activeLayerId_; }
    bool setActiveLayer(LayerId id);

    [[nodiscard]] const std::vector<Layer>& layers() const { return layers_; }
    [[nodiscard]] const std::vector<Entity>& entities() const { return entities_; }
    [[nodiscard]] const Layer* findLayer(LayerId id) const;
    [[nodiscard]] Layer* findLayer(LayerId id);
    [[nodiscard]] const Layer* findLayerByName(std::string_view name) const;
    [[nodiscard]] Layer* findLayerByName(std::string_view name);
    [[nodiscard]] const Entity* findEntity(EntityId id) const;
    [[nodiscard]] Entity* findEntity(EntityId id);

    // Creates a layer if the name is unused (case-insensitive). Returns existing id if present.
    LayerId addLayer(std::string name);
    bool setLayerVisible(LayerId id, bool visible);
    bool setLayerLocked(LayerId id, bool locked);
    bool setLayerColor(LayerId id, std::uint32_t colorArgb);
    bool setEntityLayer(EntityId id, LayerId layerId);
    bool setEntityColor(EntityId id, std::uint32_t colorArgb);
    [[nodiscard]] std::uint32_t resolvedEntityColor(const Entity& entity) const;
    EntityId addLine(geometry::Vec2 start, geometry::Vec2 end, std::string txLabel = "LINE");
    EntityId addCircle(geometry::Vec2 center, double radius, std::string txLabel = "CIRCLE");
    EntityId addArc(geometry::Vec2 center, double radius, double startAngle, double endAngle,
                    std::string txLabel = "ARC");
    EntityId addPolyline(std::vector<geometry::Vec2> vertices, bool closed = false,
                         std::string txLabel = "PLINE");
    // Closed rectangular polyline (true PLINE).
    EntityId addRectangle(geometry::Vec2 cornerA, geometry::Vec2 cornerB,
                          std::string txLabel = "RECTANGLE");
    EntityId addDimensionLinear(geometry::Vec2 start, geometry::Vec2 end, geometry::Vec2 dimLine,
                                std::string txLabel = "DIMLINEAR");
    bool removeEntity(EntityId id, std::string txLabel = "ERASE");
    bool replaceEntity(EntityId id, EntityGeometry geometry, std::string txLabel = "MODIFY");

    // Multi-op transaction helpers for modify commands.
    void beginComposite(std::string label);
    void compositeAdd(const Entity& entity);
    void compositeRemove(EntityId id);
    void compositeReplace(const Entity& before, const Entity& after);
    bool commitComposite();
    void abortComposite();

    bool moveEntities(const std::vector<EntityId>& ids, geometry::Vec2 delta,
                      std::string txLabel = "MOVE");
    bool rotateEntities(const std::vector<EntityId>& ids, geometry::Vec2 base, double degrees,
                        std::string txLabel = "ROTATE");
    bool scaleEntities(const std::vector<EntityId>& ids, geometry::Vec2 base, double factor,
                       std::string txLabel = "SCALE");
    std::vector<EntityId> copyEntities(const std::vector<EntityId>& ids, geometry::Vec2 delta,
                                       std::string txLabel = "COPY");

    void select(EntityId id);
    void deselect(EntityId id);
    void toggleSelect(EntityId id);
    void clearSelection();
    void selectInRect(const geometry::BBox& rect, bool crossing, bool additive);
    [[nodiscard]] EntityId hitTestTop(geometry::Vec2 point, double tol) const;
    [[nodiscard]] const std::unordered_set<EntityId>& selection() const { return selection_; }
    [[nodiscard]] std::vector<EntityId> selectionOrdered() const;

    [[nodiscard]] bool canUndo() const { return history_.canUndo(); }
    [[nodiscard]] bool canRedo() const { return history_.canRedo(); }
    bool undo();
    bool redo();

    void applyAddEntity(const Entity& entity);
    void applyRemoveEntity(EntityId id);
    void applyReplaceEntity(const Entity& entity);

    [[nodiscard]] EntityId nextEntityIdPeek() const { return nextEntityId_; }
    [[nodiscard]] LayerId nextLayerIdPeek() const { return nextLayerId_; }
    [[nodiscard]] EntityId allocateEntityIdPublic() { return allocateEntityId(); }

    void restoreSnapshot(std::vector<Layer> layers, std::vector<Entity> entities, LayerId active,
                         EntityId nextEntityId, LayerId nextLayerId, units::UnitSystem units,
                         std::string name);

    void markClean();
    [[nodiscard]] bool isDirty() const { return dirty_; }
    void markDirty() { dirty_ = true; }

private:
    EntityId allocateEntityId();
    LayerId allocateLayerId();
    void commit(Transaction tx);

    std::string name_{"Drawing1"};
    units::UnitSystem units_{};
    std::vector<Layer> layers_;
    std::vector<Entity> entities_;
    LayerId activeLayerId_{kInvalidLayerId};
    EntityId nextEntityId_{1};
    LayerId nextLayerId_{1};
    std::unordered_set<EntityId> selection_;
    HistoryStack history_;
    bool dirty_{false};
    std::optional<Transaction> composite_;
};

} // namespace mydesk::document
