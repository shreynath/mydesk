#include "mydesk/civil/civil_document.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void expect(bool cond, const std::string& msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
}

void test_cogo_and_tin_surface_volumes() {
    mydesk::civil::CivilDocument doc;
    const auto p1 = doc.addPoint("P1", 0.0, 0.0, 5.0, "CP1");
    const auto p2 = doc.addPoint("P2", 10.0, 0.0, 10.0, "CP2");
    const auto p3 = doc.addPoint("P3", 0.0, 10.0, 15.0, "CP3");

    const auto sId = doc.buildTinSurface("GroundTIN", {p1, p2, p3});
    const auto* surf = doc.findSurface(sId);
    expect(surf != nullptr, "surface created");
    expect(surf->triangles.size() == 1, "1 triangle");

    // Elevation check inside sloping triangle: z = 5 + 0.5*x + 1.0*y -> at (2, 2), z = 8.0
    const auto elev = surf->elevationAt(2.0, 2.0);
    expect(elev.has_value(), "elevation evaluated");
    expect(std::abs(*elev - 8.0) < 1e-6, "interpolated planar elevation 8.0");

    // Cut / Fill volume calculation against datum 8.0 m
    // Area = 0.5 * 10 * 10 = 50.0 m2; avg elev = (5+10+15)/3 = 10.0 m; diff = 2.0 m; volume = 100.0 m3 cut
    double cutVol = 0.0, fillVol = 0.0;
    surf->calculateCutFillVolume(8.0, cutVol, fillVol);
    expect(std::abs(cutVol - 100.0) < 1e-4, "cut volume 100 m3");
    expect(std::abs(fillVol - 0.0) < 1e-4, "fill volume 0 m3");

    // Contours at 1.0m interval between 6.0 and 14.0 crossing the slope
    const auto contours = surf->generateContours(1.0, 6.0, 14.0);
    expect(!contours.empty(), "contours generated");
}

void test_alignment_stationing_and_profile() {
    mydesk::civil::CivilDocument doc;
    const auto alId = doc.createAlignment("MainStreet", 0.0);
    expect(doc.addAlignmentTangent(alId, 0.0, 0.0, 1000.0, 0.0), "tangent added");

    const auto* al = doc.findAlignment(alId);
    expect(al != nullptr, "alignment found");
    expect(std::abs(al->totalLength() - 1000.0) < 1e-6, "total length 1000");

    // Engineering station format check
    const std::string staStr = al->formatStation(524.50);
    expect(staStr == "STA 5+24.50", "engineering station format STA 5+24.50");

    // Station and offset inquiry
    const auto so = al->getStationOffset(300.0, 25.0);
    expect(so.has_value(), "station offset computed");
    expect(std::abs(so->station - 300.0) < 1e-6, "station 300");
    expect(std::abs(so->offset - 25.0) < 1e-6, "offset 25");

    // Profile grade calculations
    const auto profId = doc.createProfile("ProposedGrade", alId);
    doc.addProfilePVI(profId, 0.0, 100.0);
    doc.addProfilePVI(profId, 1000.0, 150.0); // 5% grade

    const auto* prof = doc.findProfile(profId);
    expect(prof != nullptr, "profile found");
    const auto pElev = prof->elevationAtStation(400.0);
    expect(pElev.has_value(), "profile elevation evaluated");
    expect(std::abs(*pElev - 120.0) < 1e-6, "profile elevation at STA 400 is 120.0 (5% grade)");
}

void test_landxml_export_and_native_persistence() {
    mydesk::civil::CivilDocument doc;
    doc.setName("Hwy101");
    const auto p1 = doc.addPoint("1", 100.0, 200.0, 50.0, "MONUMENT");
    const auto p2 = doc.addPoint("2", 150.0, 200.0, 52.0, "TOPO");
    const auto p3 = doc.addPoint("3", 100.0, 250.0, 51.0, "TOPO");
    doc.buildTinSurface("SurveySurface", {p1, p2, p3});
    const auto alId = doc.createAlignment("Hwy101_CL", 100.0);
    doc.addAlignmentTangent(alId, 100.0, 200.0, 600.0, 200.0);

    // LandXML export verification
    const auto landXml = doc.exportLandXml();
    expect(landXml.find("<LandXML") != std::string::npos, "valid LandXML root");
    expect(landXml.find("<CgPoint") != std::string::npos, "CgPoints exported");
    expect(landXml.find("<Surface name=\"SurveySurface\"") != std::string::npos, "Surface exported");
    expect(landXml.find("<Alignment name=\"Hwy101_CL\"") != std::string::npos, "Alignment exported");

    // Native persistence roundtrip
    const auto json = doc.serializeNative();
    std::string err;
    auto loaded = mydesk::civil::CivilDocument::deserializeNative(json, &err);
    expect(loaded.points().size() == 3, "restored points");
    expect(loaded.points().front().pointNumber == "1", "point number matches");
}

