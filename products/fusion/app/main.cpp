// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QApplication>
#include <QTimer>
#include <algorithm>
#include <string_view>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QApplication::setApplicationName("MyDesk Fusion");
    QApplication::setApplicationVersion("0.2.0");
    QApplication::setOrganizationName("MyDesk");
    FusionMainWindow window;
    window.show();
    const bool smoke = std::any_of(argv + 1, argv + argc,
        [](const char* argument) { return std::string_view(argument) == "--packaging-smoke"; });
    if (smoke) QTimer::singleShot(100, &application, &QCoreApplication::quit);
    return application.exec();
}
