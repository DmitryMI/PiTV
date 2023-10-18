#include "PiTVDesktopViewer.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    PiTVDesktopViewer w;
    w.show();
    return a.exec();
}
