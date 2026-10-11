#include "mydesk/buildings/main_window.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QTimer>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName("MyDesk Buildings");
    QCoreApplication::setOrganizationName("MyDesk");
    mydesk::buildings::MainWindow window;
    if (QCoreApplication::arguments().contains(QStringLiteral("--packaging-smoke"))) {
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    } else {
        window.show();
    }
    return application.exec();
}
