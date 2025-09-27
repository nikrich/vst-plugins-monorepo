# Hungry Ghost — Multiband Compressor (JUCE)

**Codename:** `Seance Multiband Compressor`
**Owner:** Hungry Ghost (VSTs)
**Formats:** VST3
**OS:** macOS (Intel/Apple Silicon), Windows 10+
**Sample Rates:** 44.1–192 kHz
**Build:** CMake + JUCE (LTS)
**Latency Reporting:** Yes (linear‑phase + look‑ahead)
**Oversampling:** Optional 2×/4×

---

## 1) Product Goals

* Graph‑first multiband compressor with **drag‑to‑create bands** and **on‑canvas edits**.
* Per‑band **downward** & **upward** compression, **parallel mix**, and **internal/external sidechain**.
* Two crossover modes:

  * **Zero‑latency IIR** (Linkwitz–Riley 4th‑order) for tracking.
  * **Linear‑phase FIR** (windowed‑sinc) for mastering.
* **Look‑ahead**, **program‑dependent release**, **Peak/RMS** detection per band.
* Real‑time spectrum analyzer with **pre/post/GR** overlays, freeze, and delta listen.
* **Phase‑coherent** recombination; stable at extreme settings.

---

## 2) Signal Flow (High‑Level)

```
Input → Oversampling? → Band Splitter (IIR LR4 | FIR linear‑phase)
      → [Band i]: Sidechain (int/ext) → Detector (Peak/RMS) → Envelope
         → Gain Computer (downward/upward, soft knee)
         → Gain Smoother (+look‑ahead alignment)
         → Per‑band Wet/Dry
      → Sum Bands → Output Trim → (optional Dither)
      → Meter taps: Pre/Post/GR/Analyzer
```

**Crossover**

* **IIR:** cascaded Butterworth (2× biquads) to form LR4 HP/LP pairs. Optional tiny all‑pass to micro‑tune phase for perfect recombination.
* **FIR:** matched HP/LP kernels (windowed‑sinc, e.g., Kaiser). Latency `(N−1)/2` samples, reported to host. Rebuild kernels on Fc edits.

**Detectors & Gain**

* **Peak** (abs + one‑pole) and **RMS** (running mean of `x²`; sqrt).
* **Soft knee** (quadratic segment).
* **Program‑dependent release:** blend fast/slow releases by recent crest factor.
* **Upward** compression: lift signals below threshold with ratio `Ru` and ceiling.

**Look‑ahead**

* Circular buffer per band; detector reads future samples by `lookahead_ms`. Align main path with equal delay.

**Summation**

* Maintain unity when GR≈0 dB. Validate with sine sweep/null tests.

---

## 3) Parameters (APVTS Schema)

### Global

* `crossoverMode` = { "IIR-ZeroLatency", "FIR-LinearPhase" }
* `bands` = 1..6 (default 4)
* `oversampling` = { Off, 2x, 4x }
* `lookAheadMs` = 0..20 (default 3)
* `latencyCompensate` = bool
* `meterBallistics` = { Fast, Normal, Slow }

### Per Band `i`

* `freqLo_i`, `freqHi_i`
* `threshold_i` (dB), `ratio_i` (1:1..∞:1), `knee_i` (dB)
* `attackMs_i`, `releaseMs_i`, `pdr_i` (0..1)
* `detector_i` = { Peak, RMS }
* `upwardAmt_i` (0 disables)
* `makeup_i` (dB), `autoMakeup_i` (bool)
* `mix_i` (0..100%)
* `sidechain_i` = { Int, Ext(L), Ext(R), Ext(Mono) }
* `bypass_i`, `solo_i`, `delta_i`

### Analyzer

* `analyzer.pre`, `analyzer.post`, `analyzer.gr`, `analyzer.freeze`

---

## 4) DSP Details

### 4.1 Linkwitz–Riley IIR

* Build LR4 from two cascaded 2nd‑order Butterworth sections at the same Fc.
* Use matched HP/LP pairs; verify magnitude sum ≈ flat and phase alignment across Fc.
* Enforce a guard band between adjacent bands; auto‑nudge Fc to prevent inversion/overlap.

### 4.2 Linear‑Phase FIR Crossover

* Odd‑length symmetric FIR kernels: HP/LP via windowed‑sinc.
* Choose **Kaiser β** by stopband target (e.g., −90 dB).
* Length ≈ `4–8 cycles / transitionWidth`; rebuild on Fc/Q edit; pre‑warm.
* Report latency `(N−1)/2`. Align detector and audio.

### 4.3 Detectors

* **Peak:** `p[n] = max(|x[n]|, αp * p[n−1] + (1−αp)*|x[n]|)`, `αp = exp(−1/(τa*Fs))`.
* **RMS:** `r[n] = sqrt( (1−β)*x[n]^2 + β*r[n−1]^2 )`, with `β = exp(−1/(Tw*Fs))`.
* Use separate attack/release time constants; apply logarithmic domain for curve computation.
* **PDR:** compute crest factor `CF = 20*log10(peak/rms)`; interpolate between `releaseFast` and `releaseSlow`.

### 4.4 Gain Computer (Downward)

