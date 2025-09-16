#!/usr/bin/env bash
set -euo pipefail

# Build Release VST3s (Xcode) and package per-plugin .pkg and the bundle .pkg
# Usage: package_all_macos.sh [--version X.Y.Z] [--plugins limiter astral]

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)

version_override=""
declare -a plugins=("limiter" "astral")

while [[ $# -gt 0 ]]; do
  case "$1" in
    --version) version_override="$2"; shift 2;;
    --plugins) shift; plugins=( ); while [[ $# -gt 0 && $1 != --* ]]; do plugins+=("$1"); shift; done;;
    *) echo "Unknown arg: $1"; exit 2;;
  esac
done

build_plugin() {
  local subdir="$1"
  local target="$2"
  local src="$ROOT_DIR/$subdir"
  local builddir_xcode="$ROOT_DIR/$subdir/build/xcode"
  local builddir_unix="$ROOT_DIR/$subdir/build/unix"

  # Try Xcode first
  if cmake -S "$src" -B "$builddir_xcode" -G Xcode >/dev/null 2>&1; then
    cmake --build "$builddir_xcode" --config Release --target "$target"
    return
  fi

  echo "Xcode generator not available; falling back to Unix Makefiles" >&2
  cmake -S "$src" -B "$builddir_unix" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$builddir_unix" -j 8 --target "$target"
}

# Build Limiter
build_plugin src/HungryGhostLimiter HungryGhostLimiter_VST3
# Build Astral Halls
build_plugin src/HungryGhostReverb HungryGhostReverb_VST3

# Package per plugin
for pid in "${plugins[@]}"; do
  "$ROOT_DIR/packaging/macos/scripts/build_pkg.sh" "$pid" ${version_override:+--version "$version_override"}
done

# Build bundle
"$ROOT_DIR/packaging/macos/scripts/build_bundle.sh" ${version_override:+--version "$version_override"}