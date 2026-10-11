// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName("MyDesk Animate");
    QCoreApplication::setOrganizationName("MyDesk");
    mydesk::animate::ui::MainWindow window;
    window.show();
    return application.exec();
}
