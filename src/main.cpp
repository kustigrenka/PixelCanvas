#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    // Set platform before QApplication is created
    // Override at runtime: QT_QPA_PLATFORM=wayland ./PixelCanvas
    //                  or: QT_QPA_PLATFORM=xcb    ./PixelCanvas

    QApplication app(argc, argv);
    app.setApplicationName("PixelCanvas");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("PixelCanvas");

    MainWindow window;
    window.show();

    return app.exec();
}
