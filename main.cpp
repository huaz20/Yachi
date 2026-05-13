#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/images/yachiyo.jpg"));
    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
