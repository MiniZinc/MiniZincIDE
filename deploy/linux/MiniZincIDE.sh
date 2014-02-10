#!/bin/sh
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/lib
export QT_PLUGIN_PATH=$PWD/plugins
exec ./MiniZincIDE $@
