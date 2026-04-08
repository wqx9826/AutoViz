#include <QApplication>

#include "app/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("AutoViz");
    QApplication::setOrganizationName("AutoViz");

    MainWindow window;
    window.show();

    return app.exec();
}
