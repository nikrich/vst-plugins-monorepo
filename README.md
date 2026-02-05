# Hungry Ghost Proprietary VSTs

This monorepo contains JUCE-based audio plugins developed by Hungry Ghost.

[![CI](https://github.com/nikrich/vst-plugins-monorepo/actions/workflows/ci.yml/badge.svg)](https://github.com/nikrich/vst-plugins-monorepo/actions/workflows/ci.yml)
[![Release](https://github.com/nikrich/vst-plugins-monorepo/actions/workflows/release.yml/badge.svg)](https://github.com/nikrich/vst-plugins-monorepo/actions/workflows/release.yml)

## Plugins

### Astral Halls (Reverb)
Astral Halls is a modern reverb built on an FDN-based architecture designed for lush, dense, and stable tails with controllable tone and width.

- Core: 8-line Feedback Delay Network (FDN8) with a scaled Hadamard mixer
- Front end: per‑channel predelay and multi‑stage allpass diffusion for smooth early energy
- Late field: per-line HF damping, subtle modulation for decorrelation, and width control (M/S)
- Post: low/high cut EQ and smoothed wet/dry mix; optional freeze for infinite sustain

For a deeper dive into the architecture, parameters, and roadmap, see docs/readme/astral-halls.md.

### Spectral Limiter (Limiter)
A transparent, modern true‑peak limiter focused on punchy, artifact‑free loudness.

- True‑peak safe via internal oversampling (4×/8× depending on sample rate)
- Look‑ahead with a sliding‑window moving maximum detector so peaks can’t slip through
- Musical, log‑domain release smoothing; stereo max‑linking to preserve imaging
- Optional sidechain HPF to reduce pumping and a gentle safety soft clip under the ceiling
- Proper latency reporting to the host

For full signal flow, parameters, and integration notes, see docs/readme/spectral-limiter.md.

### Multiband Limiter
A frequency-aware, mastering-grade multiband limiter extending true-peak technology with per-band control.

- Complementary LR4 crossover for smooth frequency splits
- Independent limiting on each band: threshold, release, makeup gain
- Flexible architecture ready for future N-band expansion (M1 ships with 2 bands)
- Optional external sidechain for creative or corrective ducking
- Oversampling support (1×, 2×, 4×) for true-peak safety
- Transparent limiting to prevent cross-band pumping

For architecture details, parameters, and integration notes, see docs/readme/hungry-ghost-multiband-limiter.md.

## Documentation
- Astral Halls detailed README: docs/readme/astral-halls.md
- Spectral Limiter detailed README: docs/readme/spectral-limiter.md
- Multiband Limiter detailed README: docs/readme/hungry-ghost-multiband-limiter.md
