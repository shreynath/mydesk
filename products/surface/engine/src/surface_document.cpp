// SPDX-License-Identifier: MPL-2.0
#include "mydesk/surface/surface_document.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
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

bool extractString(std::string_view json, std::string_view key, std::string& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) return false;
    const auto q1 = json.find('"', json.find(':', pos));
    if (q1 == std::string_view::npos) return false;
    const auto q2 = json.find('"', q1 + 1);
    if (q2 == std::string_view::npos) return false;
    out = std::string(json.substr(q1 + 1, q2 - q1 - 1));
    return true;
}

bool extractNumber(std::string_view json, std::string_view key, double& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) return false;
    const auto colon = json.find(':', pos);
    if (colon == std::string_view::npos) return false;
    try {
        out = std::stod(std::string(json.substr(colon + 1)));
        return std::isfinite(out);
    } catch (...) {
        return false;
    }
}

// Cox-de Boor basis function evaluation
double basis(int i, int p, double u, const std::vector<double>& U) {
    if (p == 0) {
        if ((u >= U[i] && u < U[i + 1]) || (u == U.back() && u >= U[i] && u <= U[i + 1])) {
            return 1.0;
        }
        return 0.0;
    }
    double left = 0.0;
    const double denom1 = U[i + p] - U[i];
    if (denom1 > 1e-12) {
        left = ((u - U[i]) / denom1) * basis(i, p - 1, u, U);
    }
    double right = 0.0;
    const double denom2 = U[i + p + 1] - U[i + 1];
    if (denom2 > 1e-12) {
        right = ((U[i + p + 1] - u) / denom2) * basis(i + 1, p - 1, u, U);
    }
    return left + right;
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
    if (controlGrid.empty() || numU == 0 || numV == 0) return {0, 0, 0};
    const double uClamped = std::clamp(u, knotsU.front(), knotsU.back());
    const double vClamped = std::clamp(v, knotsV.front(), knotsV.back());

    Vec3 num{0, 0, 0};
    double denom = 0.0;

    for (std::size_t i = 0; i < numU; ++i) {
        const double bu = basis(static_cast<int>(i), degreeU, uClamped, knotsU);
        for (std::size_t j = 0; j < numV; ++j) {
            const double bv = basis(static_cast<int>(j), degreeV, vClamped, knotsV);
            const auto& cp = controlGrid[i * numV + j];
            const double w = cp.w * bu * bv;
            num.x += cp.p.x * w;
            num.y += cp.p.y * w;
            num.z += cp.p.z * w;
            denom += w;
        }
    }
    if (std::abs(denom) < 1e-12) return controlGrid.front().p;
    return {num.x / denom, num.y / denom, num.z / denom};
}

Vec3 NurbsSurface::normalAt(double u, double v) const {
    const double delta = 1e-4;
    Vec3 su;
    if (u + delta <= knotsU.back()) {
        const auto p0 = evaluate(u, v);
        const auto p1 = evaluate(u + delta, v);
        su = Vec3{(p1.x - p0.x) / delta, (p1.y - p0.y) / delta, (p1.z - p0.z) / delta};
    } else {
        const auto p0 = evaluate(u - delta, v);
        const auto p1 = evaluate(u, v);
        su = Vec3{(p1.x - p0.x) / delta, (p1.y - p0.y) / delta, (p1.z - p0.z) / delta};
    }

    Vec3 sv;
    if (v + delta <= knotsV.back()) {
        const auto p0 = evaluate(u, v);
        const auto p1 = evaluate(u, v + delta);
        sv = Vec3{(p1.x - p0.x) / delta, (p1.y - p0.y) / delta, (p1.z - p0.z) / delta};
    } else {
        const auto p0 = evaluate(u, v - delta);
        const auto p1 = evaluate(u, v);
        sv = Vec3{(p1.x - p0.x) / delta, (p1.y - p0.y) / delta, (p1.z - p0.z) / delta};
    }

    return su.cross(sv).normalized();
}

double NurbsSurface::gaussianCurvatureAt(double u, double v) const {
    const double d = 1e-4;
    const auto p = evaluate(u, v);
    const auto pu = evaluate(std::min(knotsU.back(), u + d), v);
    const auto pv = evaluate(u, std::min(knotsV.back(), v + d));
    const auto puu = evaluate(std::min(knotsU.back(), u + 2 * d), v);
    const auto pvv = evaluate(u, std::min(knotsV.back(), v + 2 * d));
    const auto puv = evaluate(std::min(knotsU.back(), u + d), std::min(knotsV.back(), v + d));

    const Vec3 su{(pu.x - p.x) / d, (pu.y - p.y) / d, (pu.z - p.z) / d};
    const Vec3 sv{(pv.x - p.x) / d, (pv.y - p.y) / d, (pv.z - p.z) / d};
    const Vec3 n = su.cross(sv).normalized();

    const Vec3 suu{(puu.x - 2 * pu.x + p.x) / (d * d), (puu.y - 2 * pu.y + p.y) / (d * d), (puu.z - 2 * pu.z + p.z) / (d * d)};
    const Vec3 svv{(pvv.x - 2 * pv.x + p.x) / (d * d), (pvv.y - 2 * pv.y + p.y) / (d * d), (pvv.z - 2 * pv.z + p.z) / (d * d)};
    const Vec3 suv{(puv.x - pu.x - pv.x + p.x) / (d * d), (puv.y - pu.y - pv.y + p.y) / (d * d), (puv.z - pu.z - pv.z + p.z) / (d * d)};

    const double E = su.dot(su);
    const double F = su.dot(sv);
    const double G = sv.dot(sv);

    const double e = suu.dot(n);
    const double f = suv.dot(n);
    const double g = svv.dot(n);

    const double denom = E * G - F * F;
    if (std::abs(denom) < 1e-12) return 0.0;
    return (e * g - f * f) / denom;
}

