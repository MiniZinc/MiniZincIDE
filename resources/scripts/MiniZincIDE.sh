#!/bin/sh
DIR=$(dirname $(readlink -f "$0"))
[ -n "$XDG_RUNTIME_DIR" ] && mkdir -p $XDG_RUNTIME_DIR -m 700
export LD_LIBRARY_PATH="$DIR"/lib:$LD_LIBRARY_PATH
export QT_PLUGIN_PATH="$DIR"/plugins

if ldd "$DIR"/bin/MiniZincIDE | grep -q 'libnss3.so => not found' ; then
    export LD_LIBRARY_PATH="$DIR"/lib_fallback:$LD_LIBRARY_PATH
fi

exec "$DIR"/bin/MiniZincIDE $@
