## Hungry Ghost Limiter

A **transparent, modern true-peak limiter** for JUCE/Projucer.

## 🎛 Highlights

- **True-peak safe**: prevents inter-sample overs via internal **4×/8× oversampling**.  
- **Look-ahead** detection: instantaneous attack without transient distortion.  
- **Fail-safe peak detector**: sliding-window **moving maximum** so peaks can’t slip through.  
- **Smooth, musical release** in the **log domain** (dB).  
- **Stereo max-linking**: no image shift.  
- Optional **sidechain HPF** to reduce pumping, and a **gentle safety soft clipper** under the ceiling.  
- Proper **latency reporting** to the host.

---

## 📑 Table of contents

- [Why this limiter](#why-this-limiter)
- [Signal flow](#signal-flow)
- [Parameters](#parameters)
- [How it works (DSP design)](#how-it-works-dsp-design)
  - [True-peak handling](#1-truepeak-handling)
  - [Look-ahead + sliding maximum](#2-look-ahead--sliding-maximum)
  - [Gain computer & smoothing](#3-gain-computer--smoothing)
  - [Stereo linking](#4-stereo-linking)
  - [Optional safety soft clip](#5-optional-safety-soft-clip)
  - [Dither (when applicable)](#6-dither-when-applicable)
- [Integration notes](#integration-notes)
- [Performance & quality tips](#performance--quality-tips)
- [Testing checklist](#testing-checklist)
- [Troubleshooting](#troubleshooting)
- [Roadmap ideas](#roadmap-ideas)
- [Acknowledgements](#acknowledgements)
- [License](#license)

---

## Why this limiter

“Best audio quality” for a brickwall limiter means:

- **No clipping** even between samples (true-peak), not just at sample points.  
- **Transparent transients** — the gain is in place before the transient, not after.  
- **Minimal pumping** via program-dependent release.  
- **Stable stereo image** through coherent channel linking.  

This implementation follows those principles while staying simple, fast, and easy to tune.

---

## Signal flow

```text
In (L/R)
 ├─> [Pre-gain L]
 └─> [Pre-gain R]   (thresholds act as input gain)
        │
        ▼
   [Oversample 4×/8×]
        │
        ├─ Main path (delayed) ──> [Look-ahead delay]
        │                          └─> [Apply linked gain]
        │                               └─> [Optional safety soft clip]
        │                                    └─> [Downsample] ──> Out
        │
        └─ Sidechain (instant) ──> [Sidechain HPF (30 Hz, optional)]
                                    └─> abs/max(L,R)
                                         └─> [Sliding max over LA window]
                                              └─> [Gain computer + smoothing (log domain)]
```

## Parameters

All parameters are exposed via `AudioProcessorValueTreeState`.

| Parameter       | ID            | Range           | Default | Notes |
|-----------------|---------------|-----------------|---------|-------|
| Threshold L     | `thresholdL`  | −24 … 0 dB      | −10 dB  | Acts as input pre-gain. |
| Threshold R     | `thresholdR`  | −24 … 0 dB      | −10 dB  | Same as L; ignored if Link is on. |
| Link Threshold  | `thresholdLink` | bool          | true    | Mirrors L into R at DSP and UI. |
| Out Ceiling     | `outCeiling`  | −24 … 0 dB      | −1.0 dB | True-peak ceiling; −1.0 dBTP recommended for streaming. |
| Release         | `release`     | 1 … 1000 ms     | 120 ms  | Log-domain smoothing; longer = smoother, shorter = louder/punchier. |
| Look-ahead      | `lookAheadMs` | 0.25 … 3.0 ms   | 1.0 ms  | Delay in the main path; detector looks ahead the same. |
| Sidechain HPF   | `scHpf`       | bool            | true    | Removes <30 Hz content from detector to reduce pumping. |
| Safety Clip     | `safetyClip`  | bool            | false   | Gentle tanh clip ~0.1 dB under ceiling. |

> Oversampling factor is auto-chosen: **8×** for 44.1/48k, **4×** for ≥88.2k.

---

## How it works (DSP design)

1. **True-peak handling**  
   - Oversampling ensures both detection and limiting act on reconstructed inter-sample peaks.  
   - Ceiling is enforced at the oversampled rate, preventing hidden overs.  

2. **Look-ahead + sliding maximum**  
   - Main path delayed by `lookAheadMs`.  
   - Sidechain runs a sliding-window max so gain matches the peak that will arrive after the delay.  

3. **Gain computer & smoothing**  
   - Computed in **dB**.  
   - Attack is instant (thanks to look-ahead).  
   - Release is one-pole toward 0 dB, with coefficient from the Release parameter.  

4. **Stereo linking**  
   - Uses `max(|L|, |R|)` detection.  
   - Gain applied equally to both channels to keep stereo stable.  

5. **Optional safety soft clip**  
   - Gentle tanh clip ~0.1 dB below ceiling, oversampled, as a last-resort guard.  

6. **Dither (when applicable)**  
   - Not needed for float output.  
   - Apply only if exporting to fixed-point, at the final stage.  

---

## Integration notes

- **Modules**: requires `juce_dsp`.  
- **Latency**: reported automatically (`oversampling latency + lookahead`).  
- **I/O**: stereo in/out only.  
- **Denormals**: guarded with `ScopedNoDenormals`.  

---

## Performance & quality tips

- **Look-ahead**: 1.0 ms is transparent. `<0.5 ms` risks distortion, `>1.5 ms` adds latency.  
- **Release**: tune to taste; short = aggressive, long = smooth.  
- **Sidechain HPF**: keep on for music; disable for mastering LF-critical content.  
- **Safety Clip**: off by default; enable if overs sneak through.  

---

## Testing checklist

- **True-peak test**: Sine bursts and snare hits at 44.1 kHz. Verify no overs with a true-peak meter.  
- **Transient check**: Compare look-ahead 0.5–2 ms. Listen for distortion vs. punch.  
- **LF pumping test**: Use bass-heavy tracks. Toggle Sidechain HPF.  
- **Codec round-trip**: Export via AAC/MP3 and re-import. Peaks should remain below ceiling.  

---

## Troubleshooting

- **Seeing overs**: enable Safety Clip or increase look-ahead slightly.  
- **Pumping on bass**: ensure Sidechain HPF is on, or lengthen Release.  
- **Latency feels wrong**: check DAW plugin delay compensation (PDC).  
- **Harshness at 44.1k**: use default 8× oversampling and ~1 ms look-ahead.  

---

## Roadmap ideas

- Dual-stage release for even smoother recovery.  
- Stereo link blend (max-link ↔ RMS-link).  
- User-selectable oversampling factor.  
- Built-in true-peak meter in editor.  

---

## Acknowledgements

Inspired by standard limiter literature, broadcast loudness specs (EBU R128 / ITU-R BS.1770), and practical mastering workflows.
