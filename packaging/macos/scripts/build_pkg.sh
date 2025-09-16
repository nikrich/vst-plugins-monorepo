#!/usr/bin/env bash
set -euo pipefail

# Build a per-plugin unsigned macOS .pkg that installs a VST3 to /Library/Audio/Plug-Ins/VST3
# Usage: build_pkg.sh <pluginId> [--version X.Y.Z]
# pluginId must exist in packaging/shared/plugins.json

ROOT_DIR=$(cd "$(dirname "$0")/../../.." && pwd)
PKG_DIR="$ROOT_DIR/packaging/macos"
SHARED_DIR="$ROOT_DIR/packaging/shared"
DIST_DIR="$PKG_DIR/dist"
STAGE_BASE="$PKG_DIR/stage"
PLUGINS_JSON="$SHARED_DIR/plugins.json"

plugin_id=""
version_override=""

if [[ ${1:-} != "" ]]; then
  plugin_id="$1"; shift
fi

while [[ $# -gt 0 ]]; do
  case "$1" in
    --version)
      version_override="$2"; shift 2;;
    *) echo "Unknown arg: $1"; exit 2;;
  esac
done

if [[ -z "$plugin_id" ]]; then
  echo "Usage: $0 <pluginId> [--version X.Y.Z]" >&2
  exit 2
fi

mkdir -p "$DIST_DIR" "$STAGE_BASE"

# jq is convenient but not required; parse with python for portability
read_json() {
  python3 - "$@" <<'PY'
import json,sys
path=sys.argv[1]
keypath=sys.argv[2].split('.')
with open(path,'r',encoding='utf-8') as f:
    data=json.load(f)
cur=data
for k in keypath:
    if isinstance(cur,list):
        # Find by id
        cur=next((x for x in cur if x.get('id')==k),None)
    else:
        cur=cur.get(k)
    if cur is None:
        break
print(cur if isinstance(cur,str) else json.dumps(cur))
PY
}

get_plugin_field() {
  local field="$1"
  python3 - "$PLUGINS_JSON" "$plugin_id" "$field" <<'PY'
import json,sys
pjson=sys.argv[1]
pid=sys.argv[2]
field=sys.argv[3]
with open(pjson,'r',encoding='utf-8') as f:
    data=json.load(f)
plugin=next(x for x in data['plugins'] if x['id']==pid)
val=plugin
for part in field.split('.'):
    val=val[part]
print(val)
PY
}

DISPLAY_NAME=$(get_plugin_field displayName)
PKG_ID=$(get_plugin_field mac.pkgId)
SRC_DIR=$(get_plugin_field srcDir)
BUNDLE_NAME=$(get_plugin_field vst3BundleName)

# Resolve version
VERSION="$version_override"
if [[ -z "$VERSION" ]]; then
  # Try CMake project(VERSION) from the plugin CMakeLists.txt
  cmake_file="$ROOT_DIR/$SRC_DIR/CMakeLists.txt"
  if [[ -f "$cmake_file" ]]; then
    VERSION=$(sed -nE 's/^project\([^)]*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' "$cmake_file" | head -n1 || true)
  fi
fi
: "${VERSION:=0.1.0}"

# Locate the built VST3 bundle
find_vst3() {
  local base="$ROOT_DIR/$SRC_DIR/build"
  local found
  # Xcode artefacts
  found=$(find "$base" -type d -path "*/_artefacts/Release/VST3/*.vst3" -print -quit || true)
  if [[ -n "$found" ]]; then echo "$found"; return; fi
  # Makefiles layout (fallback)
  found=$(find "$base" -type d -name "*.vst3" -path "*/Release/*" -print -quit || true)
  if [[ -n "$found" ]]; then echo "$found"; return; fi
  # Generic search
  find "$ROOT_DIR/$SRC_DIR" -type d -name "*.vst3" -print -quit || true
}

VST3_DIR=$(find_vst3)
if [[ -z "$VST3_DIR" ]]; then
  echo "ERROR: Could not find built .vst3 for $DISPLAY_NAME. Build the plugin in Release first." >&2
  exit 1
fi

# Stage
STAGE_DIR="$STAGE_BASE/$plugin_id"
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR/Library/Audio/Plug-Ins/VST3"
cp -R "$VST3_DIR" "$STAGE_DIR/Library/Audio/Plug-Ins/VST3/$BUNDLE_NAME"

# Build pkg
OUT_PKG="$DIST_DIR/${DISPLAY_NAME// /}-$VERSION-mac.pkg"
/usr/bin/pkgbuild \
  --root "$STAGE_DIR" \
  --identifier "$PKG_ID" \
  --version "$VERSION" \
  --install-location "/" \
  "$OUT_PKG"

echo "Built: $OUT_PKG"