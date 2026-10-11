// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QTimer>

int main(int argc,char** argv){
    QApplication application(argc,argv);QCoreApplication::setOrganizationName("MyDesk");QCoreApplication::setApplicationName("MyDesk Simulation");QCoreApplication::setApplicationVersion("0.3.0");
    QCommandLineParser parser;parser.addHelpOption();parser.addVersionOption();QCommandLineOption smoke("packaging-smoke","Construct the installed UI and exit.");parser.addOption(smoke);parser.addPositionalArgument("project","MyDesk Simulation .mdsim project to open.");parser.process(application);
    SimulationMainWindow window;if(parser.isSet(smoke)){QTimer::singleShot(0,&application,&QCoreApplication::quit);return application.exec();}
    if(!parser.positionalArguments().isEmpty())window.openDocument(QFileInfo(parser.positionalArguments().front()).absoluteFilePath());window.show();return application.exec();
}
