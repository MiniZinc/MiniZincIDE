#!/bin/bash
VERSION=0.9.2
rm -rf build-ide
mkdir build-ide
cd build-ide
wget http://www.minizinc.org/downloads/MiniZincIDE/MiniZincIDE-$VERSION.tgz && \
tar xzf MiniZincIDE-$VERSION.tgz && \
cd MiniZincIDE-$VERSION && \
qmake -makefile && \
make -j4 && \
macdeployqt MiniZincIDE.app -dmg && \
mv MiniZincIDE.dmg ../../MiniZincIDE-$VERSION.dmg &&
cd ../.. && \
rm -r build-ide