Let `L = 20*log10(det + ε)`, threshold `T`, ratio `R`, knee `k` (dB).

* Below `(T−k/2)`: unity.
* Above `(T+k/2)`: `GdB = T + (L−T)/R − L`.
* Within knee: quadratic interpolation.

### 4.5 Upward Compression

For `L < T`: `Δ = (T − L) * (1 − 1/Ru)`; apply make‑up and ceiling protection.

### 4.6 Look‑ahead & Smoothing

* Delay main path by `N_la` samples; detector taps pre‑delayed buffer.
* Smooth control with one‑pole or Holt 2‑stage to minimize zippering.

### 4.7 Auto‑Makeup (Approx.)

* Estimate from average linear GR or use a LUT keyed by ratio/knee to keep perceived loudness consistent.

---

## 5) JUCE Implementation Plan

### 5.1 Project Skeleton

* **Targets:** AudioProcessor + AudioProcessorEditor.
* **Modules:** `juce_audio_processors`, `juce_dsp`, `juce_gui_extra`.
* **State:** `AudioProcessorValueTreeState` (parameter layout + attachments).
* **Sidechain:** enable VST3/AU/AAX sidechain busses; per‑band detector can read from selected SC channel.

### 5.2 Band Splitter

* **IIR:** `juce::dsp::IIR::Filter<float>` biquads composed into LR4; utility that generates HP/LP coefficients from Fc.
* **FIR:** custom overlap‑save or `juce::dsp::Convolution` with prebuilt IRs; when Fc changes, rebuild IRs async, then swap atomically; update reported latency.

### 5.3 Compressor Block (per band)

```cpp
struct HGCompressorBand {
  struct Params {
    float threshold, ratio, knee, attackMs, releaseMs, pdr; 
    float upwardAmt, makeup, mix; bool autoMakeup; 
    int detectorType; // 0:Peak, 1:RMS
  };
  void prepare (double sr, int blockSize);
  void setParams (const Params& p);
  void processBlock (juce::AudioBuffer<float>& band,
                     const juce::AudioBuffer<float>& sidechain);
private:
  // detector states, look‑ahead FIFO, gain smoothing, etc.
};
```

### 5.4 Analyzer & GR Trace

* `juce::dsp::FFT` (2048/4096) on decimated taps; magnitude smoothing.
* Render **pre**, **post**, **GR** overlays on shared canvas; double‑buffer drawing; capped FPS.

### 5.5 Graph UI (Band Editor)

* Central canvas with spectrum backdrop.
* **Draggable nodes** per band:

  * Horizontal drag: Fc (moves crossover).
  * Vertical drag: threshold.
  * Wheel/Shift: ratio; Alt‑drag: transition tightness.
* Click‑drag on empty canvas to **create** a band; double‑click node to delete.
* Side panel: attack, release, PDR, detector, upward, mix, makeup, solo/delta/bypass.

### 5.6 Parameter Smoothing

* Exponential smoothing for times/frequencies, linear for gains.
* Sample‑accurate ramps when host supports.

---

## 6) Testing & Validation

1. **Crossover Null Tests**

   * Band‑limited noise & swept sine across Fc; target ripple: <0.2 dB (IIR), <0.05 dB (FIR).
2. **Latency Reporting**

   * Host PDC equals FIR group delay + look‑ahead.
3. **GR Accuracy**

   * Tones at stepped levels; measured GR vs theoretical within 0.1 dB.
4. **Program Material**

   * Percussive vs sustained; PDR reduces pumping; no zipper noise on automation.
5. **CPU/Memory**

   * 4 bands, FIR mode, 96 kHz: under 10% on Apple M‑class target; profile FIR & FFT.
6. **Edge Cases**

   * Extreme ratios, knee=0, bands collapsed, missing sidechain—fails safe.

---

## 7) Performance Notes

* SIMD (`FloatVectorOperations`) in detector & GR application.
* Consider detector‑only oversampling if fast attacks at low SR alias.
* Lock‑free FIFOs for analyzer taps.

---

## 8) UX Polish

* Band color coding; tooltips; **Delta Listen** (what is removed).
* Per‑band GR meter (bar + short trail).
* Global **“Musical/Clinical”** toggle to switch default knee/PDR depth.
* Presets: Bus Glue, Drum Split (60/250/3k), Vocal (150/1.5k/6k), Mastering Gentle 3‑band.

---

## 9) Milestones

* **M1:** Engine skeleton with 2‑band IIR + single‑band comp, static params, safe recombination.
* **M2:** N‑bands, full detector/gain computer, PDR, look‑ahead.
* **M3:** Linear‑phase FIR mode + latency reporting.
* **M4:** Graph UI + analyzer + band creation/drag.
* **M5:** Ext sidechain per band, oversampling, GR meters.
* **M6:** QA passes, presets, docs.

---

## 10) References (for internal use)

* Digital Audio Effects (dynamics & detectors).
* Designing Audio Effect Plugins in C++ (parameters, smoothing, multi‑format build).
* The Technology of Computer Music (windowing & sampling fundamentals).
* ValhallaDSP blog (rapid prototyping workflow mindset).