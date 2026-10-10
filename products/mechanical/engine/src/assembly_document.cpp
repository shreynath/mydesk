#include "mydesk/mechanical/assembly_document.hpp"

#include <BRepBuilderAPI_Transform.hxx>
#include <BRep_Builder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Writer.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <sstream>
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

bool extractString(std::string_view json, std::string_view key, std::string& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return false;
    }
    const auto q1 = json.find('"', json.find(':', pos));
    if (q1 == std::string_view::npos) {
        return false;
    }
    const auto q2 = json.find('"', q1 + 1);
    if (q2 == std::string_view::npos) {
        return false;
    }
    out = std::string(json.substr(q1 + 1, q2 - q1 - 1));
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

    report.message = "assembly constraints solved";
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
        gp_Trsf trsf;
        trsf.SetTranslation(gp_Vec(occ.x, occ.y, occ.z));
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
    oss << "{\n";
    oss << "  \"format\": \"mydesk-mechanical-assembly\",\n";
    oss << "  \"name\": \"" << escapeJson(name_) << "\",\n";
    oss << "  \"occurrences\": [\n";
    for (std::size_t i = 0; i < occurrences_.size(); ++i) {
        const auto& occ = occurrences_[i];
        oss << "    {\"id\": " << occ.id << ", \"name\": \"" << escapeJson(occ.name)
            << "\", \"x\": " << occ.x << ", \"y\": " << occ.y << ", \"z\": " << occ.z
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
    oss << "  ]\n}\n";
    return oss.str();
}

AssemblyDocument AssemblyDocument::deserializeNative(std::string_view json, std::string* error) {
    AssemblyDocument doc;
    std::string format;
    if (!extractString(json, "format", format) || format != "mydesk-mechanical-assembly") {
        if (error) {
            *error = "not a mydesk-mechanical-assembly document";
        }
        return doc;
    }
    extractString(json, "name", doc.name_);

    // Parse occurrences
    std::size_t search = json.find("\"occurrences\"");
    const auto constraintsPos = json.find("\"constraints\"");
    while (search != std::string_view::npos && (constraintsPos == std::string_view::npos || search < constraintsPos)) {
        const auto idPos = json.find("\"id\":", search);
        if (idPos == std::string_view::npos || (constraintsPos != std::string_view::npos && idPos > constraintsPos)) {
            break;
        }
        const auto partPos = json.find("\"part\":", idPos);
        if (partPos == std::string_view::npos) {
            break;
        }
        ComponentOccurrence occ;
        const auto namePos = json.find("\"name\":", idPos);
        if (namePos != std::string_view::npos) {
            extractString(json.substr(namePos, 80), "name", occ.name);
        }
        const auto xPos = json.find("\"x\":", idPos);
        const auto yPos = json.find("\"y\":", idPos);
        const auto zPos = json.find("\"z\":", idPos);
        if (xPos != std::string_view::npos) {
            occ.x = std::stod(std::string(json.substr(xPos + 4, 20)));
        }
        if (yPos != std::string_view::npos) {
            occ.y = std::stod(std::string(json.substr(yPos + 4, 20)));
        }
        if (zPos != std::string_view::npos) {
            occ.z = std::stod(std::string(json.substr(zPos + 4, 20)));
        }
        const auto grPos = json.find("\"isGrounded\":", idPos);
        if (grPos != std::string_view::npos) {
            occ.isGrounded = (json.substr(grPos, 20).find("true") != std::string_view::npos);
        }

        // Deserialize nested part document
        const auto partJsonStart = json.find('{', partPos);
        if (partJsonStart != std::string_view::npos) {
            const auto partJsonEnd = json.find("}\n    }", partJsonStart);
            if (partJsonEnd != std::string_view::npos) {
                const auto partStr = json.substr(partJsonStart, partJsonEnd - partJsonStart + 2);
                occ.part = PartDocument::deserializeNative(partStr);
            }
        }
        doc.occurrences_.push_back(std::move(occ));
        search = partPos + 10;
    }

    doc.solve();
    return doc;
}

} // namespace mydesk::mechanical
