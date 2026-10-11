#include "mechanical_session.hpp"

#include <QCoreApplication>
#include <QTemporaryDir>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

// Capability-manifest registrations: "part_workflow_round_trip",
// "assembly_browser_workflow_round_trip".

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void part_workflow_round_trip() {
    mydesk::mechanical::desktop::MechanicalSession session;
    QString error;
    expect(session.createBox("Base", 40.0, 20.0, 10.0, &error),
           error.toStdString().c_str());
    expect(session.part().hasSolid(), "box must create an evaluated solid");
    expect(std::abs(session.part().solidVolume() - 8000.0) < 1e-6,
           "box volume must be engine-derived");

    const auto width = session.part().findParameterByName("Base Sketch.Width");
    expect(width != nullptr, "box width parameter is exposed");
    expect(session.setParameter(width->id, 50.0, &error), error.toStdString().c_str());
    expect(std::abs(session.part().solidVolume() - 10000.0) < 1e-6,
           "property edit regenerates OCCT geometry");
    expect(session.undo(&error), error.toStdString().c_str());
    expect(std::abs(session.part().solidVolume() - 8000.0) < 1e-6,
           "undo restores prior feature parameters and geometry");
    expect(session.redo(&error), error.toStdString().c_str());
    expect(std::abs(session.part().solidVolume() - 10000.0) < 1e-6,
           "redo restores edited geometry");

    expect(session.createHole("Mounting Hole", 2.0, 10.0, 10.0, 10.0, &error),
           error.toStdString().c_str());
    expect(session.part().solidVolume() < 10000.0, "hole performs a real B-rep cut");

    QTemporaryDir directory;
    expect(directory.isValid(), "temporary directory");
    const QString native = directory.filePath("bracket.mydesk-mech");
    const QString step = directory.filePath("bracket.step");
    expect(session.saveAs(native, &error), error.toStdString().c_str());
    expect(!session.isDirty(), "successful save marks document clean");
    expect(session.exportStep(step, &error), error.toStdString().c_str());

    mydesk::mechanical::desktop::MechanicalSession loaded;
    expect(loaded.open(native, &error), error.toStdString().c_str());
    expect(loaded.part().features().size() == session.part().features().size(),
           "native open restores feature history");
    expect(std::abs(loaded.part().solidVolume() - session.part().solidVolume()) < 1e-6,
           "native open regenerates matching geometry");
}

void assembly_browser_workflow_round_trip() {
    mydesk::mechanical::desktop::MechanicalSession session;
    QString error;
    expect(session.createBox("Component", 10, 10, 10, &error), error.toStdString().c_str());
    session.newAssembly();
    expect(session.insertCurrentPart("Component:1", 0, 0, 0, &error),
           error.toStdString().c_str());
    expect(session.insertCurrentPart("Component:2", 25, 0, 0, &error),
           error.toStdString().c_str());
    expect(session.assembly().occurrences().size() == 2, "two real occurrences inserted");
    expect(!session.displayShape().IsNull(), "assembly viewport shape is evaluated");
    expect(session.undo(&error), error.toStdString().c_str());
    expect(session.assembly().occurrences().size() == 1, "assembly insertion undo");
    expect(session.redo(&error), error.toStdString().c_str());
    expect(session.assembly().occurrences().size() == 2, "assembly insertion redo");

    QTemporaryDir directory;
    const QString native = directory.filePath("fixture.mydesk-assy");
    expect(session.saveAs(native, &error), error.toStdString().c_str());
    mydesk::mechanical::desktop::MechanicalSession loaded;
    expect(loaded.open(native, &error), error.toStdString().c_str());
    expect(loaded.kind() == mydesk::mechanical::desktop::MechanicalSession::DocumentKind::Assembly,
           "assembly document kind detected");
    expect(loaded.assembly().occurrences().size() == 2, "assembly occurrences reopen");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    try {
        part_workflow_round_trip();
        assembly_browser_workflow_round_trip();
        std::cout << "Mechanical desktop session tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
