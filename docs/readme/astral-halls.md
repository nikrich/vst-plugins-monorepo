# Implemented Reverb Engine for Astral Halls

## Overview
This document describes the current state of our custom **FDN-based reverb engine**, as implemented in C++ with JUCE. It summarizes the architecture, key DSP modules, and parameter mapping that we have working.

---

## Architecture

### Components
1. **DelayLine**
   - Fractional delay with linear interpolation.
   - Preallocated buffer, power-of-two sized.
   - Supports modulation offsets and sample-accurate reads.

2. **Allpass Diffuser**
   - Unity-gain Schroeder allpass filter (2-multiply form).
   - Used for **input diffusion** and smoothing early reflections.
   - Configurable gain (`g`) and delay length.

3. **One-Pole Lowpass**
   - Implemented with a TPT (topology-preserving transform).
   - Used for **HF damping** in each FDN feedback loop.
   - Stable, sample-rate aware.

4. **LFO**
   - Sine oscillator with optional jitter/randomness.
   - Provides **modulation offsets** for long FDN lines.

5. **FDN8 (Feedback Delay Network)**
   - 8 delay lines with **Hadamard feedback matrix** (scaled 1/√N).
   - Per-line damping filter.
   - Per-line modulation (depth & rate).
   - Input distributed with alternating sign taps.
   - Output mixed to stereo via decorrelated patterns.

6. **ReverbEngine**
   - Wraps all components:
     - Stereo **predelay lines**.
     - 4× allpass diffusers per channel.
     - Shared **FDN8 late reverb**.
     - Post-EQ (low/high cut).
   - Provides smoothed wet/dry mix and width controls.

---

## Parameters (User-Facing)

```cpp
struct ReverbParameters {
    float mixPercent    = 25.0f;   // Wet/dry mix
    float decaySeconds  = 3.0f;    // RT60
    float size          = 1.0f;    // Room size scaling
    float predelayMs    = 20.0f;   // Input predelay
    float diffusion     = 0.75f;   // ER diffusion amount
    float modRateHz     = 0.30f;   // LFO rate
    float modDepthMs    = 1.50f;   // LFO depth
    float hfDampingHz   = 6000.0f; // HF damping cutoff
    float lowCutHz      = 100.0f;  // Reserved for future
    float highCutHz     = 18000.0f;// Post-EQ cutoff
    float width         = 1.0f;    // Stereo spread
    int   seed          = 1337;    // Random seed
    bool  freeze        = false;   // Sustain tails
};
```

---

## Processing Flow

### 1. Input Stage
- Input signal passes through **predelay** (per channel).
- Four **allpass diffusers** per channel provide input diffusion.
- Optional early reflection blending planned.

### 2. FDN Processing
- Mono mix of diffused input is fed into FDN8.
- Each delay line output is:
  - Read with modulation offsets.
  - Mixed via Hadamard matrix.
  - Damped with one-pole LP (feedback only).
  - Combined with input taps and pushed back.

### 3. Stereo Mixing
- Line outputs decorrelated via sign/permutation.
- Normalized by `1/√N`.
- Width control applied (mid/side scaling).

### 4. Post-EQ & Mix
- Wet outputs pass through **low/high cut filters**.
- Small ER blend (15%) ensures audibility of wet.
- Wet/dry mix applied with **smoothed values**.

---

## Fixes and Improvements Applied
- **Mix normalization bug** fixed (using `1/√N` instead of multiple 0.5 scalings).
- **Feedback clamping** prevents runaway (`g < 0.99`).
- **Filter hardening** prevents NaNs (cutoff clamped below Nyquist/2).
- **Capacity sizing** updated to handle max Size × Mod × fs.
- **Damping applied only to feedback** (not input taps).
- **Parameter smoothing** corrected (use `setTargetValue()` + `getNextValue()`).

---

## Current Sound Characteristics
- **Tail**: Smooth, dense, but tunable with Size/Decay.
- **Tone**: HF damping makes it **darker** at high settings.
- **Stereo field**: Wide with width=1.0, controllable mid/side spread.
- **Diffusion**: Input allpasses ensure no discrete echoes.
- **Stability**: Safe under extreme parameter changes.

---

## Next Steps
- Add **early reflection tap patterns** for different modes.
- Implement **frequency-dependent RT60 filters** (Jot/Schlecht method).
- Expand to **12–16 lines** or block-circulant mixers for flatter spectra.
- Introduce **shimmer bus** with pitch shifting in feedback.
- Optional **time-varying matrices** for evolving tails.


# Advanced Reverb Design: Beyond Valhalla

## Introduction
This document summarizes research-backed techniques to push your reverb engine further than Valhalla’s offerings, with a focus on achieving more **body** and a **darker vibe**. It consolidates findings from academic literature, AES/DAFx whitepapers, and psychoacoustic studies.

