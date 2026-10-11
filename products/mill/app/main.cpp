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
    QCoreApplication::setApplicationName("MyDesk Mill");
    QCoreApplication::setApplicationVersion("0.3.0");
    application.setStyle(QStyleFactory::create("Fusion"));
    application.setStyleSheet(R"(
        QMainWindow, QDockWidget, QTreeWidget, QWidget { background: #111827; color: #e5e7eb; }
        QMenuBar, QMenu, QToolBar, QStatusBar { background: #1f2937; color: #f8fafc; }
        QTreeWidget { alternate-background-color: #172033; border: 0; }
        QTreeWidget::item:selected { background: #0369a1; color: white; }
        QLineEdit, QComboBox, QDoubleSpinBox { background: #0f172a; border: 1px solid #475569;
            border-radius: 3px; padding: 4px; color: #f8fafc; }
        QPushButton { background: #0369a1; border: 1px solid #38bdf8; border-radius: 3px;
            color: white; padding: 7px 12px; }
        QPushButton:hover { background: #075985; }
        QPushButton:focus, QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus {
            border: 2px solid #7dd3fc; }
        QDockWidget::title { background: #1e293b; padding: 7px; font-weight: 600; }
    )");

    QCommandLineParser parser;
    parser.setApplicationDescription("MyDesk Mill professional 2.5D CAM application");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption packagingSmoke("packaging-smoke",
        "Construct the deployed desktop application and exit without showing a window.");
    parser.addOption(packagingSmoke);
    parser.addPositionalArgument("project", "MyDesk Mill .mymill project to open.");
    parser.process(application);

    MillMainWindow window;
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
