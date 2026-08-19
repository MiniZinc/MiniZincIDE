#!/usr/bin/env bash
# Build the drag-to-Applications .dmg from a signed MiniZincIDE.app.
#
# Usage: build_dmg.sh <app-path> <version> <triple>
#
# Requires: create-dmg (brew install create-dmg).
set -euo pipefail

APP="${1:?usage: build_dmg.sh <app-path> <version> <triple>}"
VERSION="${2:?usage: build_dmg.sh <app-path> <version> <triple>}"
TRIPLE="${3:?usage: build_dmg.sh <app-path> <version> <triple>}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[ -d "$APP" ] || { echo "not a directory: $APP" >&2; exit 1; }

OUT="MiniZincIDE-${VERSION}-${TRIPLE}.dmg"
rm -f "$OUT"

# Everything in this folder lands on the volume, so it holds only the app;
# --app-drop-link supplies the Applications symlink.
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
cp -R "$APP" "$STAGE/"

# --window-size must equal the background's point size (pixels / dpi * 72), which
# create-dmg uses as-is rather than scaling to fit; anything else leaves white
# padding. The PNG is 1320x840 @144dpi = 660x420pt, so gen_dmg_background.py has
# to keep that dpi tag.
#
# Icons are placed above the guide boxes drawn into the background (centres
# x=150/510, y=260), not level with them: Finder renders ~30pt lower than asked
# and puts the filename below the icon, which would otherwise fall outside.
create-dmg \
  --volname "MiniZinc IDE $VERSION" \
  --volicon "$HERE/MiniZincIDE/mznide.icns" \
  --background "$HERE/resources/misc/dmg-background.png" \
  --window-size 660 420 \
  --icon-size 96 \
  --icon "MiniZincIDE.app" 150 218 \
  --app-drop-link 510 218 \
  --hide-extension "MiniZincIDE.app" \
  "$OUT" \
  "$STAGE" \
  || true   # create-dmg exits non-zero on the harmless "Finder didn't quit in time" race

[ -f "$OUT" ] || { echo "create-dmg did not produce $OUT" >&2; exit 1; }
echo "$OUT"
