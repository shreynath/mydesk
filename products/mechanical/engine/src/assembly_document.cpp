#include "mydesk/mechanical/assembly_document.hpp"

#include <BRepBuilderAPI_Transform.hxx>
#include <BRep_Builder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Writer.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace mydesk::mechanical {

namespace {

std::string escapeJson(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

std::size_t skipWhitespace(std::string_view text, std::size_t pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
    return pos;
}

std::size_t memberValuePosition(std::string_view object, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto keyPos = object.find(needle);
    if (keyPos == std::string_view::npos) return keyPos;
    const auto colon = object.find(':', keyPos + needle.size());
    return colon == std::string_view::npos ? colon : skipWhitespace(object, colon + 1);
}

bool parseStringAt(std::string_view text, std::size_t pos, std::string& value) {
    pos = skipWhitespace(text, pos);
    if (pos >= text.size() || text[pos] != '"') return false;
    value.clear();
    bool escaped = false;
    for (++pos; pos < text.size(); ++pos) {
        const char c = text[pos];
        if (escaped) {
            value.push_back(c);
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            return true;
        } else {
            value.push_back(c);
        }
    }
    return false;
}

bool parseMemberString(std::string_view object, std::string_view key, std::string& value) {
    const auto pos = memberValuePosition(object, key);
    return pos != std::string_view::npos && parseStringAt(object, pos, value);
}

bool parseMemberNumber(std::string_view object, std::string_view key, double& value) {
    const auto pos = memberValuePosition(object, key);
    if (pos == std::string_view::npos) return false;
    std::size_t consumed = 0;
    try {
        value = std::stod(std::string(object.substr(pos)), &consumed);
    } catch (...) {
        return false;
    }
    return consumed > 0 && std::isfinite(value);
}

bool parseMemberBool(std::string_view object, std::string_view key, bool& value) {
    const auto pos = memberValuePosition(object, key);
    if (pos == std::string_view::npos) return false;
    if (object.substr(pos, 4) == "true") { value = true; return true; }
    if (object.substr(pos, 5) == "false") { value = false; return true; }
    return false;
}

std::size_t matchingDelimiter(std::string_view text, std::size_t openPos,
                              char open, char close) {
    if (openPos >= text.size() || text[openPos] != open) return std::string_view::npos;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t pos = openPos; pos < text.size(); ++pos) {
        const char c = text[pos];
        if (inString) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') inString = false;
            continue;
        }
        if (c == '"') inString = true;
        else if (c == open) ++depth;
        else if (c == close && --depth == 0) return pos;
    }
    return std::string_view::npos;
}

bool memberDelimited(std::string_view object, std::string_view key,
                     char open, char close, std::string_view& value) {
    const auto pos = memberValuePosition(object, key);
    if (pos == std::string_view::npos || pos >= object.size() || object[pos] != open) return false;
    const auto end = matchingDelimiter(object, pos, open, close);
    if (end == std::string_view::npos) return false;
    value = object.substr(pos, end - pos + 1);
    return true;
}

std::vector<std::string_view> arrayObjects(std::string_view arrayWithBrackets) {
    std::vector<std::string_view> objects;
    if (arrayWithBrackets.size() < 2) return objects;
    std::size_t pos = 1;
    const std::size_t limit = arrayWithBrackets.size() - 1;
    while ((pos = skipWhitespace(arrayWithBrackets, pos)) < limit) {
        if (arrayWithBrackets[pos] != '{') return {};
        const auto end = matchingDelimiter(arrayWithBrackets, pos, '{', '}');
        if (end == std::string_view::npos || end >= limit) return {};
        objects.push_back(arrayWithBrackets.substr(pos, end - pos + 1));
        pos = skipWhitespace(arrayWithBrackets, end + 1);
        if (pos >= limit) break;
        if (arrayWithBrackets[pos] != ',') return {};
        ++pos;
    }
    return objects;
}

const char* jointTypeName(JointType type) {
    switch (type) {
        case JointType::Rigid: return "Rigid";
        case JointType::Revolute: return "Revolute";
        case JointType::Slider: return "Slider";
    }
    return "Rigid";
}

