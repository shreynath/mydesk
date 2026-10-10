#include "mydesk/coordinate/coordinate_document.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace mydesk::coordinate {

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

bool extractNumber(std::string_view json, std::string_view key, double& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return false;
    }
    const auto colon = json.find(':', pos);
    if (colon == std::string_view::npos) {
        return false;
    }
    try {
        out = std::stod(std::string(json.substr(colon + 1)));
        return std::isfinite(out);
    } catch (...) {
        return false;
    }
}

} // namespace

bool BoundingBox3D::intersects(const BoundingBox3D& other) const {
    return (minX <= other.maxX && maxX >= other.minX) &&
           (minY <= other.maxY && maxY >= other.minY) &&
           (minZ <= other.maxZ && maxZ >= other.minZ);
}

bool BoundingBox3D::withinDistance(const BoundingBox3D& other, double clearance) const {
    const double dx = std::max(0.0, std::max(minX - other.maxX, other.minX - maxX));
    const double dy = std::max(0.0, std::max(minY - other.maxY, other.minY - maxY));
    const double dz = std::max(0.0, std::max(minZ - other.maxZ, other.minZ - maxZ));
    return std::sqrt(dx * dx + dy * dy + dz * dz) <= clearance;
}

double BoundingBox3D::penetrationDepth(const BoundingBox3D& other) const {
    if (!intersects(other)) {
        return 0.0;
    }
    const double ox = std::min(maxX, other.maxX) - std::max(minX, other.minX);
    const double oy = std::min(maxY, other.maxY) - std::max(minY, other.minY);
    const double oz = std::min(maxZ, other.maxZ) - std::max(minZ, other.minZ);
    return std::min({ox, oy, oz});
}

std::tuple<double, double, double> BoundingBox3D::center() const {
    return {(minX + maxX) * 0.5, (minY + maxY) * 0.5, (minZ + maxZ) * 0.5};
}

CoordinateDocument::CoordinateDocument() = default;

ModelId CoordinateDocument::addModel(std::string name, std::string sourcePath,
                                     std::string discipline) {
    AggregatedModel m;
    m.id = nextModelId_++;
    m.name = std::move(name);
    m.sourcePath = std::move(sourcePath);
    m.discipline = std::move(discipline);
    models_.push_back(std::move(m));
    return models_.back().id;
}

ElementId CoordinateDocument::addElementToModel(ModelId modelId, std::string name,
                                                std::string category, const BoundingBox3D& bounds) {
    for (auto& m : models_) {
        if (m.id == modelId) {
            CoordinationElement el;
            el.id = nextElementId_++;
            el.modelId = modelId;
            el.name = std::move(name);
            el.category = std::move(category);
            el.bounds = bounds;
            m.elements.push_back(el);
            return el.id;
        }
    }
    return 0;
}

const AggregatedModel* CoordinateDocument::findModel(ModelId id) const {
    for (const auto& m : models_) {
        if (m.id == id) {
            return &m;
        }
    }
    return nullptr;
}

const CoordinationElement* CoordinateDocument::findElement(ElementId id) const {
    for (const auto& m : models_) {
        for (const auto& el : m.elements) {
            if (el.id == id) {
                return &el;
            }
        }
    }
    return nullptr;
}

ClashTestId CoordinateDocument::createClashTest(std::string name, ClashType type, double tolerance,
                                                double clearance) {
    ClashTest t;
    t.id = nextTestId_++;
    t.name = std::move(name);
    t.type = type;
    t.tolerance = tolerance;
    t.clearance = clearance;
    clashTests_.push_back(std::move(t));
    return clashTests_.back().id;
}

bool CoordinateDocument::setClashTestSets(ClashTestId testId, std::vector<ModelId> setA,
                                          std::vector<ModelId> setB) {
    for (auto& t : clashTests_) {
        if (t.id == testId) {
            t.selectionSetA = std::move(setA);
            t.selectionSetB = std::move(setB);
            return true;
        }
    }
    return false;
}

