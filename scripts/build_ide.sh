#!/usr/bin/env bash
# Build the IDE and lay it out for packaging. Run natively (osx) or inside the
# manylinux container (linux; see linux_build_env.sh).
#
# Env: ROOT, BUILD_REF, IDE_PLATFORM (linux|osx), MAKE
set -eux

ROOT="${ROOT:-$PWD}"
cd "$ROOT"

mkdir -p build
cd build
# Stated, not inherited from Qt's mkspec, so the IDE, compiler and solvers share
# one floor.
qmake -makefile "CONFIG+=bundled" \
  "PREFIX=/usr" \
  "QMAKE_MACOSX_DEPLOYMENT_TARGET=12.0" \
  "DEFINES+=MINIZINC_IDE_BUILD=\\\\\\\"\"${BUILD_REF:-0}\\\\\\\"\"" \
  "$ROOT/MiniZincIDE/MiniZincIDE.pro"
"${MAKE:-make}" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"

case "${IDE_PLATFORM:-}" in
linux)
  "${MAKE:-make}" INSTALL_ROOT="$ROOT/ide" install
  test -x "$ROOT/ide/usr/bin/MiniZincIDE"
  cd "$ROOT"
  # TODO: linuxdeploy only publishes a rolling `continuous` tag - the one
  # unpinned input left in the chain. Pin by asset digest if it drifts.
  LD_BASE="https://github.com/linuxdeploy/linuxdeploy"
  curl -sSLo linuxdeploy "$LD_BASE/releases/download/continuous/linuxdeploy-x86_64.AppImage"
  curl -sSLo linuxdeploy-plugin-qt \
    "$LD_BASE-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
  chmod +x linuxdeploy linuxdeploy-plugin-qt
  # The IDE's profiler uses only QSQLITE. Avoid deploying unused drivers whose
  # optional database-client dependencies are not part of the manylinux image.
  rm -f "$QT_ROOT_DIR"/plugins/sqldrivers/libqsql{psql,odbc,mimer,mysql,ibase,oci}.so
  export APPIMAGE_EXTRACT_AND_RUN=1   # no FUSE in the container
  PATH="$ROOT:$PATH" ./linuxdeploy --appdir ide \
    --executable "$ROOT/ide/usr/bin/MiniZincIDE" \
    --executable vendor/gecode_gist/bin/fzn-gecode --plugin qt -v0
  test -e "$ROOT/ide/usr/lib/libQt6WebSockets.so.6"
  rm -rf ide/usr/share ide/usr/translations ide/usr/bin/fzn-gecode
  ;;
osx)
  cp -R MiniZincIDE.app "$ROOT/"
  ;;
esac
