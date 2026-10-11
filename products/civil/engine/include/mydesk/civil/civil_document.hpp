#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mydesk::civil {

using PointId = std::uint64_t;
using SurfaceId = std::uint64_t;
using AlignmentId = std::uint64_t;
using ProfileId = std::uint64_t;

struct CogoPoint {
    PointId id{0};
    std::string pointNumber;
    double easting{0.0};   // X
    double northing{0.0};  // Y
    double elevation{0.0}; // Z
    std::string rawDescription;
};

struct Triangle3D {
    PointId p0{0};
    PointId p1{0};
    PointId p2{0};
};

struct ContourLine {
    double elevation{0.0};
    std::vector<std::pair<double, double>> points;
};

struct AlignmentSegment {
    enum class Kind { Tangent, Curve };
    Kind kind{Kind::Tangent};
    double startStation{0.0};
    double length{0.0};
    // Tangent parameters
    double startX{0.0};
    double startY{0.0};
    double endX{0.0};
    double endY{0.0};
    // Curve parameters (if Curve)
    double centerX{0.0};
    double centerY{0.0};
    double radius{0.0};
    double startAngle{0.0};
    double endAngle{0.0};
    bool isClockwise{false};
};

struct StationOffset {
    double station{0.0};
    double offset{0.0}; // positive = right, negative = left
};

struct ProfilePVI {
    double station{0.0};
    double elevation{0.0};
    double curveLength{0.0}; // vertical curve length (0 for angular grade break)
};

struct TinSurface {
    SurfaceId id{0};
    std::string name;
    std::vector<CogoPoint> points;
    std::vector<Triangle3D> triangles;

    [[nodiscard]] std::optional<double> elevationAt(double x, double y) const;
    void calculateCutFillVolume(double datumElevation, double& cutVolume,
                                double& fillVolume) const;
    [[nodiscard]] std::vector<ContourLine> generateContours(double interval, double minElev,
                                                            double maxElev) const;
};

struct Alignment {
    AlignmentId id{0};
    std::string name;
    double startStation{0.0};
    std::vector<AlignmentSegment> segments;

    [[nodiscard]] double totalLength() const;
    [[nodiscard]] std::optional<StationOffset> getStationOffset(double x, double y) const;
    [[nodiscard]] std::pair<double, double> getPointAtStation(double station) const;
    [[nodiscard]] std::string formatStation(double station) const;
};

struct Profile {
    ProfileId id{0};
    AlignmentId alignmentId{0};
    std::string name;
    std::vector<ProfilePVI> pvis;

    [[nodiscard]] std::optional<double> elevationAtStation(double station) const;
};

class CivilDocument {
public:
    CivilDocument();

    [[nodiscard]] const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    PointId addPoint(std::string pointNum, double easting, double northing, double elevation,
                     std::string desc);
    bool updatePoint(PointId id, std::string pointNum, double easting, double northing,
                     double elevation, std::string desc);
    [[nodiscard]] const CogoPoint* findPoint(PointId id) const;
    [[nodiscard]] const std::vector<CogoPoint>& points() const { return points_; }

    SurfaceId buildTinSurface(std::string name, const std::vector<PointId>& pointIds);
    bool renameSurface(SurfaceId id, std::string name);
    [[nodiscard]] const TinSurface* findSurface(SurfaceId id) const;
    [[nodiscard]] const std::vector<TinSurface>& surfaces() const { return surfaces_; }

    AlignmentId createAlignment(std::string name, double startStation = 0.0);
    bool addAlignmentTangent(AlignmentId id, double startX, double startY, double endX,
                             double endY);
    bool addAlignmentCurve(AlignmentId id, double centerX, double centerY, double radius,
                           double startAngle, double endAngle, bool clockwise = false);
    bool updateAlignment(AlignmentId id, std::string name, double startStation);
    [[nodiscard]] const Alignment* findAlignment(AlignmentId id) const;
    [[nodiscard]] const std::vector<Alignment>& alignments() const { return alignments_; }

    ProfileId createProfile(std::string name, AlignmentId alignmentId);
    bool addProfilePVI(ProfileId id, double station, double elevation, double curveLength = 0.0);
    bool renameProfile(ProfileId id, std::string name);
    [[nodiscard]] const Profile* findProfile(ProfileId id) const;
    [[nodiscard]] const std::vector<Profile>& profiles() const { return profiles_; }

    [[nodiscard]] std::string exportLandXml() const;
    [[nodiscard]] std::string serializeNative() const;
    static CivilDocument deserializeNative(std::string_view json, std::string* error = nullptr);

private:
    std::string name_{"CivilProject1"};
    std::vector<CogoPoint> points_;
    std::vector<TinSurface> surfaces_;
    std::vector<Alignment> alignments_;
    std::vector<Profile> profiles_;
    PointId nextPointId_{1};
    SurfaceId nextSurfaceId_{1};
    AlignmentId nextAlignmentId_{1};
    ProfileId nextProfileId_{1};
};

} // namespace mydesk::civil
