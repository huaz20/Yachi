#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //设置全局字体
    QFont globalFont("Microsoft YaHei", 9);
    a.setFont(globalFont);

    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
