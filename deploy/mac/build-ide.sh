#!/bin/bash
rm -rf build-ide
mkdir build-ide
if [ "x$2" == "x" ]; then
  VERSION=$1
  cd build-ide
  curl -L -O https://github.com/MiniZinc/MiniZincIDE/archive/$VERSION.tar.gz
  mv $VERSION.tar.gz MiniZincIDE-$VERSION.tgz
else
  VERSION=$2
  cp $1 build-ide/MiniZincIDE-$VERSION.tgz
  cd build-ide
fi
tar xzf MiniZincIDE-$VERSION.tgz && \
cd MiniZincIDE-$VERSION/MiniZincIDE && \
qmake -makefile && \
make -j4 && \
macdeployqt MiniZincIDE.app -dmg && \
mv MiniZincIDE.dmg ../../../MiniZincIDE-$VERSION.dmg &&
cd ../../.. && \
rm -r build-ide

