// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"
#include "mill_viewport.hpp"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

class MillUiTest final : public QObject {
    Q_OBJECT
private slots:
    void camAuthoringPersistenceAndPost();
};

void MillUiTest::camAuthoringPersistenceAndPost() {
    // "camAuthoringPersistenceAndPost"
    MillMainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCOMPARE(window.document().tool_count(), std::size_t(1));
    QVERIFY(!window.document().can_undo());

    window.createBoundary();
    QCOMPARE(window.document().boundary_count(), std::size_t(1));
    window.createOperation();
    QCOMPARE(window.document().toolpath_count(), std::size_t(1));
    QVERIFY(window.document().validate_toolpath(0).collision_free);

    auto* stepdown = window.findChild<QDoubleSpinBox*>("operationStepdown");
    QVERIFY(stepdown);
    const std::size_t oldMoves = window.document().toolpath(0).moves.size();
    stepdown->setValue(1.0);
    window.applyProperties();
    QCOMPARE(window.document().toolpath(0).params.stepdown_mm, 1.0);
    QVERIFY(window.document().toolpath(0).moves.size() > oldMoves);
    window.undo();
    QVERIFY(window.document().toolpath(0).params.stepdown_mm > 1.0);
    window.redo();
    QCOMPARE(window.document().toolpath(0).params.stepdown_mm, 1.0);

    auto* viewport = window.findChild<MillViewport*>("machiningViewport");
    QVERIFY(viewport);
    viewport->setTopView(true);
    QVERIFY(viewport->topView());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString project = directory.filePath("workflow.mymill");
    const QString gcode = directory.filePath("workflow.nc");
    QVERIFY(window.saveDocument(project));
    QVERIFY(window.exportGcode(gcode));
    QVERIFY(QFileInfo(gcode).size() > 100);

    MillMainWindow reopened;
    QVERIFY(reopened.openDocument(project));
    QCOMPARE(reopened.document().tool_count(), std::size_t(1));
    QCOMPARE(reopened.document().boundary_count(), std::size_t(1));
    QCOMPARE(reopened.document().toolpath_count(), std::size_t(1));
    QCOMPARE(reopened.document().toolpath(0).params.stepdown_mm, 1.0);
}

QTEST_MAIN(MillUiTest)
#include "test_mill_ui.moc"