bool jointTypeFromName(std::string_view name, JointType& type) {
    if (name == "Rigid") type = JointType::Rigid;
    else if (name == "Revolute") type = JointType::Revolute;
    else if (name == "Slider") type = JointType::Slider;
    else return false;
    return true;
}

} // namespace

AssemblyDocument::AssemblyDocument() = default;

OccurrenceId AssemblyDocument::addOccurrence(std::string name, PartDocument part, double x,
                                             double y, double z, bool isGrounded) {
    ComponentOccurrence occ;
    occ.id = nextOccurrenceId_++;
    occ.name = std::move(name);
    occ.part = std::move(part);
    occ.x = x;
    occ.y = y;
    occ.z = z;
    occ.isGrounded = isGrounded;
    occurrences_.push_back(std::move(occ));
    return occurrences_.back().id;
}

ConstraintId AssemblyDocument::addConstraint(ConstraintType type, std::string name,
                                             OccurrenceId occurrenceA, OccurrenceId occurrenceB,
                                             double offset) {
    AssemblyConstraint c;
    c.id = nextConstraintId_++;
    c.type = type;
    c.name = std::move(name);
    c.occurrenceA = occurrenceA;
    c.occurrenceB = occurrenceB;
    c.offset = offset;
    constraints_.push_back(std::move(c));
    return constraints_.back().id;
}

JointId AssemblyDocument::addJoint(JointType type, std::string name,
                                   OccurrenceId parentOccurrence,
                                   OccurrenceId childOccurrence,
                                   double axisX, double axisY, double axisZ,
                                   double offsetX, double offsetY, double offsetZ,
                                   double minimum, double maximum, double position) {
    if (parentOccurrence == childOccurrence || !findOccurrence(parentOccurrence) ||
        !findOccurrence(childOccurrence)) {
        throw std::invalid_argument("joint occurrences must exist and be distinct");
    }
    const double axisLength = std::sqrt(axisX * axisX + axisY * axisY + axisZ * axisZ);
    if (!(axisLength > 1e-12) || !std::isfinite(axisLength)) {
        throw std::invalid_argument("joint axis must be finite and non-zero");
    }
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum ||
        !std::isfinite(position) || position < minimum || position > maximum) {
        throw std::invalid_argument("joint position must lie within valid limits");
    }

    AssemblyJoint joint;
    joint.id = nextJointId_++;
    joint.type = type;
    joint.name = std::move(name);
    joint.parentOccurrence = parentOccurrence;
    joint.childOccurrence = childOccurrence;
    joint.axisX = axisX / axisLength;
    joint.axisY = axisY / axisLength;
    joint.axisZ = axisZ / axisLength;
    joint.offsetX = offsetX;
    joint.offsetY = offsetY;
    joint.offsetZ = offsetZ;
    joint.minimum = minimum;
    joint.maximum = maximum;
    joint.position = position;
    joints_.push_back(std::move(joint));
    return joints_.back().id;
}

bool AssemblyDocument::setJointPosition(JointId id, double position) {
    auto* joint = findJointMutable(id);
    if (!joint || !std::isfinite(position) || position < joint->minimum ||
        position > joint->maximum) {
        return false;
    }
    if (std::abs(joint->position - position) < 1e-12) return true;
    undoStack_.push_back({id, joint->position, position});
    redoStack_.clear();
    joint->position = position;
    return true;
}

const ComponentOccurrence* AssemblyDocument::findOccurrence(OccurrenceId id) const {
    for (const auto& occ : occurrences_) {
        if (occ.id == id) {
            return &occ;
        }
    }
    return nullptr;
}

ComponentOccurrence* AssemblyDocument::findOccurrence(OccurrenceId id) {
    for (auto& occ : occurrences_) {
        if (occ.id == id) {
            return &occ;
        }
    }
    return nullptr;
}

const AssemblyJoint* AssemblyDocument::findJoint(JointId id) const {
    for (const auto& joint : joints_) {
        if (joint.id == id) return &joint;
    }
    return nullptr;
}

