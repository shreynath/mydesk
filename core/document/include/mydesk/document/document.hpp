#pragma once

#include "mydesk/document/entity.hpp"
#include "mydesk/document/history.hpp"
#include "mydesk/document/layout.hpp"
#include "mydesk/geometry/bbox.hpp"
#include "mydesk/units/units.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace mydesk::document {

struct ExplodeResult {
    std::vector<EntityId> created;
    std::size_t exploded{0};
    std::size_t skipped{0};
};

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
    [[nodiscard]] const std::vector<LayerState>& layerStates() const { return layerStates_; }
    [[nodiscard]] const std::vector<Entity>& entities() const { return entities_; }
    [[nodiscard]] const std::vector<BlockDefinition>& blocks() const { return blocks_; }
    [[nodiscard]] const BlockDefinition* findBlock(std::string_view name) const;
    [[nodiscard]] BlockDefinition* findBlock(std::string_view name);
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
    bool setLayerFrozen(LayerId id, bool frozen);
    bool setLayerPlottable(LayerId id, bool plottable);
    bool setLayerLinetype(LayerId id, Linetype linetype);
    bool setLayerLineweight(LayerId id, double millimeters);
    bool setLayerTransparency(LayerId id, float transparency);
    bool setLayerThickness(LayerId id, double thickness);
    // Saves or replaces a case-insensitively named snapshot. Restore affects
    // layers present in both the snapshot and drawing and is one undoable edit.
    bool saveLayerState(std::string name);
    bool restoreLayerState(std::string_view name);
    bool deleteLayerState(std::string_view name);
    [[nodiscard]] const LayerState* findLayerState(std::string_view name) const;
    [[nodiscard]] double linetypeScale() const { return linetypeScale_; }
    void setLinetypeScale(double scale);
    [[nodiscard]] bool isEntityVisible(const Entity& entity) const;
    [[nodiscard]] bool isEntitySelectable(const Entity& entity) const;
    bool setLayerColor(LayerId id, std::uint32_t colorArgb);
    bool setEntityLayer(EntityId id, LayerId layerId);
    bool setEntityColor(EntityId id, std::uint32_t colorArgb);
    bool setEntityLinetype(EntityId id, Linetype linetype);
    bool setEntityLineweight(EntityId id, double millimeters);
    bool setEntityLineweightByLayer(EntityId id);
    bool setEntityTransparency(EntityId id, float transparency);
    bool setEntityTransparencyByLayer(EntityId id);
    bool setEntityThickness(EntityId id, double thickness);
    bool setEntityThicknessByLayer(EntityId id);
    [[nodiscard]] std::uint32_t resolvedEntityColor(const Entity& entity) const;
    [[nodiscard]] Linetype resolvedLinetype(const Entity& entity) const;
    [[nodiscard]] double resolvedLineweightMm(const Entity& entity) const;
    [[nodiscard]] float resolvedTransparency(const Entity& entity) const;
    [[nodiscard]] double resolvedThickness(const Entity& entity) const;
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
    // Creates/replaces a block from selection (geometries relative to base). Removes source entities.
    bool defineBlockFromSelection(std::string name, geometry::Vec2 base,
                                  std::string txLabel = "BLOCK");
    EntityId addInsert(std::string blockName, geometry::Vec2 insertion, double scale = 1.0,
                       double rotationDeg = 0.0,
                       const std::unordered_map<std::string, std::string>& attributeOverrides = {},
                       std::string txLabel = "INSERT");
    EntityId addAttdef(geometry::Vec2 position, double height, std::string tag, std::string prompt,
                       std::string defaultValue, double rotationDeg = 0.0, bool invisible = false,
                       std::uint32_t flags = 0, bool isMultiline = false,
                       std::string txLabel = "ATTDEF");
    bool editInsertAttribute(EntityId id, const std::string& tag, const std::string& newValue,
                             std::string txLabel = "ATTEDIT");
    [[nodiscard]] AttributeDisplayMode attributeDisplayMode() const { return attdisp_; }
    void setAttributeDisplayMode(AttributeDisplayMode mode);
    EntityId addText(geometry::Vec2 position, double height, std::string contents,
                     double rotationDeg = 0.0, std::string txLabel = "TEXT");
    EntityId addHatch(std::vector<geometry::Vec2> boundary, std::uint32_t fillArgb = 0x6688AACC,
                      std::string txLabel = "HATCH");
    // One level: INSERT becomes its transformed primitives; POLYLINE becomes lines.
    ExplodeResult explodeEntities(const std::vector<EntityId>& ids, std::string txLabel = "EXPLODE");
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
    void applyReplaceLayer(const Layer& layer);
    void applySetActiveLayer(LayerId id);

    [[nodiscard]] EntityId nextEntityIdPeek() const { return nextEntityId_; }
    [[nodiscard]] LayerId nextLayerIdPeek() const { return nextLayerId_; }
    [[nodiscard]] EntityId allocateEntityIdPublic() { return allocateEntityId(); }

    void restoreSnapshot(std::vector<Layer> layers, std::vector<Entity> entities, LayerId active,
                         EntityId nextEntityId, LayerId nextLayerId, units::UnitSystem units,
                         std::string name, std::vector<BlockDefinition> blocks = {},
                         double linetypeScale = 1.0,
                         AttributeDisplayMode attdisp = AttributeDisplayMode::Normal,
                         std::vector<LayerState> layerStates = {});

    void markClean();
    [[nodiscard]] bool isDirty() const { return dirty_; }
    void markDirty() { dirty_ = true; }

    // Layout and Paper Space
    [[nodiscard]] const std::vector<Layout>& layouts() const { return layouts_; }
    [[nodiscard]] const Layout* findLayout(const std::string& name) const;
    [[nodiscard]] Layout* findLayout(const std::string& name);
    [[nodiscard]] const std::string& activeLayoutName() const { return activeLayoutName_; }
    bool setActiveLayout(const std::string& name);
    bool addLayout(std::string name, const PageSetup& setup = {});
    bool removeLayout(const std::string& name);
    bool renameLayout(const std::string& oldName, const std::string& newName);
    [[nodiscard]] bool tileMode() const { return tileMode_; }
    void setTileMode(bool tm);
    [[nodiscard]] bool isPaperSpace() const { return isPaperSpace_; }
    void setPaperSpace(bool ps) { isPaperSpace_ = ps; dirty_ = true; }
    std::uint64_t addViewportToLayout(const std::string& layoutName, const Viewport& vp);
    bool setViewportFrozen(const std::string& layoutName, std::uint64_t vpId, LayerId layerId, bool frozen);

private:
    EntityId allocateEntityId();
    LayerId allocateLayerId();
    void commit(Transaction tx);

    std::string name_{"Drawing1"};
    units::UnitSystem units_{};
    std::vector<Layer> layers_;
    std::vector<LayerState> layerStates_;
    std::vector<Entity> entities_;
    std::vector<BlockDefinition> blocks_;
    std::vector<Layout> layouts_;
    std::string activeLayoutName_{"Model"};
    bool tileMode_{true};
    bool isPaperSpace_{false};
    std::uint64_t nextViewportId_{1};
    LayerId activeLayerId_{kInvalidLayerId};
    EntityId nextEntityId_{1};
    LayerId nextLayerId_{1};
    std::unordered_set<EntityId> selection_;
    HistoryStack history_;
    bool dirty_{false};
    double linetypeScale_{1.0};
    AttributeDisplayMode attdisp_{AttributeDisplayMode::Normal};
    std::optional<Transaction> composite_;
};

} // namespace mydesk::document
