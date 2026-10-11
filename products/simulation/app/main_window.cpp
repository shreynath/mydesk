// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStatusBar>
#include <QTableWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <filesystem>

using namespace mydesk::simulation;

namespace {
constexpr int kKindRole=Qt::UserRole,kIndexRole=Qt::UserRole+1;
std::filesystem::path nativePath(const QString& path){
#ifdef _WIN32
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toStdString());
#endif
}
QTableWidgetItem* cell(const QString& value){auto* item=new QTableWidgetItem(value);item->setFlags(item->flags()&~Qt::ItemIsEditable);return item;}
QString bcName(BCKind kind){switch(kind){case BCKind::FixedX:return"Fixed X";case BCKind::FixedY:return"Fixed Y";case BCKind::FixedXY:return"Fixed XY";case BCKind::PrescribedX:return"Prescribed X";case BCKind::PrescribedY:return"Prescribed Y";}return{};}
}

SimulationMainWindow::SimulationMainWindow(QWidget* parent):QMainWindow(parent){
    setObjectName("simulationMainWindow");setMinimumSize(1100,720);resize(1440,900);buildUi();newDocument();
    QSettings settings;restoreGeometry(settings.value("simulation/geometry").toByteArray());restoreState(settings.value("simulation/state").toByteArray());
}

void SimulationMainWindow::buildUi(){
    viewport_=new SimulationViewport(this);setCentralWidget(viewport_);
    auto* modelDock=new QDockWidget("Simulation Model",this);modelDock->setObjectName("modelDock");tree_=new QTreeWidget(modelDock);tree_->setObjectName("modelTree");tree_->setHeaderLabels({"Entity","Definition"});modelDock->setWidget(tree_);addDockWidget(Qt::LeftDockWidgetArea,modelDock);
    connect(tree_,&QTreeWidget::itemSelectionChanged,this,[this]{auto items=tree_->selectedItems();if(items.empty())return;select({static_cast<Kind>(items.front()->data(0,kKindRole).toInt()),items.front()->data(0,kIndexRole).toULongLong()});});
    connect(viewport_,&SimulationViewport::entitySelected,this,[this](bool element,std::uint32_t id){select({element?Kind::Element:Kind::Node,id});rebuildTree();});

    auto* propDock=new QDockWidget("Input Properties",this);propDock->setObjectName("propertiesDock");auto* prop=new QWidget(propDock);auto* layout=new QVBoxLayout(prop);selectionLabel_=new QLabel("No selection",prop);selectionLabel_->setWordWrap(true);layout->addWidget(selectionLabel_);auto* form=new QFormLayout;
    nameEdit_=new QLineEdit(prop);nameEdit_->setObjectName("propertyName");
    auto makeSpin=[&](const char* name){auto* s=new QDoubleSpinBox(prop);s->setObjectName(name);s->setDecimals(8);s->setRange(-1e15,1e15);return s;};
    valueA_=makeSpin("propertyA");valueB_=makeSpin("propertyB");valueC_=makeSpin("propertyC");valueD_=makeSpin("propertyD");
    form->addRow("Name",nameEdit_);form->addRow("X / E (GPa)",valueA_);form->addRow("Y / Poisson",valueB_);form->addRow("Density",valueC_);form->addRow("Yield (MPa)",valueD_);layout->addLayout(form);auto* apply=new QPushButton("Apply input edit",prop);apply->setObjectName("applyPropertiesButton");layout->addWidget(apply);layout->addStretch();propDock->setWidget(prop);addDockWidget(Qt::RightDockWidgetArea,propDock);connect(apply,&QPushButton::clicked,this,&SimulationMainWindow::applyProperties);

    auto* resultsDock=new QDockWidget("Analysis Results",this);resultsDock->setObjectName("resultsDock");auto* results=new QWidget(resultsDock);auto* resultsLayout=new QVBoxLayout(results);displayMode_=new QComboBox(results);displayMode_->setObjectName("displayMode");displayMode_->addItems({"Model","Displacement contour","Von Mises contour","Mode shape"});resultsLayout->addWidget(displayMode_);resultSummary_=new QLabel("Model has not been solved.",results);resultSummary_->setWordWrap(true);resultsLayout->addWidget(resultSummary_);resultTable_=new QTableWidget(results);resultTable_->setObjectName("resultTable");resultTable_->setColumnCount(4);resultTable_->setHorizontalHeaderLabels({"ID","Result A","Result B","Resultant"});resultTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);resultsLayout->addWidget(resultTable_);resultsDock->setWidget(results);addDockWidget(Qt::BottomDockWidgetArea,resultsDock);
    connect(displayMode_,&QComboBox::currentIndexChanged,this,[this](int index){viewport_->setDisplayMode(static_cast<SimulationViewport::DisplayMode>(index));});

    auto* file=menuBar()->addMenu("&File");file->addAction("&New",QKeySequence::New,this,&SimulationMainWindow::newDocument);file->addAction("&Open…",QKeySequence::Open,this,&SimulationMainWindow::openDialog);file->addAction("&Save",QKeySequence::Save,this,&SimulationMainWindow::save);file->addAction("Save &As…",QKeySequence::SaveAs,this,&SimulationMainWindow::saveAs);exportAction_=file->addAction("Export Static Report…",this,&SimulationMainWindow::exportReportDialog);
    auto* edit=menuBar()->addMenu("&Edit");undoAction_=edit->addAction("&Undo",QKeySequence::Undo,this,&SimulationMainWindow::undo);redoAction_=edit->addAction("&Redo",QKeySequence::Redo,this,&SimulationMainWindow::redo);
    auto* model=menuBar()->addMenu("&Model");auto* benchmark=model->addAction("Load Cantilever Benchmark",this,&SimulationMainWindow::loadCantilever);benchmark->setObjectName("cantileverAction");auto* material=model->addAction("Add Material",this,&SimulationMainWindow::addMaterial);auto* node=model->addAction("Add Node",this,&SimulationMainWindow::addNode);node->setObjectName("addNodeAction");auto* element=model->addAction("Create CST from Last 3 Nodes",this,&SimulationMainWindow::addElement);auto* fix=model->addAction("Fix Selected Node",this,&SimulationMainWindow::fixNode);auto* load=model->addAction("Load Selected Node",this,&SimulationMainWindow::loadNode);
    auto* analysis=menuBar()->addMenu("&Analysis");auto* solve=analysis->addAction("Solve Linear Static",this,&SimulationMainWindow::solveStatic);solve->setObjectName("solveStaticAction");analysis->addAction("Solve Modal",this,&SimulationMainWindow::solveModal);
    auto* view=menuBar()->addMenu("&View");view->addAction("Fit Model",QKeySequence(Qt::Key_F),viewport_,&SimulationViewport::fitView);
    auto* toolbar=addToolBar("Simulation");toolbar->setObjectName("simulationToolbar");toolbar->setMovable(false);toolbar->addAction(benchmark);toolbar->addAction(material);toolbar->addAction(node);toolbar->addAction(element);toolbar->addSeparator();toolbar->addAction(fix);toolbar->addAction(load);toolbar->addSeparator();toolbar->addAction(solve);toolbar->addAction(undoAction_);toolbar->addAction(redoAction_);
    statusBar()->showMessage("Ready — 2D plane-stress CST model");
}

