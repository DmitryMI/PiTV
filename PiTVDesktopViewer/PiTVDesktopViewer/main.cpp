#include "PiTVDesktopViewer.h"
#include <QtWidgets/QApplication>
#include <gst/gst.h>

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    QApplication a(argc, argv);
    PiTVDesktopViewer w;
    w.show();
    return a.exec();
}