---

## 1. Frequency-Dependent RT60 (Darker, Natural Tails)

### Key References
- **Jot & Chaigne (AES 1991):** Introduced per-delay attenuation filters for frequency-dependent decay.
- **Schlecht & Habets (DAFx 2017/2019):** Provide optimized mappings for accurate RT60(f) inside FDNs.

### Implementation
- Replace your one-pole LP in each feedback loop with a designed attenuation filter (shelving + LP).
- Fit filters to a **target RT60 curve** (e.g., LF=3.5s, MF=3.2s, HF=1.8s).
- Ensures **dark but full** tails without metallic artifacts.

---

## 2. Echo Density & Mixing Time (Perceived Body)

### Key References
- **Schlecht & Habets (TASLP 2017):** Show how echo density and mixing time predict perceived fullness.

### Implementation
- Tune delay lengths so mixing time lands at **80–150 ms** for lush modes.
- Shorter mixing times (<80 ms) → Room/Chamber.
- Longer mixing times (100–150 ms) → Hall/Arena.

---

## 3. Flatter Late-Field Spectrum

### Key References
- **Anderson et al. (ICMC 2015):** Block-circulant and higher-order FDNs yield spectrally flatter late fields.

### Implementation
- Expand to **N=12–16** lines if CPU allows.
- Use **block-circulant mixing matrices** for efficiency and flatter energy distribution.
- Yields tails with **more body, less coloration**.

---

## 4. Modal Behavior & Coloration

### Key References
- **Schlecht & Habets (2019):** Modal decomposition of FDNs.
- **Abel et al. (WASPAA 2019):** Input/output tap strategies to flatten modal excitation.

### Implementation
- Redesign input/output tap vectors to **narrow excitation distributions**.
- Reduces coloration → tails feel thicker and more neutral.

---

## 5. Time-Varying Matrices (Evolving Body Without Chorus)

### Key References
- **Schlecht & Habets (JASA 2015):** Time-varying feedback matrices morph between unitary states without losing stability.

### Implementation
- Precompute two unitary mixers (Hadamard ↔ Householder).
- Slowly morph (<0.2 Hz) to evolve late reverberation.
- Adds subtle **motion and mass** without chorus wobble.

---

## 6. Absorbent Allpass Sections

### Key References
- **Dattorro (JAES 1997):** Lexicon-style tanks.
- **Dahl & Jot (DAFx 2000):** Absorbent all-pass with frequency-dependent loss.

### Implementation
- Insert **absorbent allpasses** at input.
- Example: short delay (5–15 ms), g≈0.6–0.75, slight HF loss.
- Thickens ERs, smooths brightness, enhances body.

---

## 7. Scattering & Filtered FDNs

### Key References
- **De Sena et al. (2015):** Scattering Delay Networks for physically meaningful absorption.
- **Velvet Feedback Matrix (2018):** Delays inside the feedback matrix improve echo density.

### Implementation
- Introduce **delays in matrix entries** (not just line delays).
- Improves echo density with low CPU cost.

---

## 8. Psychoacoustic Anchors

### Key References
- **Karjalainen & Järveläinen (AES 2001):** Perceptual metrics for “good late reverberance.”

### Implementation
- Target early density ramps + flat late spectrum.
- Darken HF decay relative to LF/MF.
- Users perceive this as **lush, warm, and full**.

---

## Drop-In Tweaks for Your Engine

1. Replace per-line LP with **RT60(f)-matched filters**.  
2. Expand FDN size (N=12–16) or use **block-circulant mixing**.  
3. Tune delay sets to achieve desired **mixing times**.  
4. Adjust tap vectors to flatten **modal excitation**.  
5. Add **absorbent allpass** before FDN for thicker body.  
6. Optionally, add **time-varying matrices** for evolving tails.

---

## Reading Pack

- Schroeder (1962): Early foundations (combs/allpasses).  
- Moorer (1979): Damping and ER insights.  
- Jot & Chaigne (1991): Frequency-dependent RT in FDNs.  
- Dattorro (1997): Lexicon-style allpass tanks.  
- Schlecht & Habets (2015–2019): Density, modal analysis, time-varying matrices.  
- Anderson et al. (2015): Block-circulant & high-order FDNs.  
- De Sena et al. (2015): Scattering Delay Networks.  
- Karjalainen & Järveläinen (2001): Psychoacoustics of late reverberance.

---

## Conclusion
By combining **frequency-dependent RT60 filters**, **optimized delay/matrix design**, and **absorbent early sections**, you can exceed Valhalla’s body and darkness while keeping your algorithm efficient and controllable. This roadmap provides both the **theory (whitepapers)** and **practical DSP tweaks** to guide your next iteration.

