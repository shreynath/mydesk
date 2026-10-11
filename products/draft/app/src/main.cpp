#include "main_window.hpp"

#include <QApplication>
#include <QSurfaceFormat>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("MyDesk Draft");
    QApplication::setOrganizationName("MyDesk");
    QApplication::setApplicationDisplayName(QStringLiteral("MyDesk Draft"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QSurfaceFormat format;
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    mydesk::desktop::MainWindow window;
    if (QCoreApplication::arguments().contains(QStringLiteral("--packaging-smoke"))) {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    } else {
        window.show();
    }
    return app.exec();
}
