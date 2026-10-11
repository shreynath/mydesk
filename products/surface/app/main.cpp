// SPDX-License-Identifier: MPL-2.0
// MyDesk Surface — precision NURBS surfacing desktop application
#include "main_window.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QTimer>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("MyDesk");
    QCoreApplication::setApplicationName("MyDeskSurface");
    QCoreApplication::setApplicationVersion("0.2.0");
    SurfaceMainWindow window;
    if (QCoreApplication::arguments().contains(QStringLiteral("--packaging-smoke"))) {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
        return app.exec();
    }
    window.show();
    return app.exec();
}
