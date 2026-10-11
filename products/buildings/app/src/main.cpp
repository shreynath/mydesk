#include "mydesk/buildings/main_window.hpp"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName("MyDesk Buildings");
    QCoreApplication::setOrganizationName("MyDesk");
    mydesk::buildings::MainWindow window;
    window.show();
    return application.exec();
}
