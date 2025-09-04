#include <QApplication>
#include "MainWindow.h"
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Med YOLO11 Qt");
    QApplication::setOrganizationName("YourLab");
    MainWindow w;
    w.resize(1280, 860);
    w.show();
    return app.exec();
}
