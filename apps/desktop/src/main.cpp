#include "main_window.hpp"

#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("MyDesk");
    QApplication::setOrganizationName("MyDesk");
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QSurfaceFormat format;
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    mydesk::desktop::MainWindow window;
    window.show();
    return app.exec();
}