template<class Action> bool SimulationMainWindow::transact(const std::string& label,Action&& action){
    try{document_.begin_transaction(label);action();document_.commit_transaction();invalidateResults();setDirty(true);rebuildTree();return true;}catch(const std::exception& error){if(document_.transaction_active())document_.rollback_transaction();QMessageBox::warning(this,"Simulation input",error.what());return false;}
}

void SimulationMainWindow::newDocument(){if(!maybeSave())return;document_={};filePath_.clear();selection_={};invalidateResults();viewport_->setDocument(&document_);rebuildTree();setDirty(false);}

void SimulationMainWindow::loadCantilever(){
    if(dirty_&&!maybeSave())return;document_={};Material steel;steel.name="Steel";steel.youngs_modulus_pa=200e9;steel.poissons_ratio=.3;steel.yield_strength_pa=250e6;const auto m=document_.add_material(steel);const auto n0=document_.add_node(0,0),n1=document_.add_node(.2,0),n2=document_.add_node(.2,.1),n3=document_.add_node(0,.1);document_.add_element({n0,n1,n2},m,.01);document_.add_element({n0,n2,n3},m,.01);document_.add_bc({n0,BCKind::FixedXY,0});document_.add_bc({n3,BCKind::FixedXY,0});document_.add_load({n1,0,-500});document_.add_load({n2,0,-500});filePath_.clear();selection_={Kind::Element,0};invalidateResults();viewport_->setDocument(&document_);rebuildTree();setDirty(true);statusBar()->showMessage("Loaded verified cantilever benchmark",4000);
}

