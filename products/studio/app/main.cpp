// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QTimer>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName("MyDesk");
    QCoreApplication::setOrganizationDomain("mydesk.app");
    QCoreApplication::setApplicationName("MyDeskStudio");
    QCoreApplication::setApplicationVersion("0.3.0");

    mydesk::studio::ui::MainWindow window;
    if (QCoreApplication::arguments().contains(QStringLiteral("--packaging-smoke"))) {
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    } else {
        window.show();
    }
    return application.exec();
}
