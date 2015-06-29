#!/bin/bash
MZNVERSION=2.0.3
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
qmake -makefile "CONFIG+=bundled" && \
make -j4 && \
cd ../.. && \
curl -L -O https://github.com/MiniZinc/libminizinc/releases/download/$MZNVERSION/minizinc-$MZNVERSION-Darwin.tar.gz && \
tar xf minizinc-$MZNVERSION-Darwin.tar.gz && \
cp -r minizinc-$MZNVERSION/share MiniZincIDE-$VERSION/MiniZincIDE/MiniZincIDE.app/Contents/Resources/ && \
cp minizinc-$MZNVERSION/bin/* MiniZincIDE-$VERSION/MiniZincIDE/MiniZincIDE.app/Contents/Resources/ && \
cp ../fzn-gecode/flatzinc MiniZincIDE-$VERSION/MiniZincIDE/MiniZincIDE.app/Contents/Resources/ && \
mkdir MiniZincIDE-$VERSION/MiniZincIDE/MiniZincIDE.app/Contents/Resources/gecode && \
cp ../fzn-gecode/fzn-gecode-bin MiniZincIDE-$VERSION/MiniZincIDE/MiniZincIDE.app/Contents/Resources/gecode/fzn-gecode && \
cp ../fzn-gecode/fzn-gecode-qt.conf MiniZincIDE-$VERSION/MiniZincIDE/MiniZincIDE.app/Contents/Resources/gecode/qt.conf && \
cp ../fzn-gecode/fzn-gecode MiniZincIDE-$VERSION/MiniZincIDE/MiniZincIDE.app/Contents/Resources/ && \
cp ../fzn-gecode/fzn-gecode-gist MiniZincIDE-$VERSION/MiniZincIDE/MiniZincIDE.app/Contents/Resources/ && \
cp -r ../fzn-gecode/gecode MiniZincIDE-$VERSION/MiniZincIDE/MiniZincIDE.app/Contents/Resources/share/minizinc/ && \
cd MiniZincIDE-$VERSION/MiniZincIDE && \
macdeployqt MiniZincIDE.app -dmg -executable=MiniZincIDE.app/Contents/Resources/gecode/fzn-gecode && \
mv MiniZincIDE.dmg ../../../MiniZincIDE-$VERSION-bundle.dmg &&
cd ../../.. && \
rm -r build-ide