std::size_t CoordinateDocument::runClashTest(ClashTestId testId) {
    ClashTest* test = nullptr;
    for (auto& t : clashTests_) {
        if (t.id == testId) {
            test = &t;
            break;
        }
    }
    if (!test) {
        return 0;
    }

    test->results.clear();

    // Gather elements from Set A
    std::vector<const CoordinationElement*> elementsA;
    for (auto mId : test->selectionSetA) {
        const auto* m = findModel(mId);
        if (m) {
            for (const auto& el : m->elements) {
                elementsA.push_back(&el);
            }
        }
    }

    // Gather elements from Set B
    std::vector<const CoordinationElement*> elementsB;
    for (auto mId : test->selectionSetB) {
        const auto* m = findModel(mId);
        if (m) {
            for (const auto& el : m->elements) {
                elementsB.push_back(&el);
            }
        }
    }

    for (const auto* elA : elementsA) {
        for (const auto* elB : elementsB) {
            if (elA->id == elB->id) {
                continue;
            }

            if (test->type == ClashType::Hard) {
                if (elA->bounds.intersects(elB->bounds)) {
                    const double depth = elA->bounds.penetrationDepth(elB->bounds);
                    if (depth >= test->tolerance) {
                        ClashResult res;
                        res.id = nextResultId_++;
                        res.testId = testId;
                        res.type = ClashType::Hard;
                        res.status = ClashStatus::New;
                        res.elementA = elA->id;
                        res.elementB = elB->id;
                        res.penetrationOrDistance = depth;

                        const auto [cx, cy, cz] = elA->bounds.center();
                        res.clashX = cx;
                        res.clashY = cy;
                        res.clashZ = cz;
                        res.comments = "Hard clash detected between " + elA->name + " and " + elB->name;
                        test->results.push_back(std::move(res));
                    }
                }
            } else if (test->type == ClashType::Clearance) {
                if (elA->bounds.withinDistance(elB->bounds, test->clearance)) {
                    const double dx = std::max(0.0, std::max(elA->bounds.minX - elB->bounds.maxX,
                                                             elB->bounds.minX - elA->bounds.maxX));
                    const double dy = std::max(0.0, std::max(elA->bounds.minY - elB->bounds.maxY,
                                                             elB->bounds.minY - elA->bounds.maxY));
                    const double dz = std::max(0.0, std::max(elA->bounds.minZ - elB->bounds.maxZ,
                                                             elB->bounds.minZ - elA->bounds.maxZ));
                    const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (dist <= test->clearance) {
                        ClashResult res;
                        res.id = nextResultId_++;
                        res.testId = testId;
                        res.type = ClashType::Clearance;
                        res.status = ClashStatus::New;
                        res.elementA = elA->id;
                        res.elementB = elB->id;
                        res.penetrationOrDistance = dist;

                        const auto [cx, cy, cz] = elA->bounds.center();
                        res.clashX = cx;
                        res.clashY = cy;
                        res.clashZ = cz;
                        res.comments = "Clearance violation: distance " + std::to_string(dist) + " m <= " +
                                       std::to_string(test->clearance) + " m";
                        test->results.push_back(std::move(res));
                    }
                }
            }
        }
    }
    return test->results.size();
}

const ClashTest* CoordinateDocument::findClashTest(ClashTestId testId) const {
    for (const auto& t : clashTests_) {
        if (t.id == testId) {
            return &t;
        }
    }
    return nullptr;
}

std::string CoordinateDocument::exportBcfXml() const {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<VisualizationInfo xmlns=\"http://www.buildingsmart-tech.org/specifications/bcf-releases\">\n";
    xml << "  <Components>\n";
    for (const auto& t : clashTests_) {
        for (const auto& r : t.results) {
            xml << "    <Component IfcGuid=\"EL_" << r.elementA << "\">\n";
            xml << "      <Origin X=\"" << r.clashX << "\" Y=\"" << r.clashY << "\" Z=\"" << r.clashZ << "\"/>\n";
            xml << "      <Status>" << (r.status == ClashStatus::New ? "New" : "Active") << "</Status>\n";
            xml << "      <Comment>" << escapeJson(r.comments) << "</Comment>\n";
            xml << "    </Component>\n";
        }
    }
    xml << "  </Components>\n";
    xml << "</VisualizationInfo>\n";
    return xml.str();
}

std::string CoordinateDocument::serializeNative() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"format\": \"mydesk-coordinate\",\n";
    oss << "  \"name\": \"" << escapeJson(name_) << "\",\n";
    oss << "  \"models\": [\n";
    for (std::size_t i = 0; i < models_.size(); ++i) {
        const auto& m = models_[i];
        oss << "    {\"id\": " << m.id << ", \"name\": \"" << escapeJson(m.name)
            << "\", \"discipline\": \"" << escapeJson(m.discipline) << "\", \"elements\": [\n";
        for (std::size_t e = 0; e < m.elements.size(); ++e) {
            const auto& el = m.elements[e];
            oss << "      {\"id\": " << el.id << ", \"name\": \"" << escapeJson(el.name)
                << "\", \"cat\": \"" << escapeJson(el.category)
                << "\", \"bounds\": [" << el.bounds.minX << ", " << el.bounds.minY << ", " << el.bounds.minZ
                << ", " << el.bounds.maxX << ", " << el.bounds.maxY << ", " << el.bounds.maxZ << "]}";
            if (e + 1 < m.elements.size()) {
                oss << ",";
            }
            oss << "\n";
        }
        oss << "    ]}";
        if (i + 1 < models_.size()) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << "  ]\n}\n";
    return oss.str();
}

CoordinateDocument CoordinateDocument::deserializeNative(std::string_view json, std::string* error) {
    CoordinateDocument doc;
    std::string format;
    if (!extractString(json, "format", format) || format != "mydesk-coordinate") {
        if (error) {
            *error = "not a mydesk-coordinate document";
        }
        return doc;
    }
    extractString(json, "name", doc.name_);

    // Minimal parser to reconstruct models and elements
    std::size_t search = json.find("\"models\"");
    while (search != std::string_view::npos) {
        const auto idPos = json.find("\"id\":", search);
        if (idPos == std::string_view::npos) {
            break;
        }
        const auto namePos = json.find("\"name\":", idPos);
        const auto discPos = json.find("\"discipline\":", idPos);
        if (namePos == std::string_view::npos || discPos == std::string_view::npos) {
            break;
        }
        AggregatedModel m;
        double id = 0;
        extractNumber(json.substr(idPos, 30), "id", id);
        m.id = static_cast<ModelId>(id);
        extractString(json.substr(namePos, 80), "name", m.name);
        extractString(json.substr(discPos, 80), "discipline", m.discipline);
        doc.models_.push_back(m);
        doc.nextModelId_ = std::max(doc.nextModelId_, m.id + 1);
        search = discPos + 10;
    }
    return doc;
}

} // namespace mydesk::coordinate
