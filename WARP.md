# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

Repository overview
- This repo contains a JUCE-based audio plugin project: Hungry Ghost Limiter
  - New layout root: src/
    - Plugin project: src/HungryGhostLimiter/
    - Shared UI library: src/CommonUI/ (header-only, INTERFACE CMake target)
  - Projucer project: src/HungryGhostLimiter/HungryGhostLimiter.jucer
  - Source code: src/HungryGhostLimiter/Source/** (PluginProcessor.*, PluginEditor.*, ui/*)
  - Shared UI headers: src/CommonUI/include/** (Theme, LookAndFeels, Filmstrip, ResourceResolver, Typography, VerticalMeter, FilmstripSlider, etc.)
  - Auto-generated artefacts during builds:
    - src/HungryGhostLimiter/build/** and src/HungryGhostLimiter/HungryGhostLimiter_artefacts/**
    - Some JUCE-generated JuceLibraryCode for BinaryData and headers — do not edit
  - Vendored JUCE: vendor/JUCE/** (JUCE 8; provides CMake and extras like Projucer, AudioPluginHost)
  - Assets: assets/** (fonts, logo, UI filmstrips)
  - assets/ui/kit-03 contains the UI elements

Prerequisites (macOS)
- Xcode with command-line tools: xcode-select -p (install if missing)
- CMake (brew install cmake)
- Optional: Ninja (brew install ninja) — only if you choose the Ninja generator

CommonUI (shared UI) — header-only
- Target name: CommonUI (INTERFACE)
- Location: src/CommonUI/include
- Consumers: link target CommonUI and include headers from CommonUI/... (umbrella header available)

Build HungryGhostLimiter with CMake
- From src/HungryGhostLimiter:
  - Configure (Unix Makefiles, Debug):
    cmake -S . -B build/debug -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
  - Build (Debug):
    cmake --build build/debug -j 8
  - Configure (Unix Makefiles, Release):
    cmake -S . -B build/release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
  - Build VST3 (Release):
    cmake --build build/release -j 8 --target HungryGhostLimiter_VST3
- Xcode generator variant (as used in CI):
  - Configure:
    cmake -S . -B build/xcode -G Xcode
  - Build:
    cmake --build build/xcode --config Release --target HungryGhostLimiter_VST3
- Tests:
  - Targets: HGLTests_DSP, HGLTests_Proc
  - Run (Debug):
    ./build/debug/HGLTests_DSP_artefacts/Debug/HGLTests_DSP
    ./build/debug/HGLTests_Proc_artefacts/Debug/HGLTests_Proc

One-time: build JUCE extras (Projucer, AudioPluginHost)
- Use JUCE’s CMake with the Xcode generator (recommended on macOS):
  - From repo root (recommended):
    - Configure (build extras):
      cmake -S vendor/JUCE -B build/juce -G Xcode -DJUCE_BUILD_EXTRAS=ON
    - Build Projucer + AudioPluginHost (Release):
      cmake --build build/juce --config Release --target Projucer AudioPluginHost
  - The Projucer CLI lives inside the built .app bundle. Find it with:
      PROJUCER=$(find build/juce -type f -name Projucer -path "*/Projucer_artefacts/*/Projucer.app/Contents/MacOS/*" -print -quit); echo "$PROJUCER"

Generate exporter projects (Xcode) with Projucer (optional)
- The repo does not ship exporter projects. Generate them from the .jucer if you want IDE projects:
  - Run (from src/HungryGhostLimiter/):
      "$PROJUCER" --resave HungryGhostLimiter.jucer
  - This will create Builds/MacOSX/*.xcodeproj (and other exporters if configured in the .jucer)

Install and run (local macOS)
- VST3 install (user):
  - JUCE’s CMake typically installs the built VST3 to ~/Library/Audio/Plug-Ins/VST3 on macOS when using the VST3 target.
  - To locate or manually copy after a Release build:
    VST3=$(find build -type d -name "*.vst3" -print -quit); [ -n "$VST3" ] && cp -R "$VST3" ~/Library/Audio/Plug-Ins/VST3/
- Audio Unit (AU) path (may require sudo for system-wide installs):
  /Library/Audio/Plug-Ins/Components
- Standalone app (if enabled in the .jucer): locate the .app in the build tree and open it:
  APP=$(find build -type d -name "*.app" -print -quit); [ -n "$APP" ] && open "$APP"

Test in JUCE AudioPluginHost (built above)
- Launch the host:
    HOST_APP=$(find build/juce -type d -name "AudioPluginHost.app" -path "*/AudioPluginHost_artefacts/*/Release/*" -print -quit)
    open "$HOST_APP"
- In the host, rescan your plugin folders (e.g., ~/Library/Audio/Plug-Ins/VST3) and insert Hungry Ghost Limiter to test.

Notes on linting and tests
- Lint: No repo-level clang-format/clang-tidy configuration for plugin sources is provided. JUCE ships a .clang-tidy for its own code (src/HungryGhostLimiter/JUCE/.clang-tidy), but plugin source linting isn’t configured.
- Tests: Unit tests exist (HGLTests_DSP, HGLTests_Proc) and are built/run in CI.

High-level architecture (how this plugin is structured)
- Core JUCE plugin pattern
  - Audio processor: src/HungryGhostLimiter/Source/PluginProcessor.*
    - Implements prepareToPlay/processBlock/releaseResources
    - Manages parameters via AudioProcessorValueTreeState (IDs include thresholdL, thresholdR, thresholdLink, outCeiling, release, lookAheadMs, scHpf, safetyClip as described in README.md)
    - DSP pipeline (per README): oversampled true-peak detection/limiting, look-ahead with sliding-window maximum, log-domain release smoothing, stereo max-linking, optional safety soft-clip
  - Editor/UI: src/HungryGhostLimiter/Source/PluginEditor.*
    - Composes custom controls and meters, attaches them to APVTS parameters
  - UI components: src/HungryGhostLimiter/Source/UI/** (plugin-specific UI)
  - Shared UI (header-only): src/CommonUI/include/**
    - Theme, LookAndFeels, Filmstrip, ResourceResolver, Typography, FilmstripSlider, VerticalMeter, etc.
  - Assets: assets/** (fonts/logo embedded via BinaryData generation)
- Auto-generated layer: build outputs and *_artefacts/**
  - Generated by JUCE/CMake; includes BinaryData sources and headers
  - Do not hand-edit; rebuild/regenerate via CMake
- Vendored framework: src/HungryGhostLimiter/JUCE/** (JUCE 8)
  - Provides modules (juce_*), CMake toolchain, and extras (Projucer, AudioPluginHost)

Gotchas and tips specific to this repo
- Paths updated to src/ — in CMake, assets are referenced relative to plugin dir; after the move, use ../../assets from src/HungryGhostLimiter.
- CI uses the Xcode generator and now cleans the build dir before configure to avoid stale CMakeCache.txt.
- .gitignore excludes build/ and Builds/ folders anywhere in the tree.
- If CMake defaults to Ninja and fails (no Ninja installed), force the Xcode generator: -G Xcode.
- True-peak limiting depends on oversampling; ensure Release builds when evaluating performance/quality.

VSCode integration
- Tasks (.vscode/tasks.json) are configured to:
  - Configure and build the plugin from src/HungryGhostLimiter using Unix Makefiles (Debug/Release)
  - Configure and build JUCE AudioPluginHost into build/juce (Release)
  - Compound tasks to build plugin + host
- Launch configs (.vscode/launch.json): launch AudioPluginHost under LLDB, pre-building plugin + host.
- Explorer cleanup (.vscode/settings.json): hides JUCE, JuceLibraryCode, build, Builds folders.

GitHub Actions
- CI workflow builds tests and plugin via CMake (Xcode generator), uploads VST3 artefacts.
- Release workflow builds, zips the VST3 bundle with ditto, and publishes a GitHub Release.
- Both workflows clean src/HungryGhostLimiter/build before configure to avoid stale caches.

UI Overview:
- Big Knob Animation 100 Frames 310*640 px.
- Middle Knob Animation 128 Frames 200*200 px.
- Small Knob Animation 128 Frames 150*150 px.
- VU Meter Animation 128 Frames 320*500 px.
- Big Button 150*150 px.
- Small Button 80*155 px.
- Slider 128*290 px.

Key references
- Project README: README.md (design/parameters/signal flow)
- JUCE CMake entry: src/HungryGhostLimiter/JUCE/CMakeLists.txt
- Projucer project: src/HungryGhostLimiter/HungryGhostLimiter.jucer
- Shared UI CMake target: src/CommonUI/CMakeLists.txt
- Build artefacts: src/HungryGhostLimiter/build/** and *_artefacts/**