void SimulationMainWindow::addMaterial(){transact("add material",[this]{Material m;m.name="Material "+std::to_string(document_.material_count()+1);document_.add_material(m);selection_={Kind::Material,document_.material_count()-1};});}
void SimulationMainWindow::addNode(){transact("add node",[this]{const double x=.05*static_cast<double>(document_.node_count());const auto id=document_.add_node(x,0);selection_={Kind::Node,id};});}
void SimulationMainWindow::addElement(){if(document_.material_count()==0){QMessageBox::warning(this,"Create element","Add a material first.");return;}if(document_.node_count()<3){QMessageBox::warning(this,"Create element","At least three nodes are required.");return;}transact("add CST element",[this]{const auto n=static_cast<std::uint32_t>(document_.node_count());const auto id=document_.add_element({n-3,n-2,n-1},0,.01);selection_={Kind::Element,id};});}
void SimulationMainWindow::fixNode(){if(selection_.kind!=Kind::Node)return;transact("fix node",[this]{document_.add_bc({static_cast<std::uint32_t>(selection_.index),BCKind::FixedXY,0});});}
void SimulationMainWindow::loadNode(){if(selection_.kind!=Kind::Node)return;transact("apply nodal load",[this]{document_.add_load({static_cast<std::uint32_t>(selection_.index),0,-1000});});}

void SimulationMainWindow::applyProperties(){
    if(selection_.kind==Kind::Node){transact("edit node",[this]{document_.set_node_coordinates(static_cast<std::uint32_t>(selection_.index),valueA_->value(),valueB_->value());});}
    else if(selection_.kind==Kind::Material){transact("edit material",[this]{auto m=document_.material(static_cast<std::uint32_t>(selection_.index));m.name=nameEdit_->text().trimmed().toStdString();m.youngs_modulus_pa=valueA_->value()*1e9;m.poissons_ratio=valueB_->value();m.density_kg_m3=valueC_->value();m.yield_strength_pa=valueD_->value()*1e6;document_.set_material(static_cast<std::uint32_t>(selection_.index),m);});}
}

void SimulationMainWindow::solveStatic(){staticResult_=document_.solve();viewport_->setStaticResult(&staticResult_);displayMode_->setCurrentIndex(staticResult_.converged?2:0);refreshResults();statusBar()->showMessage(QString::fromStdString(staticResult_.message),5000);}
void SimulationMainWindow::solveModal(){modalResult_=document_.solve_modal(6);viewport_->setModalResult(&modalResult_);displayMode_->setCurrentIndex(modalResult_.converged?3:0);refreshResults();statusBar()->showMessage(QString::fromStdString(modalResult_.message),5000);}
void SimulationMainWindow::invalidateResults(){staticResult_={};modalResult_={};viewport_->setStaticResult(nullptr);viewport_->setModalResult(nullptr);displayMode_->setCurrentIndex(0);refreshResults();}

void SimulationMainWindow::undo(){if(document_.undo()){invalidateResults();setDirty(true);rebuildTree();}}
void SimulationMainWindow::redo(){if(document_.redo()){invalidateResults();setDirty(true);rebuildTree();}}