double NurbsSurface::meanCurvatureAt(double u, double v) const {
    const double d = 1e-4;
    const auto p = evaluate(u, v);
    const auto pu = evaluate(std::min(knotsU.back(), u + d), v);
    const auto pv = evaluate(u, std::min(knotsV.back(), v + d));
    const auto puu = evaluate(std::min(knotsU.back(), u + 2 * d), v);
    const auto pvv = evaluate(u, std::min(knotsV.back(), v + 2 * d));
    const auto puv = evaluate(std::min(knotsU.back(), u + d), std::min(knotsV.back(), v + d));

    const Vec3 su{(pu.x - p.x) / d, (pu.y - p.y) / d, (pu.z - p.z) / d};
    const Vec3 sv{(pv.x - p.x) / d, (pv.y - p.y) / d, (pv.z - p.z) / d};
    const Vec3 n = su.cross(sv).normalized();

    const Vec3 suu{(puu.x - 2 * pu.x + p.x) / (d * d), (puu.y - 2 * pu.y + p.y) / (d * d), (puu.z - 2 * pu.z + p.z) / (d * d)};
    const Vec3 svv{(pvv.x - 2 * pv.x + p.x) / (d * d), (pvv.y - 2 * pv.y + p.y) / (d * d), (pvv.z - 2 * pv.z + p.z) / (d * d)};
    const Vec3 suv{(puv.x - pu.x - pv.x + p.x) / (d * d), (puv.y - pu.y - pv.y + p.y) / (d * d), (puv.z - pu.z - pv.z + p.z) / (d * d)};

    const double E = su.dot(su);
    const double F = su.dot(sv);
    const double G = sv.dot(sv);

    const double e = suu.dot(n);
    const double f = suv.dot(n);
    const double g = svv.dot(n);

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
    for (int i = 0; i <= 3; ++i) c.knots[i] = 0.0;
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

ContinuityReport SurfaceDocument::checkSeamContinuity(SurfacePatchId patchA, SurfacePatchId patchB, int samples) const {
    ContinuityReport rep;
    const auto* sa = findSurface(patchA);
    const auto* sb = findSurface(patchB);
    if (!sa || !sb || samples < 2) return rep;

    // Test seam along sa(u=1.0, v) and sb(u=0.0, v)
    double maxGap = 0.0;
    double maxAngDeg = 0.0;

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
    }

    rep.maxPositionalGap = maxGap;
    rep.maxAngularDeviationDeg = maxAngDeg;

    if (maxGap < 1e-4) {
        rep.achieved = ContinuityOrder::G0;
        if (maxAngDeg < 0.1) {
            rep.achieved = ContinuityOrder::G1;
        }
    }
    rep.passesTolerance = (rep.achieved >= ContinuityOrder::G0);
    return rep;
}

std::vector<double> SurfaceDocument::evaluateZebraStripes(SurfacePatchId patchId, int uRes, int vRes,
                                                          double stripeFrequency) const {
    std::vector<double> zebra;
    const auto* s = findSurface(patchId);
    if (!s || uRes < 2 || vRes < 2) return zebra;

    zebra.reserve(uRes * vRes);
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
    ss << "{\n";
    ss << "  \"format\": \"mydesk-surface\",\n";
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
           << ", \"numU\": " << s.numU << ", \"numV\": " << s.numV << "}";
        if (i + 1 < surfaces_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n}\n";
    return ss.str();
}

SurfaceDocument SurfaceDocument::deserializeNative(std::string_view json, std::string* error) {
    SurfaceDocument doc;
    std::string format;
    if (!extractString(json, "format", format) || format != "mydesk-surface") {
        if (error) *error = "not a mydesk-surface document";
        return doc;
    }
    extractString(json, "name", doc.name_);

    // Curve parsing
    const auto curvesPos = json.find("\"curves\"");
    const auto surfSectionPos = json.find("\"surfaces\"");
    std::size_t search = curvesPos;
    while (search != std::string_view::npos) {
        const auto idPos = json.find("\"id\":", search);
        if (idPos == std::string_view::npos || (surfSectionPos != std::string_view::npos && idPos >= surfSectionPos)) {
            break;
        }
        double id = 0, deg = 3;
        extractNumber(json.substr(idPos, 30), "id", id);
        std::string name;
        extractString(json.substr(idPos, 120), "name", name);
        extractNumber(json.substr(idPos, 120), "degree", deg);

        NurbsCurve c;
        c.id = static_cast<CurveId>(id);
        c.name = name;
        c.degree = static_cast<int>(deg);

        doc.curves_.push_back(std::move(c));
        doc.nextCurveId_ = std::max(doc.nextCurveId_, c.id + 1);
        search = idPos + 10;
    }

    return doc;
}

} // namespace mydesk::surface
