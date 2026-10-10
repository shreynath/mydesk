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

    [[nodiscard]] const ComponentOccurrence* findOccurrence(OccurrenceId id) const;
    [[nodiscard]] ComponentOccurrence* findOccurrence(OccurrenceId id);
    [[nodiscard]] const std::vector<ComponentOccurrence>& occurrences() const { return occurrences_; }
    [[nodiscard]] const std::vector<AssemblyConstraint>& constraints() const { return constraints_; }

    AssemblySolveReport solve();
    [[nodiscard]] TopoDS_Shape buildAssemblyShape() const;
    [[nodiscard]] double totalVolume() const;
    [[nodiscard]] std::vector<BOMEntry> generateBOM() const;

    bool exportStep(const std::string& path, std::string* error = nullptr) const;

    [[nodiscard]] std::string serializeNative() const;
    static AssemblyDocument deserializeNative(std::string_view json, std::string* error = nullptr);

private:
    std::string name_{"Assembly1"};
    std::vector<ComponentOccurrence> occurrences_;
    std::vector<AssemblyConstraint> constraints_;
    OccurrenceId nextOccurrenceId_{1};
    ConstraintId nextConstraintId_{1};
};

} // namespace mydesk::mechanical
