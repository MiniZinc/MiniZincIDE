#!/usr/bin/env bash
# Assemble an IDE bundle: IDE + compiler + solvers.
#
# Usage: package_ide.sh <linux|appimage|osx> <version> <triple>
#
# Expects minizinc/ and vendor/ (fetch_minizinc.sh) and build/ (build_ide.sh).
set -euo pipefail

MODE="${1:?usage: package_ide.sh <linux|appimage|osx> <version> <triple>}"
VERSION="${2:?usage: package_ide.sh <linux|appimage|osx> <version> <triple>}"
TRIPLE="${3:?usage: package_ide.sh <linux|appimage|osx> <version> <triple>}"
ROOT="${ROOT:-$PWD}"
cd "$ROOT"

# $1 vendor dir, $2 binary, $3 bin dest, $4 share/minizinc dest
add_solver() {
  cp "vendor/$1/bin/$2" "$3/"
  cp -a "vendor/$1/share/minizinc/." "$4/"
}

# Same, for the tools fetched from their own repositories.
add_tool() {
  cp "$1/bin/$2${EXE:-}" "$3/"
  # mzn-analyse installs only a binary; the others also carry an mznlib.
  if [ -d "$1/share/minizinc" ]; then
    cp -a "$1/share/minizinc/." "$4/"
  fi
}

# The linux bundle and the AppImage differ only in the usr/ prefix.
assemble_linux_payload() {
  prefix="$1"
  mkdir -p "$prefix/bin" "$prefix/lib" "$prefix/share/minizinc"
  cp -a ide/usr/. "$prefix/"
  cp -a minizinc/bin/. "$prefix/bin/"
  cp -a minizinc/share/. "$prefix/share/"
  add_solver gecode_gist fzn-gecode "$prefix/bin" "$prefix/share/minizinc"
  patchelf --set-rpath '$ORIGIN/../lib' "$prefix/bin/fzn-gecode"
  add_solver chuffed fzn-chuffed "$prefix/bin" "$prefix/share/minizinc"
  add_solver or-tools fzn-cp-sat "$prefix/bin" "$prefix/share/minizinc"
  cp -a vendor/highs/lib64/libhighs.so* "$prefix/lib/"
  add_tool globalizer minizinc-globalizer "$prefix/bin" "$prefix/share/minizinc"
  add_tool findMUS findMUS "$prefix/bin" "$prefix/share/minizinc"
  add_tool mzn-analyse mzn-analyse "$prefix/bin" "$prefix/share/minizinc"
  for f in minizinc mzn2doc fzn-gecode fzn-chuffed fzn-cp-sat \
           minizinc-globalizer findMUS mzn-analyse; do
    strip "$prefix/bin/$f" 2>/dev/null || true
  done
  # Roughly 15% of libhighs.so is its unstripped symbol table.
  strip "$prefix"/lib/libhighs.so* 2>/dev/null || true
}

case "$MODE" in
linux)
  PACKAGE="MiniZincIDE-${VERSION}-${TRIPLE}"
  rm -rf "$PACKAGE" && mkdir -p "$PACKAGE"
  assemble_linux_payload "$PACKAGE"
  cp resources/scripts/MiniZincIDE.sh "$PACKAGE/"
  cp resources/misc/README "$PACKAGE/" 2>/dev/null || true
  tar -czf "${PACKAGE}.tgz" "$PACKAGE"
  ;;

appimage)
  PACKAGE="MiniZinc.AppDir"
  rm -rf "$PACKAGE" && mkdir -p "$PACKAGE/usr"
  assemble_linux_payload "$PACKAGE/usr"
  cp resources/misc/README "$PACKAGE/" 2>/dev/null || true
  # appimagetool, not `linuxdeploy --output appimage`: the project's AppRun
  # dispatches on the symlink name, provides `install` and sets QT_PLUGIN_PATH,
  # all of which linuxdeploy's generic AppRun would drop.
  cp resources/scripts/AppRun "$PACKAGE/"
  chmod +x "$PACKAGE/AppRun"
  cp resources/misc/minizinc.desktop "$PACKAGE/minizinc.desktop"
  cp resources/icon.png "$PACKAGE/minizinc.png"
  OUT="MiniZincIDE-${VERSION}-${TRIPLE}.AppImage"
  ARCH=x86_64 appimagetool "$PACKAGE" "$OUT"
  ;;

osx)
  APP="MiniZincIDE.app"
  DIR="$APP/Contents/Resources"
  MZNDIR="$DIR/share/minizinc"
  [ -d "$APP" ] || { echo "missing $APP (build first)" >&2; exit 1; }
  mkdir -p "$MZNDIR/solvers" "$DIR/bin" "$DIR/lib"
  # On macOS the compiler goes directly in Resources/, where the IDE looks.
  cp -a minizinc/bin/. "$DIR/"
  cp -a minizinc/share/minizinc/. "$MZNDIR/"
  add_solver gecode_gist fzn-gecode "$DIR/bin" "$MZNDIR"
  cp resources/misc/osx-gecode-qt.conf "$DIR/bin/qt.conf"
  add_solver chuffed fzn-chuffed "$DIR/bin" "$MZNDIR"
  add_solver or-tools fzn-cp-sat "$DIR/bin" "$MZNDIR"
  cp -a vendor/highs/lib/libhighs*.dylib "$DIR/lib/"
  add_tool globalizer minizinc-globalizer "$DIR/bin" "$MZNDIR"
  add_tool findMUS findMUS "$DIR/bin" "$MZNDIR"
  add_tool mzn-analyse mzn-analyse "$DIR/bin" "$MZNDIR"
  (cd "$DIR" && strip minizinc mzn2doc 2>/dev/null || true)
  (cd "$DIR/bin" && strip fzn-gecode fzn-chuffed fzn-cp-sat \
       minizinc-globalizer findMUS mzn-analyse 2>/dev/null || true)
  strip "$DIR"/lib/libhighs*.dylib 2>/dev/null || true
  # macdeployqt deploys every Qt SQL driver and errors on each whose client
  # library is absent. Only QSQLITE is used (cp-profiler's db_handler).
  if [ -n "${QT_ROOT_DIR:-}" ]; then
    rm -f "$QT_ROOT_DIR"/plugins/sqldrivers/libqsql{psql,odbc,mimer,mysql,ibase,oci}.dylib
  fi
  macdeployqt "$APP" -executable="$DIR/bin/fzn-gecode"
  ;;

*)
  echo "unknown mode: $MODE" >&2; exit 1 ;;
esac