AssemblyJoint* AssemblyDocument::findJointMutable(JointId id) {
    for (auto& joint : joints_) {
        if (joint.id == id) return &joint;
    }
    return nullptr;
}

bool AssemblyDocument::undo() {
    if (undoStack_.empty()) return false;
    const JointEdit edit = undoStack_.back();
    undoStack_.pop_back();
    auto* joint = findJointMutable(edit.jointId);
    if (!joint) return false;
    joint->position = edit.before;
    redoStack_.push_back(edit);
    return true;
}

bool AssemblyDocument::redo() {
    if (redoStack_.empty()) return false;
    const JointEdit edit = redoStack_.back();
    redoStack_.pop_back();
    auto* joint = findJointMutable(edit.jointId);
    if (!joint) return false;
    joint->position = edit.after;
    undoStack_.push_back(edit);
    return true;
}

AssemblySolveReport AssemblyDocument::solve() {
    AssemblySolveReport report;
    report.ok = true;
    report.occurrencesSolved = occurrences_.size();

    for (const auto& c : constraints_) {
        auto* occA = findOccurrence(c.occurrenceA);
        auto* occB = findOccurrence(c.occurrenceB);
        if (!occA || !occB) {
            continue;
        }
        ++report.constraintsEvaluated;
        if (occB->isGrounded) {
            continue;
        }

        switch (c.type) {
        case ConstraintType::Mate:
            // Mate positions occurrenceB adjacent to occurrenceA with offset along Z.
            occB->x = occA->x;
            occB->y = occA->y;
            occB->z = occA->z + c.offset;
            break;
        case ConstraintType::Flush:
            // Flush aligns occurrenceB to the exact plane coordinate of occurrenceA plus offset.
            occB->x = occA->x + c.offset;
            occB->y = occA->y;
            occB->z = occA->z;
            break;
        case ConstraintType::Insert:
            // Insert aligns center axis and applies offset.
            occB->x = occA->x;
            occB->y = occA->y;
            occB->z = occA->z + c.offset;
            break;
        }
    }

    for (const auto& joint : joints_) {
        auto* parent = findOccurrence(joint.parentOccurrence);
        auto* child = findOccurrence(joint.childOccurrence);
        if (!parent || !child) {
            report.ok = false;
            report.message = "joint references a missing occurrence";
            return report;
        }
        if (child->isGrounded) {
            report.ok = false;
            report.message = "joint cannot drive a grounded child occurrence";
            return report;
        }
        ++report.jointsEvaluated;
        child->x = parent->x + joint.offsetX;
        child->y = parent->y + joint.offsetY;
        child->z = parent->z + joint.offsetZ;

        switch (joint.type) {
        case JointType::Rigid:
            child->rotationAxisX = parent->rotationAxisX;
            child->rotationAxisY = parent->rotationAxisY;
            child->rotationAxisZ = parent->rotationAxisZ;
            child->rotationDeg = parent->rotationDeg;
            break;
        case JointType::Revolute:
            child->rotationAxisX = joint.axisX;
            child->rotationAxisY = joint.axisY;
            child->rotationAxisZ = joint.axisZ;
            child->rotationDeg = joint.position;
            break;
        case JointType::Slider:
            child->x += joint.axisX * joint.position;
            child->y += joint.axisY * joint.position;
            child->z += joint.axisZ * joint.position;
            child->rotationAxisX = parent->rotationAxisX;
            child->rotationAxisY = parent->rotationAxisY;
            child->rotationAxisZ = parent->rotationAxisZ;
            child->rotationDeg = parent->rotationDeg;
            break;
        }
    }

    report.message = "assembly constraints and joints solved";
    return report;
}

