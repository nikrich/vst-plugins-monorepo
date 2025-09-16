#!/usr/bin/env bash
set -euo pipefail

PLUGIN_NAME="HungryGhostSpectralLimiter"
VERSION="0.1.1"
BUNDLE_ID="com.hungryghost.${PLUGIN_NAME,,}"

DEV_APP_CERT="Developer ID Application: Your Company (TEAMID)"
DEV_INSTALL_CERT="Developer ID Installer: Your Company (TEAMID)"

STAGE=stage
DIST=dist
PKG="$DIST/${PLUGIN_NAME}-${VERSION}.pkg"

rm -rf "$STAGE" "$DIST"
mkdir -p "$STAGE/Library/Audio/Plug-Ins/VST3"
mkdir -p "$STAGE/Library/Audio/Plug-Ins/Components"
mkdir -p "$STAGE/Library/Application Support/Avid/Audio/Plug-Ins"  # optional AAX
mkdir -p "$DIST" scripts

# Copy JUCE artifacts
cp -R "build/Release/${PLUGIN_NAME}.vst3" \
      "$STAGE/Library/Audio/Plug-Ins/VST3/"
cp -R "build/Release/${PLUGIN_NAME}.component" \
      "$STAGE/Library/Audio/Plug-Ins/Components/"
# cp -R "build/Release/${PLUGIN_NAME}.aaxplugin" \
#      "$STAGE/Library/Application Support/Avid/Audio/Plug-Ins/"

# Sign each bundle with hardened runtime
codesign --force --deep --options runtime --timestamp -s "$DEV_APP_CERT" \
  "$STAGE/Library/Audio/Plug-Ins/VST3/${PLUGIN_NAME}.vst3"
codesign --force --deep --options runtime --timestamp -s "$DEV_APP_CERT" \
  "$STAGE/Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component"
# codesign --force --deep --options runtime --timestamp -s "$DEV_APP_CERT" \
#  "$STAGE/Library/Application Support/Avid/Audio/Plug-Ins/${PLUGIN_NAME}.aaxplugin"

# Postinstall: register AU so it appears immediately
cat > scripts/postinstall <<'EOS'
#!/bin/sh
/usr/bin/pluginkit -a "/Library/Audio/Plug-Ins/Components/HungryGhostReverb.component" || true
exit 0
EOS
chmod +x scripts/postinstall

# Build the pkg (installing content at filesystem root)
pkgbuild --root "$STAGE" \
  --identifier "$BUNDLE_ID" \
  --version "$VERSION" \
  --install-location "/" \
  --scripts scripts \
  "$DIST/${PLUGIN_NAME}.pkg"

# Sign the pkg
productsign --sign "$DEV_INSTALL_CERT" "$DIST/${PLUGIN_NAME}.pkg" "$PKG"

# Notarize & staple (set up a keychain profile once with notarytool)
xcrun notarytool submit "$PKG" --keychain-profile "AC_PASSWORD" --wait
xcrun stapler staple "$PKG"
