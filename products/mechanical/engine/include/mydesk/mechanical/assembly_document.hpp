#pragma once

#include "mydesk/mechanical/part_document.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <TopoDS_Shape.hxx>

namespace mydesk::mechanical {

using OccurrenceId = std::uint64_t;
using ConstraintId = std::uint64_t;
using JointId = std::uint64_t;

enum class ConstraintType {
    Mate,
    Flush,
    Insert,
};

struct ComponentOccurrence {
    OccurrenceId id{0};
    std::string name;
    PartDocument part;
    double x{0.0};
    double y{0.0};
    double z{0.0};
    double rotationAxisX{0.0};
    double rotationAxisY{0.0};
    double rotationAxisZ{1.0};
    double rotationDeg{0.0};
    bool isGrounded{false};
};

struct AssemblyConstraint {
    ConstraintId id{0};
    ConstraintType type{ConstraintType::Mate};
    std::string name;
    OccurrenceId occurrenceA{0};
    OccurrenceId occurrenceB{0};
    double offset{0.0};
};

enum class JointType {
    Rigid,
    Revolute,
    Slider,
};

struct AssemblyJoint {
    JointId id{0};
    JointType type{JointType::Rigid};
    std::string name;
    OccurrenceId parentOccurrence{0};
    OccurrenceId childOccurrence{0};
    double axisX{0.0};
    double axisY{0.0};
    double axisZ{1.0};
    double offsetX{0.0};
    double offsetY{0.0};
    double offsetZ{0.0};
    double position{0.0}; // degrees for revolute, millimetres for slider
    double minimum{0.0};
    double maximum{0.0};
};

struct BOMEntry {
    std::string partName;
    std::size_t count{0};
    double unitVolume{0.0};
    double totalVolume{0.0};
};

struct AssemblySolveReport {
    bool ok{false};
    std::string message;
    std::size_t occurrencesSolved{0};
    std::size_t constraintsEvaluated{0};
    std::size_t jointsEvaluated{0};
};

class AssemblyDocument {
public:
    AssemblyDocument();

    [[nodiscard]] const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    OccurrenceId addOccurrence(std::string name, PartDocument part, double x = 0.0,
                               double y = 0.0, double z = 0.0, bool isGrounded = false);

    ConstraintId addConstraint(ConstraintType type, std::string name, OccurrenceId occurrenceA,
                               OccurrenceId occurrenceB, double offset = 0.0);
    JointId addJoint(JointType type, std::string name, OccurrenceId parentOccurrence,
                     OccurrenceId childOccurrence, double axisX, double axisY, double axisZ,
                     double offsetX, double offsetY, double offsetZ,
                     double minimum, double maximum, double position = 0.0);
    bool setJointPosition(JointId id, double position);

    [[nodiscard]] const ComponentOccurrence* findOccurrence(OccurrenceId id) const;
    [[nodiscard]] ComponentOccurrence* findOccurrence(OccurrenceId id);
    [[nodiscard]] const std::vector<ComponentOccurrence>& occurrences() const { return occurrences_; }
    [[nodiscard]] const std::vector<AssemblyConstraint>& constraints() const { return constraints_; }
    [[nodiscard]] const AssemblyJoint* findJoint(JointId id) const;
    [[nodiscard]] const std::vector<AssemblyJoint>& joints() const { return joints_; }

    [[nodiscard]] bool canUndo() const { return !undoStack_.empty(); }
    [[nodiscard]] bool canRedo() const { return !redoStack_.empty(); }
    bool undo();
    bool redo();

    AssemblySolveReport solve();
    [[nodiscard]] TopoDS_Shape buildAssemblyShape() const;
    [[nodiscard]] double totalVolume() const;
    [[nodiscard]] std::vector<BOMEntry> generateBOM() const;

    bool exportStep(const std::string& path, std::string* error = nullptr) const;

    [[nodiscard]] std::string serializeNative() const;
    static AssemblyDocument deserializeNative(std::string_view json, std::string* error = nullptr);

private:
    struct JointEdit {
        JointId jointId{0};
        double before{0.0};
        double after{0.0};
    };

    [[nodiscard]] AssemblyJoint* findJointMutable(JointId id);

    std::string name_{"Assembly1"};
    std::vector<ComponentOccurrence> occurrences_;
    std::vector<AssemblyConstraint> constraints_;
    std::vector<AssemblyJoint> joints_;
    OccurrenceId nextOccurrenceId_{1};
    ConstraintId nextConstraintId_{1};
    JointId nextJointId_{1};
    std::vector<JointEdit> undoStack_;
    std::vector<JointEdit> redoStack_;
};

} // namespace mydesk::mechanical
