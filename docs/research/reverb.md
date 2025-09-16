# Designing a Valhalla‑Inspired Algorithmic Reverb in JUCE

> **Goal:** Build an “ultimate”, versatile, Valhalla‑style algorithmic reverb for real‑time use in a JUCE plugin.  
> **Design pillars:** lush + dense tails, natural frequency‑dependent decay, time‑variation (modulation), great stereo image, and strong performance characteristics.

---

## Table of Contents

- [1. Introduction & Scope](#1-introduction--scope)
- [2. Reverb Fundamentals](#2-reverb-fundamentals)
  - [2.1 Algorithmic vs Convolution](#21-algorithmic-vs-convolution)
  - [2.2 What “good” sounds like](#22-what-good-sounds-like)
- [3. Classic Algorithms (the toolbox)](#3-classic-algorithms-the-toolbox)
  - [3.1 Schroeder reverberator](#31-schroeder-reverberator)
  - [3.2 Moorer’s extensions](#32-moorers-extensions)
  - [3.3 Feedback Delay Networks (FDN)](#33-feedback-delay-networks-fdn)
  - [3.4 Dattorro / Lexicon “allpass tank”](#34-dattorro--lexicon-allpass-tank)
- [4. What makes Valhalla reverbs tick (inferred)](#4-what-makes-valhalla-reverbs-tick-inferred)
  - [4.1 Modes = distinct algorithms](#41-modes--distinct-algorithms)
  - [4.2 Diffusion & early reflections](#42-diffusion--early-reflections)
  - [4.3 Modulation (chorused & random)](#43-modulation-chorused--random)
  - [4.4 Damping & tone shaping](#44-damping--tone-shaping)
  - [4.5 Nonlinear color & “shimmer”](#45-nonlinear-color--shimmer)
- [5. Architecture Blueprint (recommended)](#5-architecture-blueprint-recommended)
  - [5.1 Overview](#51-overview)
  - [5.2 Diffuser allpass chain](#52-diffuser-allpass-chain)
  - [5.3 FDN core (8×8)](#53-fdn-core-88)
  - [5.4 Modulation strategy](#54-modulation-strategy)
  - [5.5 Damping & multi‑band decay](#55-damping--multi-band-decay)
  - [5.6 Stereo strategy](#56-stereo-strategy)
  - [5.7 Optional shimmer path](#57-optional-shimmer-path)
- [6. Parameters, Ranges & UX](#6-parameters-ranges--ux)
- [7. JUCE Implementation Plan](#7-juce-implementation-plan)
  - [7.1 Class layout](#71-class-layout)
  - [7.2 Process loop pseudocode](#72-process-loop-pseudocode)
  - [7.3 Fractional delay & LFOs](#73-fractional-delay--lfos)
  - [7.4 Parameter smoothing & size morphing](#74-parameter-smoothing--size-morphing)
  - [7.5 Performance tactics](#75-performance-tactics)
  - [7.6 Threading & real‑time safety](#76-threading--real-time-safety)
- [8. Testing & Verification](#8-testing--verification)
- [9. Preset Table (starter set)](#9-preset-table-starter-set)
- [10. Extensions & Future Work](#10-extensions--future-work)
- [11. Further Reading (classic sources)](#11-further-reading-classic-sources)

---

## 1. Introduction & Scope

This document distills proven techniques from classic academic work (Schroeder, Moorer, Gerzon, Stautner & Puckette, Jot) and industry practice (Dattorro, Valhalla‑style designs) into a **concrete, high‑performance reverb blueprint** suitable for a JUCE plugin. It assumes familiarity with C++ and real‑time audio.

The “Valhalla vibe” we target:
- multiple **modes** (hall/room/plate/ambience/nonlinear/“chaotic” flavors),
- **very fast diffusion** and **ever‑increasing echo density**,
- **time variation** (chorused and/or randomized delay modulation),
- **frequency‑dependent decay** (damping), and
- **tasteful coloration** (optional saturation/quantization), plus **shimmer** mode.

---

## 2. Reverb Fundamentals

### 2.1 Algorithmic vs Convolution
- **Convolution** uses measured IRs; realistic but heavy (FFT partitioning, added latency) and less tweakable.
- **Algorithmic** uses recursive delay/filter networks; **real‑time friendly**, and **highly controllable** (size, decay, tone, modulation). This is our focus.

### 2.2 What “good” sounds like
- **Exponential decay** (RT60) without steps or pumping.
- **High echo density** that **builds rapidly** (no ping‑ponging or metallic ringing).
- **Spectrally neutral** late field (no obvious tonal modes), yet optionally **colored** by design.
- **Natural HF damping** (highs die faster than lows).
- **Wide, decorrelated stereo** with strong mono compatibility.
- **Low CPU**, resilient to **denormals**, stable at long decays.

---

## 3. Classic Algorithms (the toolbox)

### 3.1 Schroeder reverberator
- **Parallel combs** (set decay) → **series allpasses** (increase density).
- Foundational, light, but density growth is limited; can sound metallic if not modulated.

### 3.2 Moorer’s extensions
- **Low‑pass damping** inside feedback for realistic HF decay.
- **Efficient 2‑multiply allpass**.
- **Early reflections** as a separate tapped delay cluster.
  
### 3.3 Feedback Delay Networks (FDN)
- **N parallel delays** mixed by a **unitary matrix** (Hadamard/Householder/etc.) and fed back.
- Pros: **rapid density growth**, **stable exponential decay**, **excellent stereo decorrelation**, **precise decay control** (with per‑band loss filters).

### 3.4 Dattorro / Lexicon “allpass tank”
- A **single feedback loop** containing **multiple allpasses** (and filters), with multiple **taps** for outputs.
- Yields smooth, dense tails with characteristic color; a great alternative or complement to FDNs.

---

## 4. What makes Valhalla reverbs tick (inferred)

> We can’t see their source, but public descriptions and listening tests point to these patterns:

### 4.1 Modes = distinct algorithms
- Each **mode** is a different topology/tuning: e.g., *Concert Hall*, *Room*, *Plate*, *Ambience*, *Nonlin*, *Chaotic*, *Cathedral/Palace* flavors.
- Internally: different delay maps, diffusion depth, mod types, filter/era coloration.

### 4.2 Diffusion & early reflections
- Short **allpass chains** or early‑tap clusters to **instantly raise density** before the late reverb.

### 4.3 Modulation (chorused & random)
- **Slow LFOs** on long delays → lush, chorused tail.
- **Randomized delay perturbation** → avoids cyclic pitch wobble, keeps tail “alive” without obvious chorus.

### 4.4 Damping & tone shaping
- **Low‑pass in feedback** → shorter HF RT60, natural roll‑off.
- Optional low‑cut to prevent LF smear; shelving/tilt for tonality.

### 4.5 Nonlinear color & “shimmer”
- **Fixed‑point grit**, **tape‑like saturation**, gated/reverse **nonlinear** envelopes (mode‑dependent).
- **Shimmer**: pitch‑shifter **in the feedback path** (often +12 or +7 semitones) for the rising halo.

---

## 5. Architecture Blueprint (recommended)

### 5.1 Overview

```
Input → [Pre‑delay] → [Allpass Diffuser ×2–4] → [FDN 8×8 core]
                                               ↘ Stereo L/R mixes
```

- Default late reverb: **FDN (N=8)** with a **unitary feedback matrix** and **per‑line damping**.
- Alternative modes can swap core for **allpass tank(s)** or hybrids.

### 5.2 Diffuser allpass chain
- 2–4 **unity‑gain allpasses** (5–20 ms each), coefficient `g_ap ≈ 0.5…0.75`.
- **Diffusion** control maps to `g_ap` and/or number of active stages.
- Beware over‑diffusion → metallic ring; tune per mode.

**Allpass (canonical form):**
```
y[n] = -a * x[n] + x[n-D] + a * y[n-D]      (0 < a < 1)
```

### 5.3 FDN core (8×8)

**Delay set**  
- Choose mutually inharmonic lengths (samples). Example @48 kHz (illustrative):  
  `L = {1421, 1877, 2269, 2791, 3359, 4217, 5183, 6229}`  
- Expose **Size** (scales all `L_i`); re‑derive feedback gain for constant RT60.

**Feedback gain vs RT60**  
Use the longest delay `L_max` for a conservative decay fit:
```
g = 10^(-3 * L_max / (T60 * fs))        // 60 dB decay in T60 seconds
```
Clamp `g` to a safe < 1.0 for stability.

**Feedback matrix**  
- **Hadamard** or **Householder** (unitary).  
- Hadamard is great for speed (±1 coefficients → adds/subs only).  
- Implement as staged butterflies (fewer ops than naïve 8×8 mul).

**Damping (per line)**  
Insert a 1‑pole low‑pass in each feedback path:
```
damped = α * x + (1 - α) * z1
z1 = damped
```
Map **Damping** knob to `α` (or effective cutoff). Optional LF high‑pass.

### 5.4 Modulation strategy

**Why:** Breaks up static combing → **lush** tails without metallic modes.

- **Targets:** Longest FDN delays (and/or some allpasses).
- **Methods:**
  - **LFO chorusing** (sine/triangle, ~0.1–3 Hz), depth a few samples.
  - **Random walk** / filtered noise modulation (non‑cyclic, no audible vibrato).
- **Implementation:** **Fractional delay** reads with linear interpolation; uncorrelated L/R phases.

### 5.5 Damping & multi‑band decay

- Start with a simple **HF damping** (1‑pole LP).  
- Optionally add **two‑band** control (LF/HF RT60 ratios) via shelving within feedback.  
- Keep it light; complexity grows CPU & tuning effort quickly.

### 5.6 Stereo strategy

- **True‑stereo feel** with **two decorrelated FDNs** (L and R) using **permuted delay sets** and uncorrelated modulators.  
- Simpler variant: one FDN with two output mixes (works, but slightly more correlated).  
- Optional **Width**: interpolate between dual‑mono and fully wide output mixes.

### 5.7 Optional shimmer path

- Insert a **pitch shifter** in an **alternate feedback loop**:
  - **Dual‑granular** (two crossfading windows) or an FFT shifter (careful with latency/CPU).
  - Typical shifts: **+12**, **+7** semitones; mix back into the network with **Shimmer Amount**.
- Expect artifacts; **a little noise/saturation** can **mask** and add character.

---

## 6. Parameters, Ranges & UX

| Parameter | Purpose | Typical Range | Notes |
|---|---|---|---|
| **Mix** | Wet/dry blend | 0–100% | Host may manage if used on sends. |
| **Pre‑delay** | ER spacing | 0–100 ms | Simple input FIFO. |
| **Size** | Scales delay map | 50–150% | Re‑derive `g` to hold RT60. |
| **Decay (RT60)** | Tail length | 0.2–30 s (∞ optional) | Map to `g` with longest `L`. |
| **Damping** | HF decay | 0–100% | Maps to LP coefficient/cutoff. |
| **Diffusion** | Early density | 0–100% | Controls allpass coeff/count. |
| **Mod Depth** | Time variation | 0–100% | A few samples max depth. |
| **Mod Rate** | Chorusing speed | 0.05–3 Hz | Hidden or per‑mode default. |
| **Low Cut** | Mud control | 20–300 Hz | Either input or wet path. |
| **High Cut** | Air control | 6–20 kHz | Optional, complements damping. |
| **Width** | Stereo spread | 0–100% | Crossmix of L/R taps. |
| **Shimmer Amt** | Pitch‑feedback | 0–100% | Separate mode/toggle. |

**Mode switch** selects different topologies/tunings: *Hall*, *Room*, *Plate*, *Ambience*, *Nonlin*, *Chaotic*, *Shimmer*…

---

## 7. JUCE Implementation Plan

### 7.1 Class layout

```cpp
struct FDNLine {
    std::vector<float> buf;
    uint32_t writeIdx = 0;
    float zLP = 0.0f;        // damping state
    float lfoPhase = 0.0f;   // modulation phase
    float lfoInc = 0.0f;     // 2π * rate / fs
    float baseDelay = 0.0f;  // samples
    float modDepth = 0.0f;   // samples
};

class ReverbFDN final {
public:
    void prepare(double fs, int maxBlock);
    void reset();
    void setParameters(const Params& p);
    void processBlock(juce::AudioBuffer<float>& io);

private:
    double fs = 48000.0;
    std::array<FDNLine, 8> linesL, linesR; // true-stereo (permuted maps)
    float feedbackG = 0.98f;      // from RT60
    float dampAlpha = 0.9f;       // damping
    // hadamard signs or staged butterflies precomputed
    // diffuser allpasses, predelay, etc.
};
```

- **prepare**: allocate buffers (power‑of‑two sizes), compute delay maps from Size, compute `g` from RT60, set LFO incs.  
- **reset**: zero buffers/states.  
- **setParameters**: update smoothed values (see §7.4).  
- **processBlock**: per‑sample loop (or tiny sub‑blocks) with no allocations/locks.

### 7.2 Process loop pseudocode

```cpp
for each sample n:
  // 1) Pre-delay & diffuser
  x = inputL_mono;              // sum or mid-feed
  x = runAllpassDiffuser(x);

  // 2) FDN tick (Left & Right variants)
  float vL[8], vR[8];    // delay outputs
  // Read with modulation (fractional)
  for i=0..7:
    vL[i] = readFrac(linesL[i]); // linear interp
    vR[i] = readFrac(linesR[i]);

  // 3) Mix by unitary matrix (Hadamard/Householder) and apply feedback gain
  float fbL[8], fbR[8];
  hadamardMix(vL, fbL);
  hadamardMix(vR, fbR);
  for i=0..7:
    fbL[i] *= feedbackG;
    fbR[i] *= feedbackG;

  // 4) Damping and write next states (feedback + new input feed)
  for i=0..7:
    float inL = x * inGainL[i] + fbL[i];
    float inR = x * inGainR[i] + fbR[i];
    // 1-pole LP in feedback path
    linesL[i].zLP = dampAlpha * inL + (1 - dampAlpha) * linesL[i].zLP;
    linesR[i].zLP = dampAlpha * inR + (1 - dampAlpha) * linesR[i].zLP;
    write(linesL[i], linesL[i].zLP);
    write(linesR[i], linesR[i].zLP);

  // 5) Output tap mixes (decorrelated L/R)
  outL = dot(vL, outMixL);
  outR = dot(vR, outMixR);

  // 6) Mix with dry; write to buffer
  yL = dry*xL + wet*outL;
  yR = dry*xR + wet*outR;
```

### 7.3 Fractional delay & LFOs

**Linear interpolation (good enough for chorus‑depth mod):**
```cpp
inline float readFrac(const FDNLine& d) {
    float rIdx = d.writeIdx - (d.baseDelay + d.modDepth * sinf(d.lfoPhase));
    // wrap rIdx to [0, N)
    int i0 = fastWrap((int)floorf(rIdx));
    int i1 = fastWrap(i0 + 1);
    float frac = rIdx - floorf(rIdx);
    return d.buf[i0] * (1.0f - frac) + d.buf[i1] * frac;
}
```
Update `lfoPhase += lfoInc;` with wrap. For **random walk** modulation, increment a target value slowly and low‑pass filter it.

### 7.4 Parameter smoothing & size morphing

- Use `juce::SmoothedValue<float>` for **Decay/RT60**, **Damping**, **Mix**, etc. (~20–100 ms ramps).  
- **Size** changes: two options  
  1) **Glide** delay lengths (introduces brief doppler) – simplest.  
  2) **Dual‑state crossfade** (old vs new delay map) over ~100–300 ms – seamless, costs more RAM/CPU.

### 7.5 Performance tactics

- **No allocations** / locks in audio thread.  
- **Power‑of‑two** buffer sizes → **bitmask wrap** (no branch/mod).  
- **Unroll** fixed‑size loops (8) and **inline** hot paths.  
- Precompute Hadamard **butterfly** structure (add/sub only).  
- Prefer **float**; enable **FTZ/DAZ**; guard **denormals** (`ScopedNoDenormals`).  
- Keep modulation **light**; linear interp is cheap & musical.  
- Consider `juce::FloatVectorOperations` if a hotspot emerges (profile first).

### 7.6 Threading & real‑time safety

- Keep DSP **single‑threaded**; offload only non‑RT work (e.g., recompute delay maps) to background with **lock‑free** handoff.  
- Avoid atomics on hot paths; use **double‑buffered parameter state** swapped per block.  
- No file I/O, logging, or heap ops in `processBlock`.

---

## 8. Testing & Verification

- **Impulse response**: confirm **exponential decay**; density growth; no periodic flutter.  
- **RT60 check**: fit decay slope vs target T60 across settings & sample rates.  
- **Stability**: max Decay/Size, long run; ensure no blow‑ups, no denorm stalls.  
- **Listening**: drums (metallic test), vocals (sibilance + HF damping), pads (mod swirl), mono‑sum compatibility.  
- **CPU profiling**: verify instance headroom (target low single‑digit % per instance on modern CPUs).

---

## 9. Preset Table (starter set)

| Mode | Size | Decay | Damp | Diff | ModDepth | Notes |
|---|---:|---:|---:|---:|---:|---|
| **Concert Hall** | 115% | 3.5 s | 55% | 70% | 35% | Deep chorused tails; warm HF roll‑off. |
| **Bright Hall** | 120% | 4.0 s | 35% | 65% | 30% | Airy, slower HF damping. |
| **Room** | 80% | 1.4 s | 50% | 50% | 20% | Tighter early density; lower diffusion. |
| **Plate** | 100% | 2.0 s | 30% | 85% | 25% | High diffusion; brighter tail. |
| **Ambience** | 70% | 0.7 s | 45% | 75% | 10% | Early‑ref heavy; fast fade. |
| **Nonlin Gate** | 90% | 3.0 s | 50% | 65% | 15% | Apply gated envelope on output. |
| **Chaotic** | 110% | 3.0 s | 60% | 80% | 45% | Random‑walk modulation; optional soft‑sat. |
| **Shimmer +12** | 115% | 6.0 s | 55% | 70% | 30% | Pitch‑feedback path ~15–30%. |

> Tweak per material; presets double as QA targets.

---

## 10. Extensions & Future Work

- **Multi‑band decay** (explicit LF/MF/HF RT60 control).  
- **Allpass tank** modes for Lexicon‑like color; dual‑tank plates.  
- **Pitch modes**: -12/±5/±7, diffusion around shifter to mask artifacts.  
- **Dynamic reverb** (ducking keyed by dry input or sidechain).  
- **Oversampling** for “dirty” modes with nonlinearity.  
- **MIDI sync** of pre‑delay / modulation.  
- **Preset morphing** with safe crossfades.

---

## 11. Further Reading (classic sources)

- **M. R. Schroeder (1962)** – early digital reverberators (parallel combs + series allpasses).  
- **J. A. Moorer (1979)** – “About this Reverberation Business” (damping, early reflections).  
- **M. Gerzon (1971)** – matrixed feedback concepts for diffusion & stereo.  
- **P. Stautner & M. Puckette (1982)** – FDN formalization and practice.  
- **J.-M. Jot (1990s)** – frequency‑dependent decay control in FDNs (IRCAM).  
- **J. Dattorro (1997)** – “Effect Design Part 1/2” (Lexicon‑style tanks, allpass tricks).  
- **J. O. Smith** – *Physical Audio Signal Processing* chapters on reverberation.  
- **Sean Costello (ValhallaDSP Blog)** – design notes on diffusion, modulation, shimmer, color.

---

### Appendix A — RT60 & Feedback Gain

For a single delay of length `L` samples in a loop, the gain `g` to reach −60 dB in `T60` seconds at sample rate `fs` is:
```
g = 10^(-3 * L / (T60 * fs))
```
For an FDN, using `L_max` is conservative; you can refine by energy‑based estimates, but the above is robust and musical.

### Appendix B — Hadamard 8‑point (concept)

Implement as staged “butterflies” to minimize ops (only adds/subs; scale by 1/√8 once). Precompute sign patterns; unroll for speed.

### Appendix C — Denormal Hygiene

Enable FTZ/DAZ (platform flags) and wrap `processBlock` with `juce::ScopedNoDenormals`. A tiny DC‑biased dither (~1e‑12) also prevents stalls on ultra‑long tails.

---

**Author’s note:** This blueprint is intentionally modular. Start with the 8×8 FDN core + diffuser, add modulation & damping, then layer modes (plate, room, hall). Integrate shimmer last (heaviest piece). Tune by ear against references you like (e.g., ValhallaRoom/VintageVerb), then profile and trim.