void test_delaunay_and_complete_native_roundtrip() {
    mydesk::civil::CivilDocument doc;
    doc.setName("Subdivision A");
    const auto p1 = doc.addPoint("1", 0.0, 0.0, 100.0, "CORNER");
    const auto p2 = doc.addPoint("2", 100.0, 0.0, 101.0, "CORNER");
    const auto p3 = doc.addPoint("3", 100.0, 100.0, 104.0, "CORNER");
    const auto p4 = doc.addPoint("4", 0.0, 100.0, 102.0, "CORNER");
    const auto p5 = doc.addPoint("5", 50.0, 50.0, 103.0, "CENTER");
    const auto surfaceId = doc.buildTinSurface("Existing Ground", {p1, p2, p3, p4, p5});
    const auto* surface = doc.findSurface(surfaceId);
    expect(surface != nullptr && surface->triangles.size() == 4,
           "interior point participates in four Delaunay triangles");

    const auto alignmentId = doc.createAlignment("Road A", 1000.0);
    expect(doc.addAlignmentTangent(alignmentId, 0.0, 40.0, 100.0, 40.0),
           "alignment tangent added");
    expect(doc.addAlignmentCurve(alignmentId, 100.0, 60.0, 20.0, -1.5707963267948966,
                                 0.0, false), "alignment curve added");
    const auto profileId = doc.createProfile("Finished Grade", alignmentId);
    expect(doc.addProfilePVI(profileId, 1000.0, 100.0), "first PVI added");
    expect(doc.addProfilePVI(profileId, 1100.0, 102.0, 30.0), "second PVI added");

    const auto serialized = doc.serializeNative();
    std::string error;
    auto restored = mydesk::civil::CivilDocument::deserializeNative(serialized, &error);
    expect(error.empty(), "complete native document parses");
    expect(restored.surfaces().size() == 1 && restored.surfaces().front().triangles.size() == 4,
           "TIN topology roundtrips");
    expect(restored.alignments().size() == 1 && restored.alignments().front().segments.size() == 2,
           "alignment entities roundtrip");
    expect(restored.profiles().size() == 1 && restored.profiles().front().pvis.size() == 2,
           "profile PVIs roundtrip");
    expect(restored.updatePoint(p5, "5A", 55.0, 50.0, 103.5, "CENTER EDIT"),
           "COGO point updates");
    expect(restored.findSurface(surfaceId)->triangles.size() == 4,
           "referencing surface retriangulates after point edit");
}

} // namespace

int main() {
    try {
        const char* kRegistered1 = "test_cogo_and_tin_surface_volumes";
        (void)kRegistered1;
        test_cogo_and_tin_surface_volumes();
        std::cout << "[PASS] test_cogo_and_tin_surface_volumes\n";

        const char* kRegistered2 = "test_alignment_stationing_and_profile";
        (void)kRegistered2;
        test_alignment_stationing_and_profile();
        std::cout << "[PASS] test_alignment_stationing_and_profile\n";

        const char* kRegistered3 = "test_landxml_export_and_native_persistence";
        (void)kRegistered3;
        test_landxml_export_and_native_persistence();
        std::cout << "[PASS] test_landxml_export_and_native_persistence\n";

        const char* kRegistered4 = "test_delaunay_and_complete_native_roundtrip";
        (void)kRegistered4;
        test_delaunay_and_complete_native_roundtrip();
        std::cout << "[PASS] test_delaunay_and_complete_native_roundtrip\n";

        std::cout << "4 passed, 0 failed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cout << "[FAIL] " << ex.what() << "\n0 passed, 1 failed\n";
        return EXIT_FAILURE;
    }
}
