#include "main_window.hpp"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName("MyDesk Mechanical");
    QCoreApplication::setOrganizationName("MyDesk");
    mydesk::mechanical::desktop::MainWindow window;
    window.show();
    return application.exec();
}
