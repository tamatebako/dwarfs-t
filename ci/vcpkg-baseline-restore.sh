#!/usr/bin/env bash
# vcpkg-baseline-restore.sh — restore a vcpkg baseline artifact if one
# exists for this manifest, else do nothing (the caller builds normally).
#
# Usage: vcpkg-baseline-restore.sh <manifest-dir> <triplet> [install-root]
#
#   <manifest-dir>  the directory holding vcpkg.json (+ vcpkg_ports/,
#                   vcpkg_triplets/ when present)
#   <triplet>       e.g. x64-mingw-static, arm64-osx-static, x64-linux-static
#   [install-root]  OVERRIDE — producers only. Consumers omit it: the
#                   canonical root comes from the map below (the single
#                   owner; vcpkg trees embed absolute paths, so a consumer
#                   extracting anywhere else breaks the tree).
#
# Prints "restored <sha> <triplet>" and exits 0 on a restore; prints
# "miss" and exits 1 when no baseline exists (never a build here — the
# dwarfs-t vcpkg-baseline.yml is the single writer).
set -euo pipefail

MANIFEST_DIR="${1:?usage: vcpkg-baseline-restore.sh <manifest-dir> <triplet> [install-root]}"
TRIPLET="${2:?triplet required}"

# The canonical install roots — the values the producer passes as
# --x-install-root. Single owner: every consumer resolves through this
# map, never a hardcoded path of its own.
if [ $# -ge 3 ]; then
  INSTALL_ROOT="$3"
else
  case "$TRIPLET" in
    x64-mingw-static)                 INSTALL_ROOT="C:/vcpkg-inst" ;;
    arm64-osx-static|x64-osx-static)  INSTALL_ROOT="/Users/runner/vcpkg-inst" ;;
    x64-linux-static|arm64-linux-static) INSTALL_ROOT="/home/runner/vcpkg-inst" ;;
    *) echo "unknown triplet '$TRIPLET' (no canonical root registered)" >&2; exit 64 ;;
  esac
fi

# The consumer's build env flows from the same map: on GitHub the caller
# never sets DWARFS_RS_VCPKG_INSTALLED_DIR itself.
if [ -n "${GITHUB_ENV:-}" ]; then
  echo "DWARFS_RS_VCPKG_INSTALLED_DIR=$INSTALL_ROOT" >> "$GITHUB_ENV"
fi

REPO="${VCPKG_BASELINE_REPO:-tamatebako/dwarfs-t}"

# The same key the baseline workflow computes. sha256sum exists on
# Git-bash + linux; macOS has only shasum.
sha=$(cat "$MANIFEST_DIR/vcpkg.json" $(find "$MANIFEST_DIR/vcpkg_ports" "$MANIFEST_DIR/vcpkg_triplets" -type f 2>/dev/null | sort) | { sha256sum 2>/dev/null || shasum -a 256; } | cut -d' ' -f1)
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
