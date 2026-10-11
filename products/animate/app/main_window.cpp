#include "main_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <cmath>
#include <fstream>
#include <functional>
#include <string>

namespace mydesk::animate::ui {

namespace {
QDoubleSpinBox* number(const char* name, double value, double minimum = -100000.0,
                       double maximum = 100000.0) {
    auto* box = new QDoubleSpinBox;
    box->setObjectName(name);
    box->setRange(minimum, maximum);
    box->setDecimals(3);
    box->setValue(value);
    box->setKeyboardTracking(false);
    return box;
}

Quat euler(double x, double y, double z) {
    constexpr double factor = 3.14159265358979323846 / 360.0;
    const double cx=std::cos(x*factor), sx=std::sin(x*factor);
    const double cy=std::cos(y*factor), sy=std::sin(y*factor);
    const double cz=std::cos(z*factor), sz=std::sin(z*factor);
    return {sx*cy*cz-cx*sy*sz, cx*sy*cz+sx*cy*sz,
            cx*cy*sz-sx*sy*cz, cx*cy*cz+sx*sy*sz};
}

QString kindName(NodeKind kind) {
    switch (kind) {
    case NodeKind::Transform: return "Transform";
    case NodeKind::Mesh: return "Mesh";
    case NodeKind::Camera: return "Camera";
    case NodeKind::Light: return "Light";
    case NodeKind::Joint: return "Joint";
    case NodeKind::Locator: return "Locator";
    }
    return "Unknown";
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setObjectName("animateMainWindow");
    resize(1440, 900);
    buildUi();
    QSettings settings;
    restoreGeometry(settings.value("animate/geometry").toByteArray());
    restoreState(settings.value("animate/state").toByteArray());
    refresh();
}

void MainWindow::buildUi() {
    viewport_ = new AnimateViewport(this);
    viewport_->setDocument(&document_);
    viewport_->selectionChanged = [this](NodeId id) { select(id); };
    setCentralWidget(viewport_);

    auto* file = menuBar()->addMenu("&File");
    auto* newAction = file->addAction("&New", QKeySequence::New, this, [this] {
        if (!maybeSave()) return;
        document_ = AnimateDocument{}; path_.clear(); dirty_ = false;
        selected_ = kInvalidNode; viewport_->setDocument(&document_); refresh();
    });
    auto* openAction = file->addAction("&Open…", QKeySequence::Open, this, [this] { open(); });
    auto* saveAction = file->addAction("&Save", QKeySequence::Save, this, [this] { save(); });
    file->addAction("Save &As…", QKeySequence::SaveAs, this, [this] { save(true); });
    file->addSeparator();
    file->addAction("Export &USD ASCII…", QKeySequence("Ctrl+E"), this, [this] { exportUsd(); });
    file->addSeparator();
    file->addAction("E&xit", QKeySequence::Quit, this, &QWidget::close);

    auto* edit = menuBar()->addMenu("&Edit");
    undoAction_ = edit->addAction("&Undo", QKeySequence::Undo, this, [this] {
        if (document_.undo()) { markDirty(); selected_ = kInvalidNode; refresh(); }
    });
    undoAction_->setObjectName("undoAction");
    redoAction_ = edit->addAction("&Redo", QKeySequence::Redo, this, [this] {
        if (document_.redo()) { markDirty(); selected_ = kInvalidNode; refresh(); }
    });
    redoAction_->setObjectName("redoAction");
    edit->addAction("&Delete", QKeySequence::Delete, this, [this] { deleteSelection(); });

    auto* toolbar = addToolBar("Modeling");
    toolbar->setObjectName("modelingToolbar");
    toolbar->addAction(newAction); toolbar->addAction(openAction); toolbar->addAction(saveAction);
    toolbar->addSeparator();
    auto* box = toolbar->addAction("Create Box"); box->setObjectName("createBoxAction");
    auto* sphere = toolbar->addAction("Create Sphere"); sphere->setObjectName("createSphereAction");
    connect(box, &QAction::triggered, this, [this] { createBox(); });
    connect(sphere, &QAction::triggered, this, [this] { createSphere(); });
    toolbar->addSeparator();
    toolbar->addAction("Frame All", viewport_, &AnimateViewport::frameAll);

    auto* outlinerDock = new QDockWidget("Outliner", this);
    outlinerDock->setObjectName("outlinerDock");
    outliner_ = new QTreeWidget;
    outliner_->setObjectName("sceneOutliner");
    outliner_->setHeaderLabels({"Scene Node", "Type"});
    outlinerDock->setWidget(outliner_);
    addDockWidget(Qt::LeftDockWidgetArea, outlinerDock);
    connect(outliner_, &QTreeWidget::itemSelectionChanged, this, [this] {
        const auto items = outliner_->selectedItems();
        if (!items.empty()) select(items.front()->data(0, Qt::UserRole).toUInt());
    });

    auto* channelDock = new QDockWidget("Channel Box", this);
    channelDock->setObjectName("channelBoxDock");
    auto* channelWidget = new QWidget;
    auto* form = new QFormLayout(channelWidget);
    name_ = new QLineEdit; name_->setObjectName("nodeName");
    form->addRow("Name", name_);
    translate_ = {number("translateX",0), number("translateY",0), number("translateZ",0)};
    rotate_ = {number("rotateX",0), number("rotateY",0), number("rotateZ",0)};
    scale_ = {number("scaleX",1,-1000,1000), number("scaleY",1,-1000,1000),
              number("scaleZ",1,-1000,1000)};
    const char* labels[] = {"Translate X","Translate Y","Translate Z",
                            "Rotate X","Rotate Y","Rotate Z","Scale X","Scale Y","Scale Z"};
    QDoubleSpinBox* values[] = {translate_[0],translate_[1],translate_[2],rotate_[0],rotate_[1],
                                rotate_[2],scale_[0],scale_[1],scale_[2]};
    for (int index=0; index<9; ++index) form->addRow(labels[index], values[index]);
    auto* apply = new QPushButton("Apply Channels"); apply->setObjectName("applyChannelsButton");
    form->addRow(apply);
    connect(apply, &QPushButton::clicked, this, [this] { applyTransform(); });
    channelDock->setWidget(channelWidget);
    addDockWidget(Qt::RightDockWidgetArea, channelDock);

    auto* timelineDock = new QDockWidget("Timeline / Dope Sheet", this);
    timelineDock->setObjectName("timelineDock");
    auto* timelineWidget = new QWidget;
    auto* layout = new QVBoxLayout(timelineWidget);
    auto* controls = new QHBoxLayout;
    auto* play = new QPushButton("Play"); play->setObjectName("playButton");
    frameSpin_ = new QSpinBox; frameSpin_->setObjectName("currentFrame");
    frameSpin_->setRange(0,240);
    timeline_ = new QSlider(Qt::Horizontal); timeline_->setObjectName("timelineSlider");
    timeline_->setRange(0,240);
    controls->addWidget(play); controls->addWidget(new QLabel("Frame"));
    controls->addWidget(frameSpin_); controls->addWidget(timeline_,1);
    layout->addLayout(controls);
    auto* keyControls = new QHBoxLayout;
    channel_ = new QComboBox; channel_->setObjectName("keyChannel");
    channel_->addItems({"tx","ty","tz","rx","ry","rz","sx","sy","sz"});
    keyValue_ = number("keyValue",0);
    auto* setKeyButton = new QPushButton("Set Key"); setKeyButton->setObjectName("setKeyButton");
    keyControls->addWidget(new QLabel("Channel")); keyControls->addWidget(channel_);
    keyControls->addWidget(new QLabel("Value")); keyControls->addWidget(keyValue_);
    keyControls->addWidget(setKeyButton);
    layout->addLayout(keyControls);
    keys_ = new QListWidget; keys_->setObjectName("dopeSheetKeys");
    layout->addWidget(keys_);
    timelineDock->setWidget(timelineWidget);
    addDockWidget(Qt::BottomDockWidgetArea, timelineDock);
    connect(timeline_, &QSlider::valueChanged, this, [this](int value) { setFrame(value); });
    connect(frameSpin_, qOverload<int>(&QSpinBox::valueChanged), timeline_, &QSlider::setValue);
    connect(setKeyButton, &QPushButton::clicked, this, [this] { setKey(); });
    connect(play, &QPushButton::clicked, this, [this] { togglePlayback(); });
    playback_ = new QTimer(this);
    playback_->setInterval(1000/24);
    connect(playback_, &QTimer::timeout, this, [this] {
        int next = timeline_->value()+1;
        if (next > timeline_->maximum()) next = timeline_->minimum();
        timeline_->setValue(next);
    });

    qApp->setStyleSheet("QWidget{background:#151922;color:#e4e8ef;}"
        "QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox,QTreeWidget,QListWidget{background:#202632;border:1px solid #4b5563;padding:3px;}"
        "QPushButton{background:#315b8a;border:1px solid #6ba5dc;padding:5px 10px;}"
        "QDockWidget::title{background:#252c38;padding:5px;font-weight:600;}"
        "QTreeWidget::item:selected,QListWidget::item:selected{background:#315b8a;}");
}

void MainWindow::createBox() {
    document_.begin_edit();
    selected_ = document_.create_box("pCube" + std::to_string(document_.node_count()+1));
    markDirty(); refresh(); viewport_->frameAll();
}
void MainWindow::createSphere() {
    document_.begin_edit();
    selected_ = document_.create_sphere("pSphere" + std::to_string(document_.node_count()+1));
    markDirty(); refresh(); viewport_->frameAll();
}

void MainWindow::applyTransform() {
    if (selected_ == kInvalidNode) return;
    try {
        document_.begin_edit();
        document_.rename_node(selected_, name_->text().trimmed().toStdString());
        document_.set_transform(selected_, {translate_[0]->value(),translate_[1]->value(),translate_[2]->value()},
            euler(rotate_[0]->value(),rotate_[1]->value(),rotate_[2]->value()),
            {scale_[0]->value(),scale_[1]->value(),scale_[2]->value()});
        markDirty(); refresh();
    } catch (const std::exception& error) { QMessageBox::critical(this,"Channel edit",error.what()); }
}

void MainWindow::setKey() {
    if (selected_ == kInvalidNode) return;
    try {
        document_.begin_edit();
        document_.set_keyframe(selected_, channel_->currentText().toStdString(), frame_, keyValue_->value());
        markDirty(); refresh();
    } catch (const std::exception& error) { QMessageBox::critical(this,"Set key",error.what()); }
}

void MainWindow::deleteSelection() {
    if (selected_ == kInvalidNode) return;
    document_.begin_edit(); document_.remove_node(selected_); selected_=kInvalidNode;
    markDirty(); refresh();
}

void MainWindow::select(NodeId id) {
    try { (void)document_.node(id); selected_=id; }
    catch (...) { selected_=kInvalidNode; }
    viewport_->setSelectedNode(selected_); refreshProperties(); refreshKeys();
}

void MainWindow::setFrame(double frame) {
    frame_=frame; frameSpin_->blockSignals(true); frameSpin_->setValue(static_cast<int>(frame));
    frameSpin_->blockSignals(false); viewport_->setFrame(frame); statusBar()->showMessage(QString("Frame %1").arg(frame));
}

void MainWindow::togglePlayback() { playback_->isActive() ? playback_->stop() : playback_->start(); }

void MainWindow::refresh() {
    timeline_->setRange(static_cast<int>(document_.timeline().start_frame),
                        static_cast<int>(document_.timeline().end_frame));
    frameSpin_->setRange(timeline_->minimum(), timeline_->maximum());
    refreshOutliner(); refreshProperties(); refreshKeys();
    viewport_->setSelectedNode(selected_); viewport_->setFrame(frame_);
    undoAction_->setEnabled(document_.can_undo()); redoAction_->setEnabled(document_.can_redo());
    setWindowTitle(QString("%1%2 — MyDesk Animate").arg(path_.empty()?"Untitled":
        QString::fromStdString(path_.filename().string()), dirty_?"*":""));
    statusBar()->showMessage(QString("%1 nodes · %2 meshes · Frame %3")
        .arg(document_.node_count()).arg(document_.mesh_count()).arg(frame_,0,'f',1));
}

void MainWindow::refreshOutliner() {
    outliner_->blockSignals(true); outliner_->clear();
    std::function<void(NodeId,QTreeWidgetItem*)> add = [&](NodeId id,QTreeWidgetItem* parent) {
        const SceneNode& node=document_.node(id);
        auto* item=parent?new QTreeWidgetItem(parent):new QTreeWidgetItem(outliner_);
        item->setText(0,QString::fromStdString(node.name)); item->setText(1,kindName(node.kind));
        item->setData(0,Qt::UserRole,id); item->setExpanded(true);
        if(id==selected_) item->setSelected(true);
        for(NodeId child:node.children) add(child,item);
    };
    for(NodeId root:document_.root_nodes()) add(root,nullptr);
    outliner_->blockSignals(false);
}

void MainWindow::refreshProperties() {
    const bool enabled=selected_!=kInvalidNode;
    name_->setEnabled(enabled);
    for(auto* v:translate_) v->setEnabled(enabled); for(auto* v:rotate_) v->setEnabled(enabled);
    for(auto* v:scale_) v->setEnabled(enabled);
    if(!enabled){name_->clear();return;}
    const SceneNode& node=document_.node(selected_); name_->setText(QString::fromStdString(node.name));
    translate_[0]->setValue(node.translate.x);translate_[1]->setValue(node.translate.y);translate_[2]->setValue(node.translate.z);
    scale_[0]->setValue(node.scale.x);scale_[1]->setValue(node.scale.y);scale_[2]->setValue(node.scale.z);
}

void MainWindow::refreshKeys() {
    keys_->clear(); if(selected_==kInvalidNode)return;
    const SceneNode& node=document_.node(selected_); if(!node.anim)return;
    const AnimCurve* curves[]={&node.anim->tx,&node.anim->ty,&node.anim->tz,&node.anim->rx,&node.anim->ry,
        &node.anim->rz,&node.anim->sx,&node.anim->sy,&node.anim->sz};
    const char* names[]={"tx","ty","tz","rx","ry","rz","sx","sy","sz"};
    for(int c=0;c<9;++c) for(std::size_t i=0;i<curves[c]->key_count();++i){const auto& k=curves[c]->key_at(i);
        keys_->addItem(QString("%1  frame %2  value %3").arg(names[c]).arg(document_.timeline().time_to_frame(k.time),0,'f',1).arg(k.value,0,'f',3));}
}

void MainWindow::markDirty(){dirty_=true;}
bool MainWindow::maybeSave(){if(!dirty_)return true;const auto a=QMessageBox::warning(this,"Unsaved scene","Save changes?",QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel);return a==QMessageBox::Discard||(a==QMessageBox::Save&&save());}
bool MainWindow::save(bool choosePath){QString file=QString::fromStdString(path_.string());if(choosePath||path_.empty())file=QFileDialog::getSaveFileName(this,"Save Animate Scene",file,"MyDesk Animate (*.myanimate)");if(file.isEmpty())return false;if(QFileInfo(file).suffix().isEmpty())file+=".myanimate";try{document_.save_native(file.toStdString());path_=file.toStdString();dirty_=false;refresh();return true;}catch(const std::exception&e){QMessageBox::critical(this,"Save",e.what());return false;}}
void MainWindow::open(){if(!maybeSave())return;const QString file=QFileDialog::getOpenFileName(this,"Open Animate Scene",{},"MyDesk Animate (*.myanimate)");if(file.isEmpty())return;try{document_=AnimateDocument::load_native(file.toStdString());path_=file.toStdString();dirty_=false;selected_=kInvalidNode;viewport_->setDocument(&document_);refresh();}catch(const std::exception&e){QMessageBox::critical(this,"Open",e.what());}}
void MainWindow::exportUsd(){QString file=QFileDialog::getSaveFileName(this,"Export USD ASCII",{},"USD ASCII (*.usda)");if(file.isEmpty())return;std::ofstream out(file.toStdString());out<<document_.export_usda();if(!out)QMessageBox::critical(this,"Export USD","Could not write file");}
void MainWindow::closeEvent(QCloseEvent* event){if(!maybeSave()){event->ignore();return;}QSettings s;s.setValue("animate/geometry",saveGeometry());s.setValue("animate/state",saveState());event->accept();}

} // namespace mydesk::animate::ui
