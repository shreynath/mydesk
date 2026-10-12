// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"
#include <QApplication>
#include <QFile>
#include <QLineEdit>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

namespace {void expect(bool c,const char* m){if(!c)throw std::runtime_error(m);}void test_infra_qt_model_workflow(){InfraMainWindow w;w.createTerrain();w.createRoad();w.createBridge();w.createBuilding();expect(w.document().roads().size()==1,"road created");expect(w.document().bridges().size()==1,"bridge created");expect(w.document().buildings().size()==1,"building created");auto id=w.document().roads().front().id;w.selectObject(InfraMainWindow::Kind::Road,id);auto* name=w.findChild<QLineEdit*>("roadNameEditor");expect(name,"road properties exist");name->setText("Transit Spine");w.applyProperties();expect(w.document().findRoad(id)->name=="Transit Spine","property edit reaches engine");w.undo();expect(w.document().findRoad(id)->name=="Primary Corridor","undo restores road");w.redo();expect(w.document().findRoad(id)->name=="Transit Spine","redo restores edit");QTemporaryDir d;expect(d.isValid(),"temporary directory");auto native=d.filePath("region.myinfra"),geo=d.filePath("region.geojson");expect(w.saveDocument(native),"save");expect(w.exportGeoJson(geo),"export");InfraMainWindow reopened;expect(reopened.openDocument(native),"open");expect(reopened.document().roads().size()==1&&reopened.document().bridges().size()==1&&reopened.document().buildings().size()==1,"complete reopen");QFile f(geo);expect(f.open(QIODevice::ReadOnly)&&f.readAll().contains("FeatureCollection"),"GeoJSON content");}}
int main(int argc,char** argv){QApplication app(argc,argv);try{const char* registered="test_infra_qt_model_workflow";(void)registered;test_infra_qt_model_workflow();std::cout<<"Infra Qt workflow passed\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
