// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QTimer>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("MyDesk"));
    QCoreApplication::setApplicationName(QStringLiteral("MyDesk Vault"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.0"));
    mydesk::vault::VaultMainWindow window;
    if (application.arguments().contains(QStringLiteral("--packaging-smoke"))) {
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
        return application.exec();
    }
    window.restoreLastRepository();
    window.show();
    return application.exec();
}
