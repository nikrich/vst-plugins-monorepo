## Hungry Ghost Multiband Limiter

A **transparent, frequency-aware limiting solution** that extends the Spectral Limiter with intelligent band-specific control for mastering and mixing.

## 🎛 Highlights

- **Multiband architecture**: Splits audio into independent frequency bands with configurable crossover frequencies.
- **Per-band limiting**: Each band has independent threshold, release, and makeup gain controls.
- **Transparent mastering-grade limiting**: Builds on proven single-band true-peak technology.
- **Complementary crossover filtering**: LR4 (4th-order Butterworth) crossover for smooth frequency splits.
- **Flexible band count**: M1 ships with 2 bands; future versions support N bands.
- **Sidechain option**: Route external audio for creative or corrective limiting.
- **Oversampling support**: 1×/2×/4× internal oversampling for true-peak safety.
- **Proper latency reporting** including look-ahead and oversampling latency.

## Development Status

**M1 Release**: 2-band architecture with LR4 crossover, per-band limiting, oversampling, and optional external sidechain.

**Future (M2+)**: N-band support, linear-phase FIR crossovers, integrated EQ, GR metering, and factory presets.

## Architecture & Roadmap

### M1: 2-Band Architecture (Current)
- Fixed 2-band limiter with LR4 Linkwitz-Riley crossover
- Single configurable crossover frequency (default 120 Hz)
- Per-band threshold, release, makeup gain controls
- Optional external sidechain routing
- Flexible oversampling (1×, 2×, 4×)
- Proper latency reporting to DAW

### M2: N-Band Architecture
- Expand BandSplitterIIR to cascaded N-band crossovers
- Generalize processBlock() for N bands
- UI support for 3, 4, or more bands
- Maintain null test compliance (LP + HP = original)

### M3: Linear-Phase FIR Crossover
- Add FIR filter option alongside LR4 IIR
- Sharper transition, zero added latency
- Trade-off: higher CPU usage

### M4: Integrated EQ + Visualization
- Per-band parametric EQ (bell, shelf filters)
- Real-time frequency response graph
- GR meters per band
- Crossover frequency overlay

### M5: Metering & Presets
- LUFS loudness metering
- Factory presets (Mastering Punch, Vocal Clarity, Bass Control, Mix Bus)
- Preset management UI
- A/B comparison tool

## Integration Points

### CMake Build System
- Location: `src/HungryGhostMultibandLimiter/CMakeLists.txt`
- Target: `HungryGhostMultibandLimiter_VST3`
- Modules: juce_dsp, juce_gui_basics, juce_audio_processors
- Tests: `HGMBLTests_DSP` (unit tests for DSP components)

### Plugin Packaging
- Package ID (macOS): `com.hungryghost.multiband.limiter`
- Windows GUID: `{4D5E9F8A-2B1C-4A7D-9F3E-8C5B7D9E1A42}`
- Installer: Included in `packaging/shared/plugins.json`

### CI/CD Workflows
- Trigger: Changes to `src/HungryGhostMultibandLimiter/**`
- Build job: `build-multiband-limiter` in ci.yml
- Release job: Configured in release.yml
- Platforms: macOS 14 (Xcode), Windows (VS 2022)

## Testing Checklist

- [ ] Crossover null test: LP + HP sums to original < −45 dB RMS error
- [ ] Parameter layout validation
- [ ] Basic audio processing without crashes
- [ ] Stereo I/O support (2 in / 2 out)
- [ ] Multiple sample rates (44.1k, 48k, 88.2k, 96k, 192k)
- [ ] Multiple block sizes (64–2048 samples)
- [ ] DAW integration (Reaper, Studio One, Logic Pro, Ableton)
- [ ] Latency compensation (look-ahead + oversampling)
- [ ] Oversampling A/B testing (1×, 2×, 4× factors)
- [ ] Sidechain routing (Internal vs. Ext L/R/Mono)

## License

Copyright © Hungry Ghost. Proprietary. All rights reserved.
