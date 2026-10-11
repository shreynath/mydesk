#include "main_window.hpp"

#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSlider>
#include <QTreeWidget>
#include <QtTest/QTest>

#include <iostream>
#include <stdexcept>

namespace {
void expect(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

void test_animate_qt_scene_timeline_workflow() {
    mydesk::animate::ui::MainWindow window;
    window.show();
    QApplication::processEvents();
    auto* create = window.findChild<QAction*>("createBoxAction");
    auto* outliner = window.findChild<QTreeWidget*>("sceneOutliner");
    auto* tx = window.findChild<QDoubleSpinBox*>("translateX");
    auto* apply = window.findChild<QPushButton*>("applyChannelsButton");
    auto* timeline = window.findChild<QSlider*>("timelineSlider");
    auto* keyValue = window.findChild<QDoubleSpinBox*>("keyValue");
    auto* setKey = window.findChild<QPushButton*>("setKeyButton");
    auto* undo = window.findChild<QAction*>("undoAction");
    expect(create && outliner && tx && apply && timeline && keyValue && setKey && undo,
           "Animate workspace controls are present");
    create->trigger();
    expect(window.document().node_count()==1 && window.document().mesh_count()==1,
           "modeling action creates an engine mesh node");
    expect(outliner->topLevelItemCount()==1,"outliner mirrors the scene graph");
    tx->setValue(4.0); QTest::mouseClick(apply,Qt::LeftButton);
    const auto id=window.document().node_ids().front();
    expect(window.document().node(id).translate.x==4.0,"channel box edits the scene node");
    timeline->setValue(24); keyValue->setValue(10.0); QTest::mouseClick(setKey,Qt::LeftButton);
    expect(window.document().node(id).anim.has_value(),"timeline key operation creates curves");
    expect(window.document().node(id).anim->tx.key_count()==2,"key curve keeps start and keyed frames");
    undo->trigger();
    expect(!window.document().node(id).anim.has_value(),"desktop undo restores the unkeyed node");
}
}

int main(int argc,char** argv){QApplication app(argc,argv);try{test_animate_qt_scene_timeline_workflow();std::cout<<"Animate Qt workflow passed\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
