#!/usr/bin/env bash
# vcpkg-baseline-restore.sh — restore a vcpkg baseline artifact if one
# exists for this manifest, else do nothing (the caller builds normally).
#
# Usage: vcpkg-baseline-restore.sh <manifest-dir> <triplet> <install-root>
#
#   <manifest-dir>  the directory holding vcpkg.json (+ vcpkg_ports/,
#                   vcpkg_triplets/ when present)
#   <triplet>       e.g. x64-mingw-static, arm64-osx-static, x64-linux-static
#   <install-root>  the CANONICAL path the baseline was built at:
#                   C:/vcpkg-inst (windows), /Users/runner/vcpkg-inst (mac),
#                   /home/runner/vcpkg-inst (linux)
#
# Prints "restored <sha> <triplet>" and exits 0 on a restore; prints
# "miss" and exits 1 when no baseline exists (never a build here — the
# dwarfs-t vcpkg-baseline.yml is the single writer).
set -euo pipefail

MANIFEST_DIR="${1:?usage: vcpkg-baseline-restore.sh <manifest-dir> <triplet> <install-root>}"
TRIPLET="${2:?triplet required}"
INSTALL_ROOT="${3:?install root required}"

REPO="${VCPKG_BASELINE_REPO:-tamatebako/dwarfs-t}"

# The same key the baseline workflow computes. shasum -a 256 (not
# sha256sum): macOS runners have no coreutils.
sha=$(cat "$MANIFEST_DIR/vcpkg.json" $(find "$MANIFEST_DIR/vcpkg_ports" "$MANIFEST_DIR/vcpkg_triplets" -type f 2>/dev/null | sort) | shasum -a 256 | cut -d' ' -f1)
sha="${sha:0:16}"
TAG="vcpkg-baseline-${sha}-${TRIPLET}"
ARTIFACT="vcpkg-baseline-${TRIPLET}.tar.gz"

if ! gh release view "$TAG" --repo "$REPO" > /dev/null 2>&1; then
  echo "miss"
  exit 1
fi

mkdir -p "$INSTALL_ROOT"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
gh release download "$TAG" --repo "$REPO" --pattern "$ARTIFACT" --dir "$tmp" --clobber
tar -xzf "$tmp/$ARTIFACT" -C "$INSTALL_ROOT"
echo "restored $sha $TRIPLET"