void SimulationMainWindow::rebuildTree(){
    tree_->clear();auto group=[this](const QString& name){auto* item=new QTreeWidgetItem(tree_,{name,{}});item->setData(0,kKindRole,static_cast<int>(Kind::None));return item;};
    auto* materials=group(QStringLiteral("Materials (%1)").arg(document_.material_count()));for(std::size_t i=0;i<document_.material_count();++i){const auto&m=document_.material(static_cast<std::uint32_t>(i));auto* item=new QTreeWidgetItem(materials,{QString::fromStdString(m.name),QStringLiteral("E=%1 GPa").arg(m.youngs_modulus_pa/1e9)});item->setData(0,kKindRole,static_cast<int>(Kind::Material));item->setData(0,kIndexRole,QVariant::fromValue<qulonglong>(i));if(selection_.kind==Kind::Material&&selection_.index==i)tree_->setCurrentItem(item);}
    auto* mesh=group(QStringLiteral("Mesh (%1 nodes, %2 CST)").arg(document_.node_count()).arg(document_.element_count()));for(std::size_t i=0;i<document_.node_count();++i){const auto&n=document_.node(static_cast<std::uint32_t>(i));auto* item=new QTreeWidgetItem(mesh,{QStringLiteral("Node %1").arg(i),QStringLiteral("(%1, %2) m").arg(n.x).arg(n.y)});item->setData(0,kKindRole,static_cast<int>(Kind::Node));item->setData(0,kIndexRole,QVariant::fromValue<qulonglong>(i));if(selection_.kind==Kind::Node&&selection_.index==i)tree_->setCurrentItem(item);}for(std::size_t i=0;i<document_.element_count();++i){const auto&e=document_.element(static_cast<std::uint32_t>(i));auto* item=new QTreeWidgetItem(mesh,{QStringLiteral("CST %1").arg(i),QStringLiteral("N%1 N%2 N%3").arg(e.node_ids[0]).arg(e.node_ids[1]).arg(e.node_ids[2])});item->setData(0,kKindRole,static_cast<int>(Kind::Element));item->setData(0,kIndexRole,QVariant::fromValue<qulonglong>(i));if(selection_.kind==Kind::Element&&selection_.index==i)tree_->setCurrentItem(item);}
    auto* constraints=group(QStringLiteral("Constraints (%1)").arg(document_.boundary_condition_count()));for(std::size_t i=0;i<document_.boundary_condition_count();++i){const auto&bc=document_.boundary_condition(i);auto* item=new QTreeWidgetItem(constraints,{QStringLiteral("Node %1").arg(bc.node_id),bcName(bc.kind)});item->setData(0,kKindRole,static_cast<int>(Kind::Constraint));item->setData(0,kIndexRole,QVariant::fromValue<qulonglong>(i));}
    auto* loads=group(QStringLiteral("Loads (%1)").arg(document_.load_count()));for(std::size_t i=0;i<document_.load_count();++i){const auto&l=document_.load(i);auto* item=new QTreeWidgetItem(loads,{QStringLiteral("Node %1").arg(l.node_id),QStringLiteral("%1, %2 N").arg(l.fx).arg(l.fy)});item->setData(0,kKindRole,static_cast<int>(Kind::Load));item->setData(0,kIndexRole,QVariant::fromValue<qulonglong>(i));}
    tree_->expandAll();tree_->resizeColumnToContents(0);refreshProperties();undoAction_->setEnabled(document_.can_undo());redoAction_->setEnabled(document_.can_redo());
}

void SimulationMainWindow::select(Selection selection){selection_=selection;viewport_->setSelection(selection.kind==Kind::Element,static_cast<std::uint32_t>(selection.index));refreshProperties();}

void SimulationMainWindow::refreshProperties(){
    nameEdit_->setEnabled(false);for(auto*s:{valueA_,valueB_,valueC_,valueD_})s->setEnabled(false);
    if(selection_.kind==Kind::Node&&selection_.index<document_.node_count()){const auto&n=document_.node(static_cast<std::uint32_t>(selection_.index));selectionLabel_->setText(QStringLiteral("Node %1 coordinates (metres)").arg(selection_.index));valueA_->setValue(n.x);valueB_->setValue(n.y);valueA_->setEnabled(true);valueB_->setEnabled(true);}
    else if(selection_.kind==Kind::Material&&selection_.index<document_.material_count()){const auto&m=document_.material(static_cast<std::uint32_t>(selection_.index));selectionLabel_->setText(QStringLiteral("Material %1").arg(selection_.index));nameEdit_->setText(QString::fromStdString(m.name));valueA_->setValue(m.youngs_modulus_pa/1e9);valueB_->setValue(m.poissons_ratio);valueC_->setValue(m.density_kg_m3);valueD_->setValue(m.yield_strength_pa/1e6);nameEdit_->setEnabled(true);for(auto*s:{valueA_,valueB_,valueC_,valueD_})s->setEnabled(true);}
    else selectionLabel_->setText("Select a material or node to edit input properties.");
}

