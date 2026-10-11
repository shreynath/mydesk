// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class FusionUiTest final : public QObject {
    Q_OBJECT
private slots:
    void authorEditManufacturePersist() {
        // "test_fusion_desktop_workflow"
        FusionMainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        window.createStarterPart();
        QCOMPARE(window.document().timeline().size(), std::size_t(2));
        QCOMPARE(window.document().bodies().size(), std::size_t(1));
        const double originalVolume = window.document().bodies().front().volumeMm3;
        QVERIFY(window.setSelectedParameter("depth", 20.0));
        QVERIFY(window.document().bodies().front().volumeMm3 > originalVolume);

        window.addPocket();
        QCOMPARE(window.document().camSetups().size(), std::size_t(1));
        QVERIFY(window.document().camSetups().front().toolpath.size() > 10);
        window.addBoard();
        QVERIFY(window.document().ecadBoard().has_value());
        window.undo();
        QVERIFY(!window.document().ecadBoard().has_value());
        window.redo();
        QVERIFY(window.document().ecadBoard().has_value());

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString project = directory.filePath("assembly.myfusion");
        const QString program = directory.filePath("pocket.nc");
        QVERIFY(window.saveDocument(project));
        QVERIFY(window.exportGCode(program));
        QFile gcode(program);
        QVERIFY(gcode.open(QIODevice::ReadOnly));
        QVERIFY(gcode.readAll().contains("G01"));

        FusionMainWindow reopened;
        QVERIFY(reopened.openDocument(project));
        QCOMPARE(reopened.document().timeline().size(), std::size_t(4));
        QCOMPARE(reopened.document().bodies().size(), std::size_t(1));
        QCOMPARE(reopened.document().camSetups().size(), std::size_t(1));
        QVERIFY(reopened.document().ecadBoard().has_value());
    }
};

QTEST_MAIN(FusionUiTest)
#include "test_fusion_ui.moc"
