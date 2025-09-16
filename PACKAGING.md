# Packaging: macOS and Windows installers

This repo contains scripts to create per‑plugin installers and a bundle installer for both macOS and Windows.

What gets built
- macOS per‑plugin installer: .pkg that installs VST3 to /Library/Audio/Plug-Ins/VST3
- macOS bundle installer: .pkg that installs all plugins in one run (shows EULA)
- Windows per‑plugin installer: .msi that installs VST3 to C:\\Program Files\\Common Files\\VST3 (WixUI_Minimal)
- Windows bundle installer: .exe bootstrapper chaining both MSIs (shows EULA)

Notes
- Formats: VST3 only (no AU/AAX)
- macOS packages are unsigned/notarized by default. You can ship unsigned locally; notarization can be added later.
- Windows signing is optional via environment variables; real code‑signing certificates must be purchased from a CA and cannot be generated here. A self‑signed cert is only suitable for internal testing.

Directory layout
- packaging/shared/
  - plugins.json: product metadata (names, IDs, GUIDs)
  - EULA.rtf: shared license text displayed in installers
- packaging/macos/{scripts,templates,stage,dist}
- packaging/windows/{scripts,templates,obj,dist}

Prerequisites
- macOS: Xcode + command line tools, CMake
- Windows: Visual Studio 2022 (x64), CMake, WiX Toolset v3.11+ (for local Windows builds)

Build plugins (Release)
- macOS (Xcode generator):
  - Limiter: cmake -S src/HungryGhostLimiter -B src/HungryGhostLimiter/build/xcode -G Xcode
  - cmake --build src/HungryGhostLimiter/build/xcode --config Release --target HungryGhostLimiter_VST3
  - Astral Halls: cmake -S src/HungryGhostReverb -B src/HungryGhostReverb/build/xcode -G Xcode
  - cmake --build src/HungryGhostReverb/build/xcode --config Release --target HungryGhostReverb_VST3
- Windows (VS 2022 generator, x64):
  - Limiter: cmake -S src/HungryGhostLimiter -B src/HungryGhostLimiter/build/vs -G "Visual Studio 17 2022" -A x64
  - cmake --build src/HungryGhostLimiter/build/vs --config Release --target HungryGhostLimiter_VST3
  - Astral Halls: cmake -S src/HungryGhostReverb -B src/HungryGhostReverb/build/vs -G "Visual Studio 17 2022" -A x64
  - cmake --build src/HungryGhostReverb/build/vs --config Release --target HungryGhostReverb_VST3

Create installers (local)
- macOS per‑plugin pkg:
  - ./scripts/package_all_macos.sh --plugins limiter astral --version 0.1.0
  - Outputs in packaging/macos/dist/
- Windows per‑plugin MSI + bundle EXE (on Windows):
  - scripts\\package_all_windows.ps1 -Plugins limiter,astral -Version 0.1.0
  - Outputs in packaging\windows\dist\

Windows signing (optional)
- Provide these environment variables before running the packaging scripts if you want to sign MSIs/EXEs:
  - WINDOWS_SIGNING_CERT_BASE64: Base64 of your PFX (DO NOT commit to repo)
  - WINDOWS_SIGNING_CERT_PASSWORD: Password for the PFX
  - WINDOWS_SIGNING_TIMESTAMP_URL: e.g. http://timestamp.digicert.com
  - Set SIGN_WINDOWS=1 to enable signing

macOS EULA
- The EULA is shown in the bundle .pkg (Distribution XML). Per‑plugin pkgs built via pkgbuild do not display a EULA.

CI
- macOS and Windows jobs can call the same scripts. The workflows can upload the per‑plugin packages and the bundle package as artifacts or attach them to Releases.
