#!/usr/bin/env bash
set -euo pipefail

# Build a bundle .pkg that chains both per-plugin pkgs and shows a EULA
# Usage: build_bundle.sh [--version X.Y.Z]

ROOT_DIR=$(cd "$(dirname "$0")/../../.." && pwd)
PKG_DIR="$ROOT_DIR/packaging/macos"
SHARED_DIR="$ROOT_DIR/packaging/shared"
DIST_DIR="$PKG_DIR/dist"
TEMPLATES_DIR="$PKG_DIR/templates"

version_override=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --version) version_override="$2"; shift 2;;
    *) echo "Unknown arg: $1"; exit 2;;
  esac
done

# Get plugin metadata
readarray -t PLUG_IDS < <(python3 - <<'PY'
import json
with open('packaging/shared/plugins.json','r') as f:
    data=json.load(f)
print('\n'.join([p['id'] for p in data['plugins']]))
PY
)

get_field() {
  python3 - "$@" <<'PY'
import json,sys
path=sys.argv[1]
plugin_id=sys.argv[2]
field=sys.argv[3]
with open(path,'r') as f:
    data=json.load(f)
plugin=next(x for x in data['plugins'] if x['id']==plugin_id)
val=plugin
for part in field.split('.'):
    val=val[part]
print(val)
PY
}

bundle_name=$(python3 -c 'import json;print(json.load(open("packaging/shared/plugins.json"))[["bundleDisplayName"]])' 2>/dev/null || echo "Hungry Ghost Plugins")
: "${bundle_name:=Hungry Ghost Plugins}"

# Resolve bundle version
BUNDLE_VERSION="$version_override"
if [[ -z "$BUNDLE_VERSION" ]]; then
  # derive from max of per-plugin versions or default
  vers=()
  for pid in "${PLUG_IDS[@]}"; do
    cmake_file=$(get_field packaging/shared/plugins.json "$pid" srcDir)/CMakeLists.txt
    cmake_file="$ROOT_DIR/$cmake_file"
    v=$(sed -nE 's/^project\([^)]*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' "$cmake_file" | head -n1 || true)
    [[ -n "$v" ]] && vers+=("$v")
  done
  if [[ ${#vers[@]} -gt 0 ]]; then
    BUNDLE_VERSION=$(printf '%s\n' "${vers[@]}" | sort -V | tail -n1)
  else
    BUNDLE_VERSION=0.1.0
  fi
fi

# Ensure per-plugin pkgs exist and capture paths/ids/versions
PKG_REFS=""
CHOICES=""
idx=1
for pid in "${PLUG_IDS[@]}"; do
  display=$(get_field packaging/shared/plugins.json "$pid" displayName)
  pkg_id=$(get_field packaging/shared/plugins.json "$pid" mac.pkgId)
  # Find matching pkg file in dist
  pkg_path=$(ls "$DIST_DIR" | grep -i "${display// /}-.*-mac\.pkg$" | head -n1 || true)
  if [[ -z "$pkg_path" ]]; then
    echo "ERROR: Per-plugin pkg for $display not found in $DIST_DIR. Build those first." >&2
    exit 1
  fi
  pkg_full="$DIST_DIR/$pkg_path"
  # Extract version from filename
  ver=$(echo "$pkg_path" | sed -E 's/.*-([0-9]+\.[0-9]+\.[0-9]+)-mac\.pkg/\1/')
  PKG_REFS+="    <pkg-ref id=\"$pkg_id\" version=\"$ver\" onConclusion=\"RequireRestart\">$pkg_full</pkg-ref>\n"
  CHOICES+="      <choice id=\"choice$idx\" title=\"$display\" enabled=\"true\" visible=\"true\"><pkg-ref id=\"$pkg_id\"/></choice>\n"
  idx=$((idx+1))
done

LICENSE_PATH="$SHARED_DIR/EULA.rtf"

DIST_XML="$PKG_DIR/templates/distribution.xml"
cat > "$DIST_XML" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
  <title>Hungry Ghost Plugins</title>
  <license file="$LICENSE_PATH"/>
  <options customize="allow" require-scripts="false"/>
  <choices-outline>
    <line choice="default">
$(printf "%s" "$CHOICES")
    </line>
  </choices-outline>
$PKG_REFS
</installer-gui-script>
XML

OUT_PKG="$DIST_DIR/HungryGhostPlugins-$BUNDLE_VERSION-mac.pkg"
/usr/bin/productbuild \
  --distribution "$DIST_XML" \
  --package-path "$DIST_DIR" \
  "$OUT_PKG"

echo "Built: $OUT_PKG"