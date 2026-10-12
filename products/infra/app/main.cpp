// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QStyleFactory>
#include <QTimer>

int main(int argc,char** argv){QApplication app(argc,argv);QCoreApplication::setOrganizationName("MyDesk");QCoreApplication::setApplicationName("MyDesk Infra");QCoreApplication::setApplicationVersion("0.3.0");app.setStyle(QStyleFactory::create("Fusion"));app.setStyleSheet("QMainWindow,QDockWidget,QTreeWidget,QWidget{background:#182025;color:#e5ecef}QMenuBar,QMenu,QToolBar,QStatusBar{background:#26333a;color:#f8fafb}QTreeWidget{alternate-background-color:#202b31;border:0}QTreeWidget::item:selected{background:#39725d}QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox{background:#11181c;border:1px solid #596a70;border-radius:3px;padding:4px}QPushButton{background:#3d745d;border:1px solid #70a98e;border-radius:3px;padding:7px;color:white}QDockWidget::title{background:#2d3b42;padding:7px;font-weight:600}");QCommandLineParser parser;parser.addHelpOption();parser.addVersionOption();QCommandLineOption smoke("packaging-smoke","Construct the infrastructure workspace and exit.");parser.addOption(smoke);parser.addPositionalArgument("project",".myinfra model to open");parser.process(app);InfraMainWindow window;if(parser.isSet(smoke)){QTimer::singleShot(0,&app,&QCoreApplication::quit);return app.exec();}if(!parser.positionalArguments().isEmpty())window.openDocument(QFileInfo(parser.positionalArguments().front()).absoluteFilePath());window.show();return app.exec();}
