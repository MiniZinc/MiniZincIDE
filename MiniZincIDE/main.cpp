#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("MiniZinc");
    a.setOrganizationDomain("minizinc.org");
    a.setApplicationName("MiniZinc IDE");
    MainWindow w;
    w.show();

    return a.exec();
}
