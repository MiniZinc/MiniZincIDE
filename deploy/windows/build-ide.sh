#!/bin/bash
VERSION=0.9.2
QTDIR=/cygdrive/c/Qt/Qt5.2.0/5.2.0/msvc2010/bin
MSVCDIR=/cygdrive/c/Program\ Files\ \(x86\)/Microsoft\ Visual\ Studio\ 10.0/VC/bin
ISCC=/cygdrive/c/Program\ Files\ \(x86\)/Inno\ Setup\ 5
export PATH=$QTDIR:$MSVCDIR:$ISCC:$PATH
rm -rf build-ide
mkdir build-ide
cd build-ide
wget http://www.minizinc.org/downloads/MiniZincIDE/MiniZincIDE-$VERSION.tgz && \
tar xzf MiniZincIDE-$VERSION.tgz && \
cd MiniZincIDE-$VERSION && \
qmake && \
nmake && \
mv release/MiniZincIDE.exe ../.. && \
cd ../.. && \
rm -r build-ide && \
ISCC.exe MiniZincIDE.iss