void SimulationMainWindow::refreshResults(){
    resultTable_->setRowCount(0);if(staticResult_.converged){resultSummary_->setText(QStringLiteral("Static converged · max displacement %1 mm · max von Mises %2 MPa").arg(staticResult_.max_displacement*1000,0,'g',6).arg(staticResult_.max_von_mises/1e6,0,'g',6));resultTable_->setRowCount(static_cast<int>(staticResult_.element_results.size()));for(int row=0;row<resultTable_->rowCount();++row){const auto&r=staticResult_.element_results[static_cast<std::size_t>(row)];resultTable_->setItem(row,0,cell(QString::number(r.element_id)));resultTable_->setItem(row,1,cell(QString::number(r.sigma_xx/1e6)));resultTable_->setItem(row,2,cell(QString::number(r.sigma_yy/1e6)));resultTable_->setItem(row,3,cell(QString::number(r.von_mises/1e6)));}}
    else if(modalResult_.converged){resultSummary_->setText(QStringLiteral("Modal converged · %1 flexible modes").arg(modalResult_.modes.size()));resultTable_->setRowCount(static_cast<int>(modalResult_.modes.size()));for(int row=0;row<resultTable_->rowCount();++row){const auto&m=modalResult_.modes[static_cast<std::size_t>(row)];resultTable_->setItem(row,0,cell(QString::number(m.mode_number)));resultTable_->setItem(row,1,cell(QString::number(m.frequency_hz)));resultTable_->setItem(row,2,cell(QString::number(m.eigenvalue_rad2_s2)));resultTable_->setItem(row,3,cell(QString::number(m.relative_residual)));}}
    else resultSummary_->setText("Model has not been solved.");exportAction_->setEnabled(staticResult_.converged);
}

void SimulationMainWindow::setDirty(bool dirty){dirty_=dirty;const QString name=filePath_.isEmpty()?"Untitled.mdsim":QFileInfo(filePath_).fileName();setWindowTitle(QStringLiteral("%1%2 — MyDesk Simulation").arg(dirty_?"• ":"",name));undoAction_->setEnabled(document_.can_undo());redoAction_->setEnabled(document_.can_redo());}
bool SimulationMainWindow::saveDocument(const QString& path){try{document_.save_native_file(nativePath(path));filePath_=path;setDirty(false);return true;}catch(const std::exception&e){QMessageBox::critical(this,"Save failed",e.what());return false;}}
bool SimulationMainWindow::openDocument(const QString& path){try{document_=SimulationDocument::load_native_file(nativePath(path));filePath_=path;selection_={};invalidateResults();viewport_->setDocument(&document_);rebuildTree();setDirty(false);return true;}catch(const std::exception&e){QMessageBox::critical(this,"Open failed",e.what());return false;}}
void SimulationMainWindow::save(){if(filePath_.isEmpty())saveAs();else saveDocument(filePath_);}void SimulationMainWindow::saveAs(){QString path=QFileDialog::getSaveFileName(this,"Save Simulation Project",filePath_,"MyDesk Simulation (*.mdsim)");if(path.isEmpty())return;if(!path.endsWith(".mdsim",Qt::CaseInsensitive))path+=".mdsim";saveDocument(path);}void SimulationMainWindow::openDialog(){if(!maybeSave())return;const QString path=QFileDialog::getOpenFileName(this,"Open Simulation Project",{},"MyDesk Simulation (*.mdsim)");if(!path.isEmpty())openDocument(path);}
bool SimulationMainWindow::exportStaticReport(const QString& path){if(!staticResult_.converged)return false;QSaveFile file(path);if(!file.open(QIODevice::WriteOnly|QIODevice::Text))return false;const QByteArray data=QByteArray::fromStdString(document_.export_report(staticResult_));return file.write(data)==data.size()&&file.commit();}
void SimulationMainWindow::exportReportDialog(){QString path=QFileDialog::getSaveFileName(this,"Export Static Analysis Report",{},"Text report (*.txt)");if(path.isEmpty())return;if(!path.endsWith(".txt",Qt::CaseInsensitive))path+=".txt";if(!exportStaticReport(path))QMessageBox::critical(this,"Export failed","Solve a valid static model before exporting.");}
bool SimulationMainWindow::maybeSave(){if(!dirty_)return true;const auto answer=QMessageBox::warning(this,"Unsaved simulation","Save changes before continuing?",QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Save);if(answer==QMessageBox::Cancel)return false;if(answer==QMessageBox::Save){save();return!dirty_;}return true;}
void SimulationMainWindow::closeEvent(QCloseEvent* event){if(!maybeSave()){event->ignore();return;}QSettings settings;settings.setValue("simulation/geometry",saveGeometry());settings.setValue("simulation/state",saveState());event->accept();}
