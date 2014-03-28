#!/bin/bash
QTDIR=~/Qt5.2.1/5.2.1/gcc_64
export PATH=$QTDIR/bin:$PATH
rm -rf build-ide
mkdir build-ide
if [ "x$2" == "x" ]; then
VERSION=$1
cd build-ide
wget http://www.minizinc.org/downloads/MiniZincIDE/MiniZincIDE-$VERSION.tgz
else
VERSION=$2
cp $1 build-ide/MiniZincIDE-$VERSION.tgz
cd build-ide
fi
PACKAGE=MiniZincIDE-$VERSION-linux-x86_64
tar xzf MiniZincIDE-$VERSION.tgz && \
cd MiniZincIDE-$VERSION && \
qmake -makefile && \
make -j2 && \
mkdir $PACKAGE && \
mv MiniZincIDE $PACKAGE/ && \
mkdir $PACKAGE/lib && \
for q in \
  libicui18n.so.51 libicuuc.so.51 libicudata.so.51 \
  libQt5Core.so.5 libQt5DBus.so.5 libQt5Gui.so.5 libQt5Widgets.so.5; do \
  cp $QTDIR/lib/$q $PACKAGE/lib/; \
done && \
cp -r $QTDIR/plugins $PACKAGE/ && \
cp ../../MiniZincIDE.sh $PACKAGE/ && \
chmod +x $PACKAGE/MiniZincIDE.sh && \
tar czf $PACKAGE.tgz $PACKAGE && \
mv $PACKAGE.tgz ../..
