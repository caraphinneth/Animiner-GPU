#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    // QCoreApplication::setAttribute (Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute (Qt::AA_UseHighDpiPixmaps);
    QCoreApplication::setApplicationName ("Animiner-GPU");
    QCoreApplication::setOrganizationName ("Animecoin");
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
