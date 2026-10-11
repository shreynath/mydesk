// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

class SurfaceUiTest final : public QObject {
    Q_OBJECT
private slots:
    // "test_surface_desktop_workflow"
    void createEditUndoSaveReopen() {
        SurfaceMainWindow window;
        window.show();
        QTest::qWait(20);

        auto* create = window.findChild<QAction*>("createPatchAction");
        QVERIFY(create);
        create->trigger();
        QCOMPARE(window.document().surfaces().size(), std::size_t{1});

        const auto id = window.document().surfaces().front().id;
        window.selectSurface(id, 0);
        auto* z = window.findChild<QDoubleSpinBox*>("controlZSpin");
        auto* apply = window.findChild<QPushButton*>("applyPropertiesButton");
        QVERIFY(z); QVERIFY(apply);
        z->setValue(12.5);
        QTest::mouseClick(apply, Qt::LeftButton);
        QCOMPARE(window.document().findSurface(id)->controlGrid.front().p.z, 12.5);

        QAction* undoAction = nullptr;
        for (auto* action : window.findChildren<QAction*>()) if (action->shortcut() == QKeySequence::Undo) undoAction = action;
        QVERIFY(undoAction);
        undoAction->trigger();
        QCOMPARE(window.document().findSurface(id)->controlGrid.front().p.z, 0.0);
        QAction* redoAction = nullptr;
        for (auto* action : window.findChildren<QAction*>()) if (action->shortcut() == QKeySequence::Redo) redoAction = action;
        QVERIFY(redoAction); redoAction->trigger();
        QCOMPARE(window.document().findSurface(id)->controlGrid.front().p.z, 12.5);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("panel.mysurface");
        QVERIFY(window.saveDocument(path));
        SurfaceMainWindow reopened;
        QVERIFY(reopened.openDocument(path));
        QCOMPARE(reopened.document().surfaces().size(), std::size_t{1});
        QCOMPARE(reopened.document().surfaces().front().controlGrid.front().p.z, 12.5);
    }

    // "test_surface_ui_seam_tools"
    void seamToolsUseEngine() {
        SurfaceMainWindow window;
        auto* create = window.findChild<QAction*>("createBicubicPatchAction");
        QVERIFY(create); create->trigger(); create->trigger();
        QCOMPARE(window.document().surfaces().size(), std::size_t{2});
        auto* align = window.findChild<QPushButton*>("alignSeamButton");
        QVERIFY(align); QTest::mouseClick(align, Qt::LeftButton);
        QVERIFY(window.document().canUndo());
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    SurfaceUiTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_surface_ui.moc"
