# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

Repository overview
- This repo contains a JUCE-based audio plugin project: Hungry Ghost Limiter
  - Project root: HungryGhostLimiter/
  - Projucer project: HungryGhostLimiter/HungryGhostLimiter.jucer
  - Source code: HungryGhostLimiter/Source/** (PluginProcessor.*, PluginEditor.*, ui/*, styling/*)
  - Auto-generated glue: HungryGhostLimiter/JuceLibraryCode/** (do not edit directly)
  - Vendored JUCE: HungryGhostLimiter/JUCE/** (JUCE 8; provides CMake and extras like Projucer, AudioPluginHost)
  - Assets: assets/** (fonts, logo)
- There is no existing WARP.md, no tests, and no committed exporter/build projects. You must generate them.

Prerequisites (macOS)
- Xcode with command-line tools: xcode-select -p (install if missing)
- CMake (brew install cmake)
- Optional: Ninja (brew install ninja) — only if you choose the Ninja generator

One-time: build JUCE extras (Projucer, AudioPluginHost)
- Use JUCE’s CMake with the Xcode generator (recommended on macOS):
  - From HungryGhostLimiter/ (project dir)
    - Configure (build extras):
      cmake -S JUCE -B build/juce -G Xcode -DJUCE_BUILD_EXTRAS=ON
    - Build Projucer + AudioPluginHost (Release):
      cmake --build build/juce --config Release --target Projucer AudioPluginHost
  - The Projucer CLI lives inside the built .app bundle. Find it with:
      PROJUCER=$(find build/juce -type f -name Projucer -path "*/Projucer_artefacts/*/Projucer.app/Contents/MacOS/*" -print -quit); echo "$PROJUCER"

Generate exporter projects (Xcode) with Projucer
- The repo does not ship exporter projects. Generate them from the .jucer:
  - Run (from HungryGhostLimiter/):
      "$PROJUCER" --resave HungryGhostLimiter.jucer
  - This will create Builds/MacOSX/*.xcodeproj (and other exporters if configured in the .jucer)

Build the plugin with xcodebuild
- Discover the generated Xcode project:
    XCODEPROJ=$(find Builds -maxdepth 3 -type d -name "*.xcodeproj" -print -quit); echo "$XCODEPROJ"
- List schemes/targets so you know what to build:
    xcodebuild -list -project "$XCODEPROJ"
- Build all targets (Debug):
    xcodebuild -project "$XCODEPROJ" -configuration Debug -derivedDataPath build/xcode -alltargets
- Build all targets (Release):
    xcodebuild -project "$XCODEPROJ" -configuration Release -derivedDataPath build/xcode -alltargets
- Build a single scheme (replace with an actual scheme from -list, e.g., a VST3 or Standalone scheme):
    SCHEME="HungryGhostLimiter - VST3"
    xcodebuild -project "$XCODEPROJ" -scheme "$SCHEME" -configuration Release -derivedDataPath build/xcode
- Find built artefacts (VST3/component/app) under DerivedData:
    find build/xcode -type d -name "*.vst3" -or -name "*.component" -or -name "*.app"

Install and run (local macOS)
- VST3 install (user):
    mkdir -p ~/Library/Audio/Plug-Ins/VST3
    VST3=$(find build/xcode -type d -name "*.vst3" -print -quit)
    [ -n "$VST3" ] && cp -R "$VST3" ~/Library/Audio/Plug-Ins/VST3/
- Audio Unit (AU) path (may require sudo for system-wide installs):
    /Library/Audio/Plug-Ins/Components
- Standalone app (if enabled in the .jucer): locate the .app in build/xcode and open it:
    APP=$(find build/xcode -type d -name "*.app" -print -quit)
    [ -n "$APP" ] && open "$APP"

Test in JUCE AudioPluginHost (built above)
- Launch the host:
    HOST_APP=$(find build/juce -type d -name "AudioPluginHost.app" -path "*/AudioPluginHost_artefacts/*/Release/*" -print -quit)
    open "$HOST_APP"
- In the host, rescan your plugin folders (e.g., ~/Library/Audio/Plug-Ins/VST3) and insert Hungry Ghost Limiter to test.

Notes on linting and tests
- Lint: No repo-level clang-format/clang-tidy configuration for plugin sources is provided. JUCE ships a .clang-tidy for its own code (HungryGhostLimiter/JUCE/.clang-tidy), but plugin source linting isn’t configured.
- Tests: No automated test suite is present; there are no unit/CI test commands to run.

High-level architecture (how this plugin is structured)
- Core JUCE plugin pattern
  - Audio processor: HungryGhostLimiter/Source/PluginProcessor.*
    - Implements prepareToPlay/processBlock/releaseResources
    - Manages parameters via AudioProcessorValueTreeState (IDs include thresholdL, thresholdR, thresholdLink, outCeiling, release, lookAheadMs, scHpf, safetyClip as described in README.md)
    - DSP pipeline (per README): oversampled true-peak detection/limiting, look-ahead with sliding-window maximum, log-domain release smoothing, stereo max-linking, optional safety soft-clip
    - Likely uses juce::dsp utilities (delay/oversampling/filters)
  - Editor/UI: HungryGhostLimiter/Source/PluginEditor.*
    - Composes custom controls and meters, attaches them to APVTS parameters
  - UI components: HungryGhostLimiter/Source/UI/**
    - Controls.*: knobs/sliders and parameter attachments
    - Threshold.*: left/right threshold UI with link behavior
    - Meter.*: gain reduction or level metering components
  - Styling: HungryGhostLimiter/Source/styling/LookAndFeels.*
    - Custom LookAndFeel and font integration
  - Assets: assets/** (fonts/logo embedded via Projucer-generated BinaryData in JuceLibraryCode)
- Auto-generated layer: HungryGhostLimiter/JuceLibraryCode/**
  - Generated by Projucer; includes compile glue for different plugin formats
  - Do not hand-edit; regenerate by resaving the .jucer
- Vendored framework: HungryGhostLimiter/JUCE/** (JUCE 8)
  - Provides modules (juce_*), CMake toolchain, and extras (Projucer, AudioPluginHost)

Gotchas and tips specific to this repo
- You must generate exporter projects from the .jucer before you can build the plugin; none are checked in.
- If xcodebuild can’t find a scheme after resave, open the generated .xcodeproj in Xcode once to let it index, or pass -alltargets.
- If CMake defaults to Ninja and fails (no Ninja installed), force the Xcode generator: -G Xcode
- True-peak limiting depends on oversampling; ensure Release builds (with optimizations) when evaluating performance/quality.

Key references
- Project README: README.md (design/parameters/signal flow)
- JUCE CMake entry: HungryGhostLimiter/JUCE/CMakeLists.txt
- Projucer project: HungryGhostLimiter/HungryGhostLimiter.jucer
- Auto-generated note: HungryGhostLimiter/JuceLibraryCode/ReadMe.txt

