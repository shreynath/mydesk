#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace mydesk::coordinate {

using ModelId = std::uint64_t;
using ElementId = std::uint64_t;
using ClashTestId = std::uint64_t;
using ClashResultId = std::uint64_t;

enum class ClashType {
    Hard,      // Physical geometric penetration
    Clearance, // Proximity within clearance distance
};

enum class ClashStatus {
    New,
    Active,
    Reviewed,
    Approved,
    Resolved,
};

struct BoundingBox3D {
    double minX{0.0};
    double minY{0.0};
    double minZ{0.0};
    double maxX{0.0};
    double maxY{0.0};
    double maxZ{0.0};

    [[nodiscard]] bool intersects(const BoundingBox3D& other) const;
    [[nodiscard]] bool withinDistance(const BoundingBox3D& other, double clearance) const;
    [[nodiscard]] double penetrationDepth(const BoundingBox3D& other) const;
    [[nodiscard]] std::tuple<double, double, double> center() const;
};

struct Vector3D {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

// A rigid oriented box. Axes are an orthonormal local basis and halfExtents are
// measured along those axes. This is the narrow-phase primitive used for
// rotated IFC/NWC-style components; bounds remains its broad-phase AABB.
struct OrientedBox3D {
    Vector3D center;
    Vector3D halfExtents;
    std::array<Vector3D, 3> axes{{{1.0, 0.0, 0.0},
                                  {0.0, 1.0, 0.0},
                                  {0.0, 0.0, 1.0}}};

    [[nodiscard]] static OrientedBox3D fromEuler(Vector3D center, Vector3D halfExtents,
                                                  double yawRadians, double pitchRadians = 0.0,
                                                  double rollRadians = 0.0);
    [[nodiscard]] BoundingBox3D axisAlignedBounds() const;
    [[nodiscard]] bool valid() const;
};

struct CoordinationElement {
    ElementId id{0};
    ModelId modelId{0};
    std::string name;
    std::string category;
    BoundingBox3D bounds;
    std::optional<OrientedBox3D> orientedBounds;
};

struct AggregatedModel {
    ModelId id{0};
    std::string name;
    std::string sourcePath;
    std::string discipline;
    std::vector<CoordinationElement> elements;
};

struct ClashResult {
    ClashResultId id{0};
    ClashTestId testId{0};
    ClashType type{ClashType::Hard};
    ClashStatus status{ClashStatus::New};
    ElementId elementA{0};
    ElementId elementB{0};
    double penetrationOrDistance{0.0};
    double clashX{0.0};
    double clashY{0.0};
    double clashZ{0.0};
    std::string comments;
};

struct ClashTest {
    ClashTestId id{0};
    std::string name;
    ClashType type{ClashType::Hard};
    double tolerance{0.001};
    double clearance{0.05};
    std::vector<ModelId> selectionSetA;
    std::vector<ModelId> selectionSetB;
    std::vector<ClashResult> results;
    struct RunStatistics {
        std::size_t rawPairCount{0};
        std::size_t broadPhaseCandidates{0};
        std::size_t narrowPhaseChecks{0};
    } lastRun;
};

class CoordinateDocument {
public:
    CoordinateDocument();

    [[nodiscard]] const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    ModelId addModel(std::string name, std::string sourcePath, std::string discipline);
    ElementId addElementToModel(ModelId modelId, std::string name, std::string category,
                                const BoundingBox3D& bounds);
    ElementId addOrientedElementToModel(ModelId modelId, std::string name, std::string category,
                                        const OrientedBox3D& bounds);
    [[nodiscard]] const AggregatedModel* findModel(ModelId id) const;
    [[nodiscard]] const CoordinationElement* findElement(ElementId id) const;
    [[nodiscard]] const std::vector<AggregatedModel>& models() const { return models_; }

    ClashTestId createClashTest(std::string name, ClashType type, double tolerance = 0.001,
                                double clearance = 0.05);
    bool setClashTestSets(ClashTestId testId, std::vector<ModelId> setA, std::vector<ModelId> setB);
    std::size_t runClashTest(ClashTestId testId);
    bool setClashResultReview(ClashTestId testId, ClashResultId resultId, ClashStatus status,
                              std::string comments);
    bool undo();
    bool redo();
    [[nodiscard]] bool canUndo() const { return !undoHistory_.empty(); }
    [[nodiscard]] bool canRedo() const { return !redoHistory_.empty(); }
    [[nodiscard]] const ClashTest* findClashTest(ClashTestId testId) const;

    [[nodiscard]] std::string exportBcfXml() const;
    [[nodiscard]] std::string serializeNative() const;
    static CoordinateDocument deserializeNative(std::string_view json, std::string* error = nullptr);

private:
    std::string name_{"CoordinationSession1"};
    std::vector<AggregatedModel> models_;
    std::vector<ClashTest> clashTests_;
    ModelId nextModelId_{1};
    ElementId nextElementId_{1};
    ClashTestId nextTestId_{1};
    ClashResultId nextResultId_{1};

    struct ReviewChange {
        ClashTestId testId{0};
        ClashResultId resultId{0};
        ClashStatus beforeStatus{ClashStatus::New};
        ClashStatus afterStatus{ClashStatus::New};
        std::string beforeComments;
        std::string afterComments;
    };
    std::vector<ReviewChange> undoHistory_;
    std::vector<ReviewChange> redoHistory_;
};

} // namespace mydesk::coordinate
