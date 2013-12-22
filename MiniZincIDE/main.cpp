/*
 *  Author:
 *     Guido Tack <guido.tack@monash.edu>
 *
 *  Copyright:
 *     NICTA 2013
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
