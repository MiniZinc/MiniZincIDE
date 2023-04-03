#!/bin/sh
DIR=$(dirname $(readlink -f "$0"))
[ -n "$XDG_RUNTIME_DIR" ] && mkdir -p $XDG_RUNTIME_DIR -m 700
export LD_LIBRARY_PATH="$DIR"/lib:$LD_LIBRARY_PATH
export QT_PLUGIN_PATH="$DIR"/plugins

exec "$DIR"/bin/MiniZincIDE $@
