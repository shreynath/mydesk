// SPDX-License-Identifier: MPL-2.0
#include "main_window.hpp"

#include <QApplication>
#include <QCoreApplication>

#include <string_view>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("MyDesk"));
    QCoreApplication::setApplicationName(QStringLiteral("MyDeskDraftLT"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.0"));

    mydesk::draft_lt::MainWindow window;
    if (argc > 1 && std::string_view(argv[1]) == "--packaging-smoke") {
        app.processEvents();
        return window.document().layers().empty() ? 1 : 0;
    }
    window.show();
    return app.exec();
}
