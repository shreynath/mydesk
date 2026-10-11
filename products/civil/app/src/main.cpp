// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QStyleFactory>
#include <QTimer>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName("MyDesk");
    QCoreApplication::setApplicationName("MyDesk Civil");
    QCoreApplication::setApplicationVersion("0.2.0");
    application.setStyle(QStyleFactory::create("Fusion"));
    application.setStyleSheet(R"(
        QMainWindow, QDockWidget, QTreeWidget, QWidget { background: #141b22; color: #dce6ec; }
        QMenuBar, QMenu, QToolBar, QStatusBar { background: #202b34; color: #f3f7f9; }
        QTreeWidget { alternate-background-color: #19242d; border: 0; }
        QTreeWidget::item:selected { background: #136f83; color: white; }
        QLineEdit, QDoubleSpinBox { background: #0f171d; border: 1px solid #52636d;
            border-radius: 3px; padding: 4px; color: #f5f8fa; }
        QPushButton { background: #176c7c; border: 1px solid #50afbd; border-radius: 3px;
            color: white; padding: 7px 12px; }
        QPushButton:hover { background: #208395; }
        QDockWidget::title { background: #26343e; padding: 7px; font-weight: 600; }
    )");

    QCommandLineParser parser;
    parser.setApplicationDescription("MyDesk Civil professional civil engineering design application");
    parser.addHelpOption(); parser.addVersionOption();
    QCommandLineOption packagingSmoke("packaging-smoke",
        "Construct the deployed civil workspace and exit without showing a window.");
    parser.addOption(packagingSmoke);
    parser.addPositionalArgument("project", "MyDesk Civil .mydesk-civil project to open.");
    parser.process(application);

    CivilMainWindow window;
    if (parser.isSet(packagingSmoke)) {
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
        return application.exec();
    }
    if (!parser.positionalArguments().isEmpty()) {
        window.openDocument(QFileInfo(parser.positionalArguments().front()).absoluteFilePath());
    }
    window.show();
    return application.exec();
}
