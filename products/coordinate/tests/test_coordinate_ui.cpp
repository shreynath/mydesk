#include "main_window.hpp"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMetaObject>
#include <QTableWidget>
#include <QTreeWidget>

#include <iostream>
#include <stdexcept>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        using namespace mydesk::coordinate;
        CoordinateDocument document;
        document.setName("GUI integration");
        const auto architecture = document.addModel("Architecture", "arch.ifc", "Architecture");
        const auto services = document.addModel("Services", "mep.ifc", "MEP");
        const auto wall = document.addElementToModel(architecture, "Wall 01", "Wall",
                                                      {0, 0, 0, 5, 0.3, 3});
        document.addElementToModel(services, "Duct 01", "Duct", {2, -1, 2, 3, 1, 2.5});
        const auto test = document.createClashTest("Architecture vs services", ClashType::Hard,
                                                   0.001);
        document.setClashTestSets(test, {architecture}, {services});
        expect(document.runClashTest(test) == 1, "GUI fixture has one clash");

        CoordinateMainWindow window;
        window.replaceDocument(std::move(document));
        window.show();
        QApplication::processEvents();
        auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("federatedModelTree"));
        auto* results = window.findChild<QTableWidget*>(QStringLiteral("clashResultsTable"));
        auto* viewport = window.findChild<QWidget*>(QStringLiteral("coordinationViewport"));
        expect(tree != nullptr && tree->topLevelItemCount() == 2,
               "federated model tree mirrors document models");
        expect(tree->topLevelItem(0)->childCount() == 1,
               "federated model tree mirrors model elements");
        expect(results != nullptr && results->rowCount() == 1,
               "clash results table mirrors engine results");
        expect(viewport != nullptr, "coordination viewport is constructed");
        expect(!window.grab().isNull(), "offscreen desktop workspace renders successfully");

        tree->setCurrentItem(tree->topLevelItem(0)->child(0));
        QApplication::processEvents();
        auto* name = window.findChild<QLineEdit*>(QStringLiteral("elementNameEdit"));
        auto* maxX = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementBound3Edit"));
        expect(name != nullptr && maxX != nullptr, "property editors are available");
        name->setText(QStringLiteral("Wall 01 coordinated"));
        maxX->setValue(6.0);
        expect(QMetaObject::invokeMethod(&window, "applyElementProperties", Qt::DirectConnection),
               "property apply slot is invokable");
        expect(window.document().findElement(wall)->name == "Wall 01 coordinated",
               "property panel edits the engine document");
        expect(window.document().findElement(wall)->bounds.maxX == 6.0,
               "property panel edits geometry bounds");
        expect(QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection),
               "undo slot is invokable");
        expect(window.document().findElement(wall)->name == "Wall 01",
               "GUI undo delegates to the document undo stack");
        std::cout << "[PASS] coordinate Qt document binding and property workflow\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << error.what() << '\n';
        return 1;
    }
}
