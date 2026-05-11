#include <QApplication>
#include <QIcon>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("PixelCanvas");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("PixelCanvas");
    app.setWindowIcon(QIcon(":/resources/app.png"));

    MainWindow window;
    window.show();

    return app.exec();
}
