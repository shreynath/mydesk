// SPDX-License-Identifier: MPL-2.0
#include "mydesk/surface/surface_document.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace mydesk::surface {

namespace {

constexpr double kPi = 3.14159265358979323846;

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

// Cox-de Boor basis function evaluation
double basis(int i, int p, double u, const std::vector<double>& U) {
    if (i < 0 || p < 0) return 0.0;
    const auto index = static_cast<std::size_t>(i);
    const auto degree = static_cast<std::size_t>(p);
    if (p == 0) {
        if ((u >= U[index] && u < U[index + 1]) ||
            (u == U.back() && u >= U[index] && u <= U[index + 1])) {
            return 1.0;
        }
        return 0.0;
    }
    double left = 0.0;
    const double denom1 = U[index + degree] - U[index];
    if (denom1 > 1e-12) {
        left = ((u - U[index]) / denom1) * basis(i, p - 1, u, U);
    }
    double right = 0.0;
    const double denom2 = U[index + degree + 1] - U[index + 1];
    if (denom2 > 1e-12) {
        right = ((U[index + degree + 1] - u) / denom2) * basis(i + 1, p - 1, u, U);
    }
    return left + right;
}

double basisDerivative(int i, int p, int order, double u, const std::vector<double>& knots) {
    if (order == 0) return basis(i, p, u, knots);
    if (p == 0 || order > p) return 0.0;

    const auto index = static_cast<std::size_t>(i);
    const auto degree = static_cast<std::size_t>(p);
    double left = 0.0;
    const double leftDenominator = knots[index + degree] - knots[index];
    if (leftDenominator > 1e-12) {
        left = static_cast<double>(p) / leftDenominator *
               basisDerivative(i, p - 1, order - 1, u, knots);
    }
    double right = 0.0;
    const double rightDenominator = knots[index + degree + 1] - knots[index + 1];
    if (rightDenominator > 1e-12) {
        right = static_cast<double>(p) / rightDenominator *
                basisDerivative(i + 1, p - 1, order - 1, u, knots);
    }
    return left - right;
}

Vec3 add(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 subtract(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 multiply(const Vec3& value, double scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vec3 divide(const Vec3& value, double scale) {
    if (std::abs(scale) < 1e-14) return {};
    return {value.x / scale, value.y / scale, value.z / scale};
}

std::size_t skipWhitespace(std::string_view text, std::size_t pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
    return pos;
}

bool parseJsonString(std::string_view text, std::size_t pos, std::string& value,
                     std::size_t* end = nullptr) {
    pos = skipWhitespace(text, pos);
    if (pos >= text.size() || text[pos] != '"') return false;
    value.clear();
    bool escaped = false;
    for (++pos; pos < text.size(); ++pos) {
        const char c = text[pos];
        if (escaped) {
            if (c != '"' && c != '\\' && c != '/' && c != 'b' && c != 'f' &&
                c != 'n' && c != 'r' && c != 't') return false;
            switch (c) {
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: value.push_back(c); break;
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            if (end) *end = pos + 1;
            return true;
        } else {
            value.push_back(c);
        }
    }
    return false;
}

std::size_t memberValuePosition(std::string_view object, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const std::size_t keyPos = object.find(needle);
    if (keyPos == std::string_view::npos) return keyPos;
    const std::size_t colon = object.find(':', keyPos + needle.size());
    return colon == std::string_view::npos ? colon : skipWhitespace(object, colon + 1);
}

bool parseMemberString(std::string_view object, std::string_view key, std::string& value) {
    const auto pos = memberValuePosition(object, key);
    return pos != std::string_view::npos && parseJsonString(object, pos, value);
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
        if (c == '"') {
            inString = true;
        } else if (c == open) {
            ++depth;
        } else if (c == close && --depth == 0) {
            return pos;
        }
    }
    return std::string_view::npos;
}

bool memberArray(std::string_view object, std::string_view key, std::string_view& contents) {
    const auto pos = memberValuePosition(object, key);
    if (pos == std::string_view::npos || pos >= object.size() || object[pos] != '[') return false;
    const auto end = matchingDelimiter(object, pos, '[', ']');
    if (end == std::string_view::npos) return false;
    contents = object.substr(pos + 1, end - pos - 1);
    return true;
}

bool parseNumberArray(std::string_view object, std::string_view key,
                      std::vector<double>& values) {
    std::string_view array;
    if (!memberArray(object, key, array)) return false;
    values.clear();
    std::size_t pos = 0;
    while ((pos = skipWhitespace(array, pos)) < array.size()) {
        std::size_t consumed = 0;
        double value = 0.0;
        try {
            value = std::stod(std::string(array.substr(pos)), &consumed);
        } catch (...) {
            return false;
        }
        if (consumed == 0 || !std::isfinite(value)) return false;
        values.push_back(value);
        pos = skipWhitespace(array, pos + consumed);
        if (pos == array.size()) break;
        if (array[pos] != ',') return false;
        ++pos;
    }
    return true;
}

std::vector<std::string_view> arrayObjects(std::string_view array) {
    std::vector<std::string_view> objects;
    std::size_t pos = 0;
    while ((pos = skipWhitespace(array, pos)) < array.size()) {
        if (array[pos] != '{') return {};
        const auto end = matchingDelimiter(array, pos, '{', '}');
        if (end == std::string_view::npos) return {};
        objects.push_back(array.substr(pos, end - pos + 1));
        pos = skipWhitespace(array, end + 1);
        if (pos == array.size()) break;
        if (array[pos] != ',') return {};
        ++pos;
    }
    return objects;
}

bool parseWeightedPoints(std::string_view object, std::string_view key,
                         std::vector<WeightedPoint>& points) {
    std::string_view array;
    if (!memberArray(object, key, array)) return false;
    const auto objects = arrayObjects(array);
    if (skipWhitespace(array, 0) < array.size() && objects.empty()) return false;
    points.clear();
    for (const auto item : objects) {
        WeightedPoint point;
        if (!parseMemberNumber(item, "x", point.p.x) ||
            !parseMemberNumber(item, "y", point.p.y) ||
            !parseMemberNumber(item, "z", point.p.z) ||
            !parseMemberNumber(item, "w", point.w) || !(point.w > 0.0)) {
            return false;
        }
        points.push_back(point);
    }
    return true;
}

bool validKnots(const std::vector<double>& knots, std::size_t controlCount, int degree) {
    return degree >= 1 && controlCount > static_cast<std::size_t>(degree) &&
           knots.size() == controlCount + static_cast<std::size_t>(degree) + 1 &&
           std::is_sorted(knots.begin(), knots.end()) && knots.front() < knots.back();
}

} // namespace

double Vec3::length() const {
    return std::sqrt(x * x + y * y + z * z);
}

Vec3 Vec3::normalized() const {
    const double l = length();
    if (l < 1e-12) return {0, 0, 0};
    return {x / l, y / l, z / l};
}

Vec3 Vec3::cross(const Vec3& o) const {
    return {
        y * o.z - z * o.y,
        z * o.x - x * o.z,
        x * o.y - y * o.x
    };
}

double Vec3::dot(const Vec3& o) const {
    return x * o.x + y * o.y + z * o.z;
}

Vec3 NurbsCurve::evaluate(double u) const {
    if (controlPoints.empty()) return {0, 0, 0};
    const double uClamped = std::clamp(u, knots.front(), knots.back());

    Vec3 num{0, 0, 0};
    double denom = 0.0;

    for (std::size_t i = 0; i < controlPoints.size(); ++i) {
        const double b = basis(static_cast<int>(i), degree, uClamped, knots);
        const double w = controlPoints[i].w * b;
        num.x += controlPoints[i].p.x * w;
        num.y += controlPoints[i].p.y * w;
        num.z += controlPoints[i].p.z * w;
        denom += w;
    }
    if (std::abs(denom) < 1e-12) return controlPoints.front().p;
    return {num.x / denom, num.y / denom, num.z / denom};
}

double NurbsCurve::curvatureAt(double u) const {
    const double du = 1e-4;
    const double u0 = std::max(knots.front(), u - du);
    const double u1 = u;
    const double u2 = std::min(knots.back(), u + du);

    const auto p0 = evaluate(u0);
    const auto p1 = evaluate(u1);
    const auto p2 = evaluate(u2);

    const double d1 = (u1 - u0 > 0.0) ? u1 - u0 : 1e-6;
    const double d2 = (u2 - u1 > 0.0) ? u2 - u1 : 1e-6;

    const Vec3 v1{(p1.x - p0.x) / d1, (p1.y - p0.y) / d1, (p1.z - p0.z) / d1};
    const Vec3 v2{(p2.x - p1.x) / d2, (p2.y - p1.y) / d2, (p2.z - p1.z) / d2};

    const Vec3 acc{(v2.x - v1.x) / (0.5 * (d1 + d2)),
                   (v2.y - v1.y) / (0.5 * (d1 + d2)),
                   (v2.z - v1.z) / (0.5 * (d1 + d2))};

    const double speed = v1.length();
    if (speed < 1e-9) return 0.0;
    const Vec3 crossProd = v1.cross(acc);
    return crossProd.length() / (speed * speed * speed);
}

Vec3 NurbsSurface::evaluate(double u, double v) const {
    return derivativesAt(u, v).point;
}

NurbsSurface::Derivatives NurbsSurface::derivativesAt(double u, double v) const {
    Derivatives result;
    if (controlGrid.size() != numU * numV || numU == 0 || numV == 0 ||
        knotsU.empty() || knotsV.empty()) {
        return result;
    }
    const double uc = std::clamp(u, knotsU.front(), knotsU.back());
    const double vc = std::clamp(v, knotsV.front(), knotsV.back());

    Vec3 a, au, av, auu, auv, avv;
    double w = 0.0, wu = 0.0, wv = 0.0, wuu = 0.0, wuv = 0.0, wvv = 0.0;
    for (std::size_t i = 0; i < numU; ++i) {
        const int ii = static_cast<int>(i);
        const double nu = basisDerivative(ii, degreeU, 0, uc, knotsU);
        const double nud = basisDerivative(ii, degreeU, 1, uc, knotsU);
        const double nudd = basisDerivative(ii, degreeU, 2, uc, knotsU);
        for (std::size_t j = 0; j < numV; ++j) {
            const int jj = static_cast<int>(j);
            const double nv = basisDerivative(jj, degreeV, 0, vc, knotsV);
            const double nvd = basisDerivative(jj, degreeV, 1, vc, knotsV);
            const double nvdd = basisDerivative(jj, degreeV, 2, vc, knotsV);
            const auto& cp = controlGrid[i * numV + j];
            const double b = nu * nv * cp.w;
            const double bu = nud * nv * cp.w;
            const double bv = nu * nvd * cp.w;
            const double buu = nudd * nv * cp.w;
            const double buv = nud * nvd * cp.w;
            const double bvv = nu * nvdd * cp.w;
            a = add(a, multiply(cp.p, b));
            au = add(au, multiply(cp.p, bu));
            av = add(av, multiply(cp.p, bv));
            auu = add(auu, multiply(cp.p, buu));
            auv = add(auv, multiply(cp.p, buv));
            avv = add(avv, multiply(cp.p, bvv));
            w += b; wu += bu; wv += bv; wuu += buu; wuv += buv; wvv += bvv;
        }
    }
    if (std::abs(w) < 1e-14) return result;

    result.point = divide(a, w);
    result.du = divide(subtract(au, multiply(result.point, wu)), w);
    result.dv = divide(subtract(av, multiply(result.point, wv)), w);
    result.duu = divide(subtract(subtract(auu, multiply(result.point, wuu)),
                                 multiply(result.du, 2.0 * wu)), w);
    result.duv = divide(subtract(subtract(subtract(auv, multiply(result.point, wuv)),
                                          multiply(result.du, wv)),
                                 multiply(result.dv, wu)), w);
    result.dvv = divide(subtract(subtract(avv, multiply(result.point, wvv)),
                                 multiply(result.dv, 2.0 * wv)), w);
    return result;
}

Vec3 NurbsSurface::normalAt(double u, double v) const {
    const auto derivatives = derivativesAt(u, v);
    return derivatives.du.cross(derivatives.dv).normalized();
}

double NurbsSurface::gaussianCurvatureAt(double u, double v) const {
    const auto d = derivativesAt(u, v);
    const Vec3 n = d.du.cross(d.dv).normalized();
    const double E = d.du.dot(d.du);
    const double F = d.du.dot(d.dv);
    const double G = d.dv.dot(d.dv);
    const double e = d.duu.dot(n);
    const double f = d.duv.dot(n);
    const double g = d.dvv.dot(n);

    const double denom = E * G - F * F;
    if (std::abs(denom) < 1e-12) return 0.0;
    return (e * g - f * f) / denom;
}

double NurbsSurface::meanCurvatureAt(double u, double v) const {
    const auto d = derivativesAt(u, v);
    const Vec3 n = d.du.cross(d.dv).normalized();
    const double E = d.du.dot(d.du);
    const double F = d.du.dot(d.dv);
    const double G = d.dv.dot(d.dv);
    const double e = d.duu.dot(n);
    const double f = d.duv.dot(n);
    const double g = d.dvv.dot(n);

    const double denom = 2.0 * (E * G - F * F);
    if (std::abs(denom) < 1e-12) return 0.0;
    return (e * G - 2.0 * f * F + g * E) / denom;
}

SurfaceDocument::SurfaceDocument() = default;

CurveId SurfaceDocument::addCubicCurve(std::string name, std::vector<Vec3> controlPoints) {
    if (controlPoints.size() < 4) return 0;
    NurbsCurve c;
    c.id = nextCurveId_++;
    c.name = std::move(name);
    c.degree = 3;
    const std::size_t n = controlPoints.size();
    // Clamped knot vector: 4 zeros, uniform interior, 4 ones
    c.knots.resize(n + 4);
    for (std::size_t i = 0; i <= 3; ++i) c.knots[i] = 0.0;
    for (std::size_t i = 4; i < n; ++i) {
        c.knots[i] = static_cast<double>(i - 3) / static_cast<double>(n - 3);
    }
    for (std::size_t i = n; i < n + 4; ++i) c.knots[i] = 1.0;

    for (const auto& pt : controlPoints) {
        c.controlPoints.push_back({pt, 1.0});
    }
    curves_.push_back(std::move(c));
    return curves_.back().id;
}

const NurbsCurve* SurfaceDocument::findCurve(CurveId id) const {
    for (const auto& c : curves_) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

std::vector<CurvatureCombSample> SurfaceDocument::generateCurvatureComb(CurveId id, int samples, double scale) const {
    std::vector<CurvatureCombSample> comb;
    const auto* curve = findCurve(id);
    if (!curve || samples < 2) return comb;

    for (int i = 0; i < samples; ++i) {
        const double u = static_cast<double>(i) / (samples - 1);
        const auto pt = curve->evaluate(u);
        const double kappa = curve->curvatureAt(u);
        // Tangent vector
        const double du = 1e-4;
        const auto pNext = curve->evaluate(std::min(1.0, u + du));
        const Vec3 tang{(pNext.x - pt.x) / du, (pNext.y - pt.y) / du, (pNext.z - pt.z) / du};
        const Vec3 norm = Vec3{0, 0, 1}.cross(tang).normalized();

        CurvatureCombSample smp;
        smp.u = u;
        smp.point = pt;
        smp.curvature = kappa;
        smp.combVector = Vec3{pt.x + norm.x * kappa * scale,
                              pt.y + norm.y * kappa * scale,
                              pt.z + norm.z * kappa * scale};
        comb.push_back(smp);
    }
    return comb;
}

SurfacePatchId SurfaceDocument::addBilinearPatch(std::string name, Vec3 p00, Vec3 p10, Vec3 p01, Vec3 p11) {
    NurbsSurface s;
    s.id = nextSurfId_++;
    s.name = std::move(name);
    s.degreeU = 1;
    s.degreeV = 1;
    s.knotsU = {0.0, 0.0, 1.0, 1.0};
    s.knotsV = {0.0, 0.0, 1.0, 1.0};
    s.numU = 2;
    s.numV = 2;
    s.controlGrid = {
        {p00, 1.0}, {p01, 1.0},
        {p10, 1.0}, {p11, 1.0}
    };
    surfaces_.push_back(std::move(s));
    return surfaces_.back().id;
}

SurfacePatchId SurfaceDocument::addBicubicPatch(std::string name, const std::vector<Vec3>& grid4x4) {
    if (grid4x4.size() != 16) return 0;
    NurbsSurface s;
    s.id = nextSurfId_++;
    s.name = std::move(name);
    s.degreeU = 3;
    s.degreeV = 3;
    s.knotsU = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0};
    s.knotsV = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0};
    s.numU = 4;
    s.numV = 4;
    for (const auto& p : grid4x4) {
        s.controlGrid.push_back({p, 1.0});
    }
    surfaces_.push_back(std::move(s));
    return surfaces_.back().id;
}

const NurbsSurface* SurfaceDocument::findSurface(SurfacePatchId id) const {
    for (const auto& s : surfaces_) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

NurbsSurface* SurfaceDocument::findSurfaceMutable(SurfacePatchId id) {
    for (auto& surface : surfaces_) {
        if (surface.id == id) return &surface;
    }
    return nullptr;
}

ContinuityReport SurfaceDocument::checkSeamContinuity(SurfacePatchId patchA, SurfacePatchId patchB, int samples) const {
    ContinuityReport rep;
    const auto* sa = findSurface(patchA);
    const auto* sb = findSurface(patchB);
    if (!sa || !sb || samples < 2) return rep;

    // Test seam along sa(u=1.0, v) and sb(u=0.0, v)
    double maxGap = 0.0;
    double maxAngDeg = 0.0;
    double maxCurvatureDeviation = 0.0;

    for (int i = 0; i < samples; ++i) {
        const double v = static_cast<double>(i) / (samples - 1);
        const auto pa = sa->evaluate(1.0, v);
        const auto pb = sb->evaluate(0.0, v);
        const double gap = Vec3{pa.x - pb.x, pa.y - pb.y, pa.z - pb.z}.length();
        maxGap = std::max(maxGap, gap);

        const auto na = sa->normalAt(1.0, v);
        const auto nb = sb->normalAt(0.0, v);
        const double dot = std::clamp(na.dot(nb), -1.0, 1.0);
        const double angDeg = std::acos(dot) * (180.0 / kPi);
        maxAngDeg = std::max(maxAngDeg, angDeg);

        const auto da = sa->derivativesAt(1.0, v);
        const auto db = sb->derivativesAt(0.0, v);
        const double metricA = da.du.dot(da.du);
        const double metricB = db.du.dot(db.du);
        const double curvatureA = metricA > 1e-14 ? da.duu.dot(na) / metricA : 0.0;
        const double curvatureB = metricB > 1e-14 ? db.duu.dot(nb) / metricB : 0.0;
        maxCurvatureDeviation = std::max(maxCurvatureDeviation,
                                         std::abs(curvatureA - curvatureB));
    }

    rep.maxPositionalGap = maxGap;
    rep.maxAngularDeviationDeg = maxAngDeg;
    rep.maxCurvatureDeviation = maxCurvatureDeviation;

    if (maxGap < 1e-4) {
        rep.achieved = ContinuityOrder::G0;
        if (maxAngDeg < 0.1) {
            rep.achieved = ContinuityOrder::G1;
            if (maxCurvatureDeviation < 1e-5) {
                rep.achieved = ContinuityOrder::G2;
            }
        }
    }
    rep.passesTolerance = maxGap < 1e-4;
    return rep;
}

bool SurfaceDocument::alignSeam(SurfacePatchId patchA, SurfacePatchId patchB,
                                ContinuityOrder order) {
    const auto* source = findSurface(patchA);
    auto* target = findSurfaceMutable(patchB);
    if (!source || !target || patchA == patchB ||
        source->degreeU != 3 || target->degreeU != 3 ||
        source->numU != 4 || target->numU != 4 ||
        source->numV != target->numV || source->numV == 0 ||
        source->controlGrid.size() != source->numU * source->numV ||
        target->controlGrid.size() != target->numU * target->numV) {
        return false;
    }
    const auto isClampedCubicBezier = [](const NurbsSurface& surface) {
        return surface.knotsU.size() == 8 &&
               std::all_of(surface.knotsU.begin(), surface.knotsU.begin() + 4,
                           [&](double knot) { return knot == surface.knotsU.front(); }) &&
               std::all_of(surface.knotsU.begin() + 4, surface.knotsU.end(),
                           [&](double knot) { return knot == surface.knotsU.back(); }) &&
               surface.knotsU.front() < surface.knotsU.back();
    };
    if (!isClampedCubicBezier(*source) || !isClampedCubicBezier(*target)) return false;
    for (const auto& cp : source->controlGrid) {
        if (std::abs(cp.w - 1.0) > 1e-12) return false;
    }
    for (const auto& cp : target->controlGrid) {
        if (std::abs(cp.w - 1.0) > 1e-12) return false;
    }

    SurfaceEdit edit;
    edit.patchId = patchB;
    edit.before = target->controlGrid;
    const double sourceSpan = source->knotsU.back() - source->knotsU.front();
    const double targetSpan = target->knotsU.back() - target->knotsU.front();
    const double firstDerivativeScale = targetSpan / sourceSpan;
    const double secondDerivativeScale = firstDerivativeScale * firstDerivativeScale;
    for (std::size_t j = 0; j < source->numV; ++j) {
        const Vec3 p1 = source->controlGrid[1 * source->numV + j].p;
        const Vec3 p2 = source->controlGrid[2 * source->numV + j].p;
        const Vec3 p3 = source->controlGrid[3 * source->numV + j].p;
        target->controlGrid[j].p = p3;
        target->controlGrid[j].w = 1.0;

        if (order >= ContinuityOrder::G1) {
            target->controlGrid[target->numV + j].p =
                add(p3, multiply(subtract(p3, p2), firstDerivativeScale));
            target->controlGrid[target->numV + j].w = 1.0;
        }
        if (order >= ContinuityOrder::G2) {
            // Match the endpoint second derivative of cubic Bezier rows:
            // (Q0 - 2Q1 + Q2)/spanB^2 = (P3 - 2P2 + P1)/spanA^2.
            const Vec3 q0 = target->controlGrid[j].p;
            const Vec3 q1 = target->controlGrid[target->numV + j].p;
            const Vec3 sourceSecondDifference = add(subtract(p3, multiply(p2, 2.0)), p1);
            target->controlGrid[2 * target->numV + j].p =
                add(subtract(multiply(q1, 2.0), q0),
                    multiply(sourceSecondDifference, secondDerivativeScale));
            target->controlGrid[2 * target->numV + j].w = 1.0;
        }
    }
    edit.after = target->controlGrid;
    undoStack_.push_back(std::move(edit));
    redoStack_.clear();
    return true;
}

bool SurfaceDocument::undo() {
    if (undoStack_.empty()) return false;
    SurfaceEdit edit = std::move(undoStack_.back());
    undoStack_.pop_back();
    auto* surface = findSurfaceMutable(edit.patchId);
    if (!surface) return false;
    surface->controlGrid = edit.before;
    redoStack_.push_back(std::move(edit));
    return true;
}

bool SurfaceDocument::redo() {
    if (redoStack_.empty()) return false;
    SurfaceEdit edit = std::move(redoStack_.back());
    redoStack_.pop_back();
    auto* surface = findSurfaceMutable(edit.patchId);
    if (!surface) return false;
    surface->controlGrid = edit.after;
    undoStack_.push_back(std::move(edit));
    return true;
}

std::vector<double> SurfaceDocument::evaluateZebraStripes(SurfacePatchId patchId, int uRes, int vRes,
                                                          double stripeFrequency) const {
    std::vector<double> zebra;
    const auto* s = findSurface(patchId);
    if (!s || uRes < 2 || vRes < 2) return zebra;

    zebra.reserve(static_cast<std::size_t>(uRes) * static_cast<std::size_t>(vRes));
    for (int i = 0; i < uRes; ++i) {
        const double u = static_cast<double>(i) / (uRes - 1);
        for (int j = 0; j < vRes; ++j) {
            const double v = static_cast<double>(j) / (vRes - 1);
            const auto norm = s->normalAt(u, v);
            // Zebra stripe pattern computed from reflection normal component
            const double intensity = 0.5 + 0.5 * std::sin(stripeFrequency * norm.y * 2.0 * kPi);
            zebra.push_back(intensity);
        }
    }
    return zebra;
}

std::string SurfaceDocument::exportIges() const {
    std::ostringstream ss;
    ss << "                                                                        S      1\n";
    ss << "1H,,1H;,11HMyDeskSurface,10Hsurface.igs,32HMyDesk Alias Class Surfacing,D      1\n";
    ss << "128,1,0,1,0,0,0,0,0,0;                                                  P      1\n";
    ss << "S      1D      1P      1                                                T      1\n";
    return ss.str();
}

std::string SurfaceDocument::serializeNative() const {
    std::ostringstream ss;
    ss << std::setprecision(17);
    ss << "{\n";
    ss << "  \"format\": \"mydesk-surface\",\n";
    ss << "  \"schemaVersion\": 2,\n";
    ss << "  \"name\": \"" << escapeJson(name_) << "\",\n";
    ss << "  \"curves\": [\n";
    for (std::size_t i = 0; i < curves_.size(); ++i) {
        const auto& c = curves_[i];
        ss << "    {\"id\": " << c.id << ", \"name\": \"" << escapeJson(c.name) << "\", \"degree\": " << c.degree
           << ", \"knots\": [";
        for (std::size_t k = 0; k < c.knots.size(); ++k) {
            ss << c.knots[k];
            if (k + 1 < c.knots.size()) ss << ", ";
        }
        ss << "], \"cps\": [";
        for (std::size_t p = 0; p < c.controlPoints.size(); ++p) {
            const auto& pt = c.controlPoints[p];
            ss << "{\"x\": " << pt.p.x << ", \"y\": " << pt.p.y << ", \"z\": " << pt.p.z << ", \"w\": " << pt.w << "}";
            if (p + 1 < c.controlPoints.size()) ss << ", ";
        }
        ss << "]}";
        if (i + 1 < curves_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    ss << "  \"surfaces\": [\n";
    for (std::size_t i = 0; i < surfaces_.size(); ++i) {
        const auto& s = surfaces_[i];
        ss << "    {\"id\": " << s.id << ", \"name\": \"" << escapeJson(s.name)
           << "\", \"degU\": " << s.degreeU << ", \"degV\": " << s.degreeV
           << ", \"numU\": " << s.numU << ", \"numV\": " << s.numV
           << ", \"knotsU\": [";
        for (std::size_t k = 0; k < s.knotsU.size(); ++k) {
            ss << s.knotsU[k];
            if (k + 1 < s.knotsU.size()) ss << ", ";
        }
        ss << "], \"knotsV\": [";
        for (std::size_t k = 0; k < s.knotsV.size(); ++k) {
            ss << s.knotsV[k];
            if (k + 1 < s.knotsV.size()) ss << ", ";
        }
        ss << "], \"grid\": [";
        for (std::size_t p = 0; p < s.controlGrid.size(); ++p) {
            const auto& point = s.controlGrid[p];
            ss << "{\"x\": " << point.p.x << ", \"y\": " << point.p.y
               << ", \"z\": " << point.p.z << ", \"w\": " << point.w << "}";
            if (p + 1 < s.controlGrid.size()) ss << ", ";
        }
        ss << "]}";
        if (i + 1 < surfaces_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n}\n";
    return ss.str();
}

SurfaceDocument SurfaceDocument::deserializeNative(std::string_view json, std::string* error) {
    SurfaceDocument doc;
    if (error) error->clear();
    std::string format;
    if (!parseMemberString(json, "format", format) || format != "mydesk-surface") {
        if (error) *error = "not a mydesk-surface document";
        return doc;
    }
    if (!parseMemberString(json, "name", doc.name_)) {
        if (error) *error = "surface document is missing its name";
        return SurfaceDocument{};
    }
    double schemaVersion = 0.0;
    if (!parseMemberNumber(json, "schemaVersion", schemaVersion) || schemaVersion != 2.0) {
        if (error) *error = "unsupported mydesk-surface schema version";
        return SurfaceDocument{};
    }

    auto fail = [&](std::string message) {
        if (error) *error = std::move(message);
        return SurfaceDocument{};
    };

    std::string_view curvesArray;
    std::string_view surfacesArray;
    if (!memberArray(json, "curves", curvesArray) ||
        !memberArray(json, "surfaces", surfacesArray)) {
        return fail("surface document is missing entity arrays");
    }

    const auto curveObjects = arrayObjects(curvesArray);
    if (skipWhitespace(curvesArray, 0) < curvesArray.size() && curveObjects.empty()) {
        return fail("invalid curve array");
    }
    for (const auto object : curveObjects) {
        NurbsCurve curve;
        double id = 0.0;
        double degree = 0.0;
        if (!parseMemberNumber(object, "id", id) || id < 1.0 || std::floor(id) != id ||
            !parseMemberString(object, "name", curve.name) ||
            !parseMemberNumber(object, "degree", degree) || std::floor(degree) != degree ||
            !parseNumberArray(object, "knots", curve.knots) ||
            !parseWeightedPoints(object, "cps", curve.controlPoints)) {
            return fail("invalid NURBS curve record");
        }
        curve.id = static_cast<CurveId>(id);
        curve.degree = static_cast<int>(degree);
        if (!validKnots(curve.knots, curve.controlPoints.size(), curve.degree)) {
            return fail("invalid NURBS curve knot/control-point dimensions");
        }
        doc.nextCurveId_ = std::max(doc.nextCurveId_, curve.id + 1);
        doc.curves_.push_back(std::move(curve));
    }

    const auto surfaceObjects = arrayObjects(surfacesArray);
    if (skipWhitespace(surfacesArray, 0) < surfacesArray.size() && surfaceObjects.empty()) {
        return fail("invalid surface array");
    }
    for (const auto object : surfaceObjects) {
        NurbsSurface surface;
        double id = 0.0, degreeU = 0.0, degreeV = 0.0, numU = 0.0, numV = 0.0;
        if (!parseMemberNumber(object, "id", id) || id < 1.0 || std::floor(id) != id ||
            !parseMemberString(object, "name", surface.name) ||
            !parseMemberNumber(object, "degU", degreeU) || std::floor(degreeU) != degreeU ||
            !parseMemberNumber(object, "degV", degreeV) || std::floor(degreeV) != degreeV ||
            !parseMemberNumber(object, "numU", numU) || numU < 1.0 || std::floor(numU) != numU ||
            !parseMemberNumber(object, "numV", numV) || numV < 1.0 || std::floor(numV) != numV ||
            !parseNumberArray(object, "knotsU", surface.knotsU) ||
            !parseNumberArray(object, "knotsV", surface.knotsV) ||
            !parseWeightedPoints(object, "grid", surface.controlGrid)) {
            return fail("invalid NURBS surface record");
        }
        surface.id = static_cast<SurfacePatchId>(id);
        surface.degreeU = static_cast<int>(degreeU);
        surface.degreeV = static_cast<int>(degreeV);
        surface.numU = static_cast<std::size_t>(numU);
        surface.numV = static_cast<std::size_t>(numV);
        if (surface.controlGrid.size() != surface.numU * surface.numV ||
            !validKnots(surface.knotsU, surface.numU, surface.degreeU) ||
            !validKnots(surface.knotsV, surface.numV, surface.degreeV)) {
            return fail("invalid NURBS surface knot/control-grid dimensions");
        }
        doc.nextSurfId_ = std::max(doc.nextSurfId_, surface.id + 1);
        doc.surfaces_.push_back(std::move(surface));
    }

    return doc;
}

} // namespace mydesk::surface
