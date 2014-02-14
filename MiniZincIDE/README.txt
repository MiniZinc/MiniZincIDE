================================================================
MiniZinc IDE
================================================================

http://www.minizinc.org

The MiniZinc IDE is copyright 2013 NICTA. Please see LICENSE.txt for license information.

1. Prerequisites

To compile the MiniZinc IDE, you need the Qt toolkit (http://qt-project.org),
version 5.0 or newer. On Windows and Mac OS, you can use the precompiled
binaries from the Qt web site. On Linux, you can use the packages that come
with your distribution (remember to also install the -dev packages that
contain the header files).

On Windows, you need Microsoft Visual C++ 2010 or later.
On Mac OS, you need XCode 4 or later.
On Linux, you need a recent C++ compiler (gcc or clang).

To run the MiniZinc IDE, you will need version 1.6 of the G12 MiniZinc
distribution (http://www.minizinc.org/download.html).

2. Compilation

2.1 Using Qt Creator

Unpack the source code. Double-click the file MiniZincIDE.pro in the
MiniZincIDE folder. This will open MiniZincIDE project in the Qt Creator
application. Configure the project with the default settings. Change the build
type to Release (using the icon in the bottom left hand corner that says
"Debug"). Choose Build Project "MiniZincIDE" from the Build menu. If
everything succeeds, the MiniZinc IDE binary will be created in the build
folder that you set in the initial configuration.

2.2 Using the command line

Unpack the source code. Run qmake in the MiniZincIDE subdirectory. Run make
(or nmake on a Windows command line).

2.3 Troubleshooting

If the qmake step fails with a message about qtwidgets, please make sure your
Qt version is indeed 5.0 or newer.
