## Hungry Ghost Multiband Limiter

A **transparent, frequency-aware limiting solution** that extends the Spectral Limiter with intelligent band-specific control for mastering and mixing.

## 🎛 Highlights

- **Multiband architecture**: Splits audio into independent frequency bands with configurable crossover frequencies
- **Per-band limiting**: Each band has independent threshold, release, and makeup gain controls
- **Transparent mastering-grade limiting**: Builds on proven single-band true-peak technology
- **Complementary crossover filtering**: LR4 (4th-order Butterworth) crossover for smooth frequency splits
- **Flexible band count**: M1 ships with 2 bands; future versions support N bands
- **Sidechain option**: Route external audio for creative or corrective limiting
- **Oversampling support**: 1×/2×/4× internal oversampling for true-peak safety
- **Proper latency reporting** including look-ahead and oversampling latency

## Development Status

**M1 Release**: 2-band architecture with LR4 crossover, per-band limiting, oversampling, and optional external sidechain.

**Future (M2+)**: N-band support, linear-phase FIR crossovers, integrated EQ, GR metering, and factory presets.

## Integration Points

### CMake Build System
- Location: `src/HungryGhostMultibandLimiter/CMakeLists.txt`
- Plugin target: `HungryGhostMultibandLimiter_VST3`
- Test target: `HGMBLTests_DSP`

### Plugin Packaging
- macOS Package ID: `com.hungryghost.multiband.limiter`
- Windows GUID: `{4D5E9F8A-2B1C-4A7D-9F3E-8C5B7D9E1A42}`

### CI/CD Workflows
- Trigger: Changes to `src/HungryGhostMultibandLimiter/**`
- Build job: `build-multiband-limiter` in ci.yml
- Release job: Configured in release.yml

## License

Copyright © Hungry Ghost. Proprietary. All rights reserved.
