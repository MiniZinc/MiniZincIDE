#include "mainwindow.h"

int main(int argc, char *argv[])
{
    IDE a(argc, argv);
    a.setOrganizationName("MiniZinc");
    a.setOrganizationDomain("minizinc.org");
    a.setApplicationName("MiniZinc IDE");
    QStringList args = QApplication::arguments();
    QStringList files;
    for (int i=1; i<args.size(); i++) {
        if (args[i].endsWith(".mzp")) {
            MainWindow* mw = new MainWindow(args[i]);
            mw->show();
        } else {
            files << args[i];
        }
    }
    MainWindow w(files);
    w.show();

    return a.exec();
}
