#include "main_window.hpp"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("MyDesk"));
    QCoreApplication::setApplicationName(QStringLiteral("MyDesk Coordinate"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.3.0"));
    CoordinateMainWindow window;
    window.show();
    return application.exec();
}
