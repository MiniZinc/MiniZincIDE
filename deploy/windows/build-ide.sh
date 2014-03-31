#!/bin/bash
QTDIR=/cygdrive/c/Qt/Qt5.2.1/5.2.1/msvc2010/bin
MSVCDIR=/cygdrive/c/Program\ Files\ \(x86\)/Microsoft\ Visual\ Studio\ 10.0/VC/bin
ISCC=/cygdrive/c/Program\ Files\ \(x86\)/Inno\ Setup\ 5
export PATH=$QTDIR:$MSVCDIR:$ISCC:$PATH
rm -rf build-ide
mkdir build-ide
if [ "x$2" == "x" ]; then
cd build-ide
VERSION=$1
wget http://www.minizinc.org/downloads/MiniZincIDE/MiniZincIDE-$VERSION.tgz
else
VERSION=$2
cp $1 build-ide/MiniZincIDE-$VERSION.tgz
cd build-ide
fi
tar xzf MiniZincIDE-$VERSION.tgz && \
cd MiniZincIDE-$VERSION && \
qmake && \
nmake && \
mv release/MiniZincIDE.exe ../.. && \
cd ../.. && \
rm -r build-ide && \
ISCC.exe /dMyAppVersion="$VERSION" MiniZincIDE.iss