TopoDS_Shape AssemblyDocument::buildAssemblyShape() const {
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);

    for (const auto& occ : occurrences_) {
        const TopoDS_Shape& partShape = occ.part.shape();
        if (partShape.IsNull()) {
            continue;
        }
        constexpr double pi = 3.141592653589793238462643383279502884;
        gp_Trsf trsf;
        const double axisLength = std::sqrt(occ.rotationAxisX * occ.rotationAxisX +
                                            occ.rotationAxisY * occ.rotationAxisY +
                                            occ.rotationAxisZ * occ.rotationAxisZ);
        if (std::abs(occ.rotationDeg) > 1e-12 && axisLength > 1e-12) {
            trsf.SetRotation(gp_Ax1(gp_Pnt(0.0, 0.0, 0.0),
                                    gp_Dir(occ.rotationAxisX, occ.rotationAxisY,
                                           occ.rotationAxisZ)),
                             occ.rotationDeg * (pi / 180.0));
        }
        trsf.SetTranslationPart(gp_Vec(occ.x, occ.y, occ.z));
        BRepBuilderAPI_Transform transformer(partShape, trsf, Standard_True);
        if (transformer.IsDone() && !transformer.Shape().IsNull()) {
            builder.Add(compound, transformer.Shape());
        }
    }
    return compound;
}

double AssemblyDocument::totalVolume() const {
    double total = 0.0;
    for (const auto& occ : occurrences_) {
        total += occ.part.solidVolume();
    }
    return total;
}

std::vector<BOMEntry> AssemblyDocument::generateBOM() const {
    std::unordered_map<std::string, BOMEntry> table;
    for (const auto& occ : occurrences_) {
        const std::string& pName = occ.part.name();
        auto& entry = table[pName];
        entry.partName = pName;
        ++entry.count;
        entry.unitVolume = occ.part.solidVolume();
        entry.totalVolume += occ.part.solidVolume();
    }

    std::vector<BOMEntry> out;
    out.reserve(table.size());
    for (auto& [_, row] : table) {
        out.push_back(std::move(row));
    }
    return out;
}

bool AssemblyDocument::exportStep(const std::string& path, std::string* error) const {
    const TopoDS_Shape shape = buildAssemblyShape();
    if (shape.IsNull()) {
        if (error) {
            *error = "no assembly geometry to export";
        }
        return false;
    }
    STEPControl_Writer writer;
    const IFSelect_ReturnStatus status = writer.Transfer(shape, STEPControl_AsIs);
    if (status != IFSelect_RetDone) {
        if (error) {
            *error = "assembly STEP transfer failed";
        }
        return false;
    }
    if (writer.Write(path.c_str()) != IFSelect_RetDone) {
        if (error) {
            *error = "assembly STEP write failed";
        }
        return false;
    }
    return true;
}

