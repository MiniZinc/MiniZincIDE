#!/bin/bash
VERSION=0.9.2
PACKAGE=MiniZincIDE-$VERSION-linux-x86_64
QTDIR=~/Qt5.2.0/5.2.0/gcc_64
export PATH=$QTDIR/bin:$PATH
rm -rf build-ide
mkdir build-ide
cd build-ide
wget http://www.minizinc.org/download/MiniZincIDE-$VERSION.tar.gz && \
tar xzf MiniZincIDE-$VERSION.tar.gz && \
cd MiniZincIDE-$VERSION && \
qmake -makefile && \
make -j4 && \
mkdir $PACKAGE && \
mv MiniZincIDE $PACKAGE/ && \
mkdir $PACKAGE/lib && \
for q in \
  libicui18n.so.51 libicuuc.so.51 libicudata.so.51 \
  libQt5Core.so.5 libQt5DBus.so.5 libQt5Gui.so.5 libQt5Widgets.so.5; do \
  cp $QTDIR/$q $PACKAGE/lib/; \
done && \
cp -r $QTDIR/plugins $PACKAGE/ && \
cp ../../MiniZincIDE.sh $PACKAGE/ && \
chmod +x $PACKAGE/MiniZincIDE.sh && \
tar czf $PACKAGE.tgz $PACKAGE && \
mv $PACKAGE.tgz ../..
