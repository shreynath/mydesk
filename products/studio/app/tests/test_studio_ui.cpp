// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QtTest/QTest>

#include <cstdio>

using mydesk::studio::ui::MainWindow;

namespace {

template <typename Widget>
Widget* required(MainWindow& window, const char* name, int& failures) {
    Widget* widget = window.findChild<Widget*>(name);
    if (!widget) {
        std::fprintf(stderr, "Missing Studio UI object: %s\n", name);
        ++failures;
    }
    return widget;
}

} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    int failures = 0;
    MainWindow window;
    window.show();
    const bool exposed = QTest::qWaitForWindowExposed(&window);
    (void)exposed;

    auto* primitiveKind = required<QComboBox>(window, "createPrimitiveKind", failures);
    auto* objectName = required<QLineEdit>(window, "createObjectName", failures);
    auto* length = required<QDoubleSpinBox>(window, "createBoxLength", failures);
    auto* width = required<QDoubleSpinBox>(window, "createBoxWidth", failures);
    auto* height = required<QDoubleSpinBox>(window, "createBoxHeight", failures);
    auto* create = required<QPushButton>(window, "createPrimitiveButton", failures);
    auto* sceneTree = required<QTreeWidget>(window, "sceneTree", failures);
    auto* translateX = required<QDoubleSpinBox>(window, "translateX", failures);
    auto* applyTransform = required<QPushButton>(window, "applyTransformButton", failures);
    auto* modifierKind = required<QComboBox>(window, "modifierKind", failures);
    auto* primary = required<QDoubleSpinBox>(window, "modifierPrimary", failures);
    auto* addModifier = required<QPushButton>(window, "addModifierButton", failures);
    auto* undo = required<QAction>(window, "undoAction", failures);
    auto* redo = required<QAction>(window, "redoAction", failures);
    if (failures != 0) return 1;

    primitiveKind->setCurrentIndex(0);
    objectName->setText(QStringLiteral("UiBox"));
    length->setValue(12.0);
    width->setValue(8.0);
    height->setValue(4.0);
    QTest::mouseClick(create, Qt::LeftButton);
    if (window.document().objects().size() != 1 || sceneTree->topLevelItemCount() != 1) {
        std::fprintf(stderr, "Create workflow did not update document and scene explorer\n");
        ++failures;
    }

    translateX->setValue(15.0);
    QTest::mouseClick(applyTransform, Qt::LeftButton);
    if (window.document().objects().front().transform.translation.x != 15.0) {
        std::fprintf(stderr, "Transform workflow did not update document\n");
        ++failures;
    }

    modifierKind->setCurrentIndex(2);
    primary->setValue(45.0);
    QTest::mouseClick(addModifier, Qt::LeftButton);
    if (window.document().objects().front().modifierStack.size() != 1) {
        std::fprintf(stderr, "Modifier workflow did not update the stack\n");
        ++failures;
    }

    undo->trigger();
    if (!window.document().objects().front().modifierStack.empty()) {
        std::fprintf(stderr, "Undo did not remove the modifier\n");
        ++failures;
    }
    redo->trigger();
    if (window.document().objects().front().modifierStack.size() != 1) {
        std::fprintf(stderr, "Redo did not restore the modifier\n");
        ++failures;
    }

    return failures == 0 ? 0 : 1;
}
