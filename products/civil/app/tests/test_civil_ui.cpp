// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QFile>
#include <QTemporaryDir>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testCivilAuthoringPersistenceAndUndo() {
    CivilMainWindow window;
    const auto p1 = window.createPoint("100", 0.0, 0.0, 100.0, "BM");
    window.createPoint("101", 100.0, 0.0, 102.0, "TOPO");
    window.createPoint("102", 100.0, 80.0, 105.0, "TOPO");
    window.createPoint("103", 0.0, 80.0, 101.0, "TOPO");
    window.createPoint("104", 45.0, 35.0, 103.0, "TOPO");
    const auto surface = window.createSurface("Existing Ground");
    const auto alignment = window.createAlignment("Road A");
    const auto profile = window.createProfile("Finished Grade");
    expect(surface != 0 && alignment != 0 && profile != 0, "civil objects created");
    expect(window.document().surfaces().front().triangles.size() == 4,
           "five-point surface has Delaunay triangles around interior point");

    window.selectObject(CivilMainWindow::SelectionKind::Point, p1);
    auto* elevation = window.findChild<QDoubleSpinBox*>("pointElevationEditor");
    expect(elevation != nullptr, "point property editor exists");
    elevation->setValue(111.25);
    window.applyProperties();
    expect(std::abs(window.document().findPoint(p1)->elevation - 111.25) < 1e-9,
           "property edit reaches document");
    window.undo();
    expect(std::abs(window.document().findPoint(p1)->elevation - 100.0) < 1e-9,
           "undo restores civil document snapshot");
    window.redo();
    expect(std::abs(window.document().findPoint(p1)->elevation - 111.25) < 1e-9,
           "redo reapplies civil edit");

    QTemporaryDir directory;
    expect(directory.isValid(), "temporary directory available");
    const QString nativePath = directory.filePath("site.mydesk-civil");
    const QString xmlPath = directory.filePath("site.xml");
    expect(window.saveDocument(nativePath), "native save succeeds");
    expect(window.exportLandXml(xmlPath), "LandXML export succeeds");
    CivilMainWindow reopened;
    expect(reopened.openDocument(nativePath), "native reopen succeeds");
    expect(reopened.document().points().size() == 5, "points persist");
    expect(reopened.document().surfaces().size() == 1, "surface persists");
    expect(reopened.document().alignments().size() == 1, "alignment persists");
    expect(reopened.document().profiles().size() == 1, "profile persists");
    QFile xml(xmlPath);
    expect(xml.open(QIODevice::ReadOnly), "LandXML readable");
    const QByteArray contents = xml.readAll();
    expect(contents.contains("<Surface") && contents.contains("<Alignment") &&
           contents.contains("<ProfAlignment"), "LandXML contains civil definitions");
}
}

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        const char* kRegisteredWorkflow = "testCivilAuthoringPersistenceAndUndo";
        (void)kRegisteredWorkflow;
        testCivilAuthoringPersistenceAndUndo();
        std::cout << "Civil Qt workflow passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "FAIL: " << exception.what() << '\n';
        return 1;
    }
}
