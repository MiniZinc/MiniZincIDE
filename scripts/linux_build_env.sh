#!/usr/bin/env bash
# Prepare a manylinux_2_28 container to build the IDE. Source, do not execute.
#
# manylinux_2_28 matches libminizinc and minizinc-vendor (glibc 2.28); anything
# newer raises the whole bundle's floor. Qt 6.10 would too: it needs glibc 2.34.
#
# Env: QT_VERSION (required), QT_MODULES (optional, space separated)
set -eu

: "${QT_VERSION:?QT_VERSION must be set}"

# manylinux ships gcc-toolset rather than a new system gcc.
for f in /opt/rh/gcc-toolset-*/enable; do
  # shellcheck disable=SC1090
  [ -e "$f" ] && . "$f"
done

# xcb-util-* are what linuxdeploy's qt plugin needs for the xcb platform plugin.
# Several live in EPEL and CodeReady (`powertools` on EL8, `crb` on EL9). Not
# silenced: a missing package only surfaces later, as a broken bundle.
QT_DEPS="alsa-lib cups-libs fontconfig freetype-devel libxkbcommon-x11 \
  libX11-devel libxcb-devel libxkbcommon-devel libXcomposite-devel \
  libXcursor-devel libXi-devel libXrandr-devel libXtst-devel mesa-libGL-devel \
  xcb-util-cursor xcb-util-image xcb-util-keysyms xcb-util-renderutil xcb-util-wm"

dnf -y install epel-release 2>/dev/null || true
# shellcheck disable=SC2086
dnf -y --enablerepo=powertools install $QT_DEPS \
  || dnf -y --enablerepo=crb install $QT_DEPS \
  || dnf -y install $QT_DEPS

# The system python3 (AlmaLinux 8 => 3.6) is too old for aqtinstall.
PY=$(ls -d /opt/python/cp311-cp311/bin/python /opt/python/cp312-cp312/bin/python 2>/dev/null | head -1)
"$PY" -m pip install --quiet aqtinstall

# Qt renamed the Linux desktop arch (6.5 = gcc_64, 6.9+ = linux_gcc_64).
QT_ARCH=$("$PY" -m aqt list-qt linux desktop --arch "$QT_VERSION" \
  | tr ' ' '\n' | grep -E '^(linux_)?gcc_64$' | head -1)

# shellcheck disable=SC2086
"$PY" -m aqt install-qt linux desktop "$QT_VERSION" "$QT_ARCH" \
  ${QT_MODULES:+-m $QT_MODULES} --outputdir /opt/qt

QT_DIR=$(ls -d /opt/qt/"$QT_VERSION"/*/ | head -1)
QT_DIR=${QT_DIR%/}
export PATH="$QT_DIR/bin:$PATH"
export QT_ROOT_DIR="$QT_DIR"
# linuxdeploy resolves dependencies the way the loader would, so Qt must be here.
export LD_LIBRARY_PATH="$QT_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
qmake --version