std::string AssemblyDocument::serializeNative() const {
    std::ostringstream oss;
    oss << std::setprecision(17);
    oss << "{\n";
    oss << "  \"format\": \"mydesk-mechanical-assembly\",\n";
    oss << "  \"schemaVersion\": 2,\n";
    oss << "  \"name\": \"" << escapeJson(name_) << "\",\n";
    oss << "  \"occurrences\": [\n";
    for (std::size_t i = 0; i < occurrences_.size(); ++i) {
        const auto& occ = occurrences_[i];
        oss << "    {\"id\": " << occ.id << ", \"name\": \"" << escapeJson(occ.name)
            << "\", \"x\": " << occ.x << ", \"y\": " << occ.y << ", \"z\": " << occ.z
            << ", \"axisX\": " << occ.rotationAxisX
            << ", \"axisY\": " << occ.rotationAxisY
            << ", \"axisZ\": " << occ.rotationAxisZ
            << ", \"rotationDeg\": " << occ.rotationDeg
            << ", \"isGrounded\": " << (occ.isGrounded ? "true" : "false")
            << ", \"part\": " << occ.part.serializeNative() << "}";
        if (i + 1 < occurrences_.size()) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << "  ],\n";
    oss << "  \"constraints\": [\n";
    for (std::size_t i = 0; i < constraints_.size(); ++i) {
        const auto& c = constraints_[i];
        std::string typeStr = "Mate";
        if (c.type == ConstraintType::Flush) {
            typeStr = "Flush";
        } else if (c.type == ConstraintType::Insert) {
            typeStr = "Insert";
        }
        oss << "    {\"id\": " << c.id << ", \"type\": \"" << typeStr
            << "\", \"name\": \"" << escapeJson(c.name)
            << "\", \"occA\": " << c.occurrenceA << ", \"occB\": " << c.occurrenceB
            << ", \"offset\": " << c.offset << "}";
        if (i + 1 < constraints_.size()) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << "  ],\n";
    oss << "  \"joints\": [\n";
    for (std::size_t i = 0; i < joints_.size(); ++i) {
        const auto& joint = joints_[i];
        oss << "    {\"id\": " << joint.id << ", \"type\": \""
            << jointTypeName(joint.type) << "\", \"name\": \"" << escapeJson(joint.name)
            << "\", \"parent\": " << joint.parentOccurrence
            << ", \"child\": " << joint.childOccurrence
            << ", \"axisX\": " << joint.axisX << ", \"axisY\": " << joint.axisY
            << ", \"axisZ\": " << joint.axisZ << ", \"offsetX\": " << joint.offsetX
            << ", \"offsetY\": " << joint.offsetY << ", \"offsetZ\": " << joint.offsetZ
            << ", \"position\": " << joint.position << ", \"minimum\": " << joint.minimum
            << ", \"maximum\": " << joint.maximum << "}";
        if (i + 1 < joints_.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n}\n";
    return oss.str();
}

AssemblyDocument AssemblyDocument::deserializeNative(std::string_view json, std::string* error) {
    AssemblyDocument doc;
    if (error) error->clear();
    std::string format;
    if (!parseMemberString(json, "format", format) || format != "mydesk-mechanical-assembly") {
        if (error) *error = "not a mydesk-mechanical-assembly document";
        return doc;
    }
    double schemaVersion = 0.0;
    if (!parseMemberNumber(json, "schemaVersion", schemaVersion) || schemaVersion != 2.0 ||
        !parseMemberString(json, "name", doc.name_)) {
        if (error) *error = "unsupported or malformed assembly schema";
        return AssemblyDocument{};
    }

    auto fail = [&](std::string message) {
        if (error) *error = std::move(message);
        return AssemblyDocument{};
    };

    std::string_view occurrenceArray;
    std::string_view constraintArray;
    std::string_view jointArray;
    if (!memberDelimited(json, "occurrences", '[', ']', occurrenceArray) ||
        !memberDelimited(json, "constraints", '[', ']', constraintArray) ||
        !memberDelimited(json, "joints", '[', ']', jointArray)) {
        return fail("assembly entity arrays are missing or malformed");
    }

    for (const auto object : arrayObjects(occurrenceArray)) {
        ComponentOccurrence occurrence;
        double id = 0.0;
        std::string_view partJson;
        if (!parseMemberNumber(object, "id", id) || id < 1.0 || std::floor(id) != id ||
            !parseMemberString(object, "name", occurrence.name) ||
            !parseMemberNumber(object, "x", occurrence.x) ||
            !parseMemberNumber(object, "y", occurrence.y) ||
            !parseMemberNumber(object, "z", occurrence.z) ||
            !parseMemberNumber(object, "axisX", occurrence.rotationAxisX) ||
            !parseMemberNumber(object, "axisY", occurrence.rotationAxisY) ||
            !parseMemberNumber(object, "axisZ", occurrence.rotationAxisZ) ||
            !parseMemberNumber(object, "rotationDeg", occurrence.rotationDeg) ||
            !parseMemberBool(object, "isGrounded", occurrence.isGrounded) ||
            !memberDelimited(object, "part", '{', '}', partJson)) {
            return fail("invalid occurrence record");
        }
        occurrence.id = static_cast<OccurrenceId>(id);
        std::string partError;
        occurrence.part = PartDocument::deserializeNative(partJson, &partError);
        if (!partError.empty() || !occurrence.part.hasSolid()) {
            return fail("invalid occurrence part: " + partError);
        }
        doc.nextOccurrenceId_ = std::max(doc.nextOccurrenceId_, occurrence.id + 1);
        doc.occurrences_.push_back(std::move(occurrence));
    }

    for (const auto object : arrayObjects(constraintArray)) {
        AssemblyConstraint constraint;
        double id = 0.0, occurrenceA = 0.0, occurrenceB = 0.0;
        std::string type;
        if (!parseMemberNumber(object, "id", id) || id < 1.0 || std::floor(id) != id ||
            !parseMemberString(object, "type", type) ||
            !parseMemberString(object, "name", constraint.name) ||
            !parseMemberNumber(object, "occA", occurrenceA) ||
            !parseMemberNumber(object, "occB", occurrenceB) ||
            !parseMemberNumber(object, "offset", constraint.offset)) {
            return fail("invalid assembly constraint record");
        }
        if (type == "Mate") constraint.type = ConstraintType::Mate;
        else if (type == "Flush") constraint.type = ConstraintType::Flush;
        else if (type == "Insert") constraint.type = ConstraintType::Insert;
        else return fail("unknown assembly constraint type");
        constraint.id = static_cast<ConstraintId>(id);
        constraint.occurrenceA = static_cast<OccurrenceId>(occurrenceA);
        constraint.occurrenceB = static_cast<OccurrenceId>(occurrenceB);
        if (!doc.findOccurrence(constraint.occurrenceA) ||
            !doc.findOccurrence(constraint.occurrenceB)) {
            return fail("assembly constraint references a missing occurrence");
        }
        doc.nextConstraintId_ = std::max(doc.nextConstraintId_, constraint.id + 1);
        doc.constraints_.push_back(std::move(constraint));
    }

    for (const auto object : arrayObjects(jointArray)) {
        AssemblyJoint joint;
        double id = 0.0, parent = 0.0, child = 0.0;
        std::string type;
        if (!parseMemberNumber(object, "id", id) || id < 1.0 || std::floor(id) != id ||
            !parseMemberString(object, "type", type) || !jointTypeFromName(type, joint.type) ||
            !parseMemberString(object, "name", joint.name) ||
            !parseMemberNumber(object, "parent", parent) ||
            !parseMemberNumber(object, "child", child) ||
            !parseMemberNumber(object, "axisX", joint.axisX) ||
            !parseMemberNumber(object, "axisY", joint.axisY) ||
            !parseMemberNumber(object, "axisZ", joint.axisZ) ||
            !parseMemberNumber(object, "offsetX", joint.offsetX) ||
            !parseMemberNumber(object, "offsetY", joint.offsetY) ||
            !parseMemberNumber(object, "offsetZ", joint.offsetZ) ||
            !parseMemberNumber(object, "position", joint.position) ||
            !parseMemberNumber(object, "minimum", joint.minimum) ||
            !parseMemberNumber(object, "maximum", joint.maximum)) {
            return fail("invalid assembly joint record");
        }
        joint.id = static_cast<JointId>(id);
        joint.parentOccurrence = static_cast<OccurrenceId>(parent);
        joint.childOccurrence = static_cast<OccurrenceId>(child);
        const double axisLength = std::sqrt(joint.axisX * joint.axisX +
                                            joint.axisY * joint.axisY +
                                            joint.axisZ * joint.axisZ);
        if (!doc.findOccurrence(joint.parentOccurrence) ||
            !doc.findOccurrence(joint.childOccurrence) ||
            !(axisLength > 1e-12) || joint.minimum > joint.maximum ||
            joint.position < joint.minimum || joint.position > joint.maximum) {
            return fail("assembly joint references or limits are invalid");
        }
        joint.axisX /= axisLength;
        joint.axisY /= axisLength;
        joint.axisZ /= axisLength;
        doc.nextJointId_ = std::max(doc.nextJointId_, joint.id + 1);
        doc.joints_.push_back(std::move(joint));
    }

    const auto solveReport = doc.solve();
    if (!solveReport.ok) return fail(solveReport.message);
    return doc;
}

} // namespace mydesk::mechanical
