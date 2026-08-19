#!/usr/bin/env bash
# Fetch the MiniZinc compiler and the solvers the IDE ships with.
#
# Usage: fetch_minizinc.sh <triple>
#
# Release selection: a tagged build uses the libminizinc release with the same
# tag, anything else uses `edge` (republished on every libminizinc develop push).
# Override with MZN_RELEASE.
#
# Solver versions come from that release's vendor.lock, so the IDE bundles what
# the compiler was tested against. It takes gecode_gist where the CLI package
# takes plain gecode. findMUS and mzn-analyse follow the same tag; Globalizer
# does not version with MiniZinc, so vendor.lock pins it.
#
# Requires: gh (via $GH_TOKEN), tar.
set -euxo pipefail

TRIPLE="${1:?usage: fetch_minizinc.sh <triple>}"
MZN_REPO="${MZN_REPO:-MiniZinc/libminizinc}"
VENDOR_REPO="${VENDOR_REPO:-MiniZinc/minizinc-vendor}"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here"

if [ -n "${MZN_RELEASE:-}" ]; then
  ref="$MZN_RELEASE"
elif [ "${GITHUB_REF_TYPE:-}" = "tag" ]; then
  ref="${GITHUB_REF_NAME}"
else
  ref="edge"
fi

mkdir -p minizinc vendor
gh release download "$ref" --repo "$MZN_REPO" \
  --pattern "minizinc-compiler-only-${TRIPLE}.tar.gz" --dir . --clobber
tar -xzf "minizinc-compiler-only-${TRIPLE}.tar.gz" -C minizinc
rm -f "minizinc-compiler-only-${TRIPLE}.tar.gz"

gh release download "$ref" --repo "$MZN_REPO" --pattern "vendor.lock" --dir . --clobber

deps="gecode_gist chuffed highs or-tools"

for dep in $deps; do
  # gecode_gist is the same source as gecode, published under gecode's version.
  key="$dep"
  [ "$dep" = "gecode_gist" ] && key="gecode"
  ver="$(grep -E "^${key}=" vendor.lock | head -1 | cut -d= -f2-)"
  [ -n "$ver" ] || { echo "no version pinned for '$key' in vendor.lock" >&2; exit 1; }
  asset="${dep}-${ver}-${TRIPLE}.tar.gz"
  gh release download "${dep}-${ver}" --repo "$VENDOR_REPO" --pattern "$asset" --dir . --clobber
  tar -xzf "$asset" -C vendor
  rm -f "$asset"
done

# Laid out as <tool>/bin and <tool>/share/minizinc, the paths the .iss and
# package_ide.sh expect.
for tool in findMUS:FindMUS mzn-analyse:mzn-analyse; do
  dir="${tool%%:*}"; repo="${tool##*:}"
  asset="${dir}-${TRIPLE}.tar.gz"
  mkdir -p "$dir"
  gh release download "$ref" --repo "MiniZinc/$repo" --pattern "$asset" --dir . --clobber
  tar -xzf "$asset" -C "$dir"
  rm -f "$asset"
done

# OpenSSL is only needed by this installer, but pinned with everything else.
case "$TRIPLE" in
  *windows*)
    over="$(grep -E '^openssl=' vendor.lock | head -1 | cut -d= -f2-)"
    [ -n "$over" ] || { echo "no version pinned for 'openssl' in vendor.lock" >&2; exit 1; }
    asset="openssl-${over}-${TRIPLE}.tar.gz"
    gh release download "openssl-${over}" --repo "$VENDOR_REPO" --pattern "$asset" --dir . --clobber
    tar -xzf "$asset" -C vendor
    rm -f "$asset"
    ;;
esac

# GHC cannot target Windows ARM64, so Globalizer has no build there.
if [ "$TRIPLE" != aarch64-windows ]; then
  gver="$(grep -E '^globalizer=' vendor.lock | head -1 | cut -d= -f2-)"
  [ -n "$gver" ] || { echo "no version pinned for 'globalizer' in vendor.lock" >&2; exit 1; }
  mkdir -p globalizer
  gh release download "$gver" --repo MiniZinc/Globalizer \
    --pattern "globalizer-${TRIPLE}.tar.gz" --dir . --clobber
  tar -xzf "globalizer-${TRIPLE}.tar.gz" -C globalizer
  rm -f "globalizer-${TRIPLE}.tar.gz"
fi
