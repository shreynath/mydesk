#include "mydesk/buildings/bim_viewport.hpp"
#include "mydesk/buildings/main_window.hpp"

#include <QAction>
#include <QApplication>
#include <QToolBar>
#include <QTreeWidget>
#include <QtTest/QTest>

#include <iostream>
#include <stdexcept>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

QAction* toolbarAction(QToolBar* toolbar, const QString& text) {
    for (QAction* action : toolbar->actions()) {
        if (action->text() == text) return action;
    }
    return nullptr;
}

void test_buildings_qt_authoring_workflow() {
    using namespace mydesk::buildings;
    MainWindow window;
    window.resize(1100, 720);
    window.show();
    QApplication::processEvents();

    auto* viewport = static_cast<BimViewport*>(
        window.findChild<QWidget*>(QStringLiteral("bimViewport")));
    auto* browser = window.findChild<QTreeWidget*>(QStringLiteral("projectBrowser"));
    auto* toolbar = window.findChild<QToolBar*>(QStringLiteral("BimAuthoringToolbar"));
    expect(viewport && browser && toolbar, "BIM workspace widgets are constructed");
    expect(browser->topLevelItemCount() == 4, "project browser exposes BIM categories");

    QAction* wall = toolbarAction(toolbar, QStringLiteral("Wall"));
    QAction* door = toolbarAction(toolbar, QStringLiteral("Door"));
    QAction* view3d = toolbarAction(toolbar, QStringLiteral("3D"));
    expect(wall && door && view3d, "authoring and view actions are available");

    wall->trigger();
    const QPoint center(viewport->width() / 2, viewport->height() / 2);
    QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, center);
    QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, center + QPoint(180, 0));
    QApplication::processEvents();
    expect(window.controller().document().walls.size() == 1,
           "two viewport clicks create a semantic wall");

    door->trigger();
    QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, center + QPoint(90, 0));
    QApplication::processEvents();
    expect(window.controller().document().doors.size() == 1,
           "opening tool creates a wall-hosted door");

    view3d->setChecked(true);
    QApplication::processEvents();
    expect(!viewport->grab().isNull(), "axonometric viewport renders the authored document");

    QAction* undo = nullptr;
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->shortcut() == QKeySequence::Undo) { undo = action; break; }
    }
    expect(undo != nullptr, "undo action is available");
    undo->trigger();
    expect(window.controller().document().doors.empty(),
           "desktop undo removes the hosted opening transaction");
}

} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        test_buildings_qt_authoring_workflow();
        std::cout << "MyDesk Buildings Qt authoring workflow passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
