#include "main_window.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QTimer>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("MyDesk"));
    QCoreApplication::setApplicationName(QStringLiteral("MyDesk Coordinate"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.3.0"));
    CoordinateMainWindow window;
    if (QCoreApplication::arguments().contains(QStringLiteral("--packaging-smoke"))) {
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    } else {
        window.show();
    }
    return application.exec();
}
