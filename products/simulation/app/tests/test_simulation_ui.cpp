// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QApplication>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

class SimulationUiTest final : public QObject {
    Q_OBJECT
private slots:
void authorSolvePersist(){
    // "test_simulation_desktop_workflow"
    SimulationMainWindow window;window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.loadCantilever();QCOMPARE(window.document().node_count(),std::size_t(4));QCOMPARE(window.document().element_count(),std::size_t(2));
    window.solveStatic();QVERIFY(window.staticResult().converged);QVERIFY(window.staticResult().max_displacement>0.0);
    window.addNode();QCOMPARE(window.document().node_count(),std::size_t(5));window.undo();QCOMPARE(window.document().node_count(),std::size_t(4));
    window.solveStatic();QVERIFY(window.staticResult().converged);
    QTemporaryDir directory;QVERIFY(directory.isValid());const QString project=directory.filePath("cantilever.mdsim");const QString report=directory.filePath("static.txt");QVERIFY(window.saveDocument(project));QVERIFY(window.exportStaticReport(report));QVERIFY(QFileInfo(report).size()>100);
    SimulationMainWindow reopened;QVERIFY(reopened.openDocument(project));QCOMPARE(reopened.document().node_count(),std::size_t(4));QCOMPARE(reopened.document().element_count(),std::size_t(2));
}
};
QTEST_MAIN(SimulationUiTest)
#include "test_simulation_ui.moc"
