#!/bin/bash
rm -rf archive-ide
mkdir archive-ide
cd archive-ide
git clone http://www.minizinc.org/repos/MiniZincIDE
cd MiniZincIDE
if [ "$1" == "rotd" ]; then git checkout develop; fi
if [ "$1" == "release" ]; then git checkout release/$2; fi
TAG=`git describe --always`
DATE=`date "+%Y-%m-%d"`
if [ "$1" == "rotd" ]
then VERSION=${DATE}-${TAG}
elif [ "$1" == "release" ]
then VERSION=$2
else VERSION=${TAG}
fi
git archive --prefix=MiniZincIDE-${VERSION}/ --output=MiniZincIDE-${VERSION}.tgz HEAD:MiniZincIDE/
mv MiniZincIDE-${VERSION}.tgz ../../
cd ../..
rm -rf archive-ide
