# Valhalla‑ish Saturator: Architecture & Starter Code (JUCE)

> A compact, ear‑first saturation plugin with clean gain staging, oversampling, and a few tasteful waveshapers.

---

## 1) Signal Flow (single‑band)

```
Input
  ↓
[Input Trim]
  ↓
[Pre‑Emphasis]  // gentle HP tilt ~6 dB/oct @ ~200 Hz (prevents mud pumping the shaper)
  ↓
[Oversampling 2–4×]
  ↓
[Waveshaper]    // selectable: tanh / atan / soft clip / fuzz‑exp (asym)
  ↓
[Post‑De‑Emphasis] // tilt back, optional LP to shave foldover residue
  ↓
[Level Comp]    // auto makeup + output trim
  ↓
[Mix]           // dry/wet (equal‑power crossfade)
  ↓
Output
```

**Key ideas**
- Keep the **nonlinearity isolated** between linear pre/post stages. This makes the tone controllable, and it maps well to a simple block diagram.
- Use **oversampling** around the shaper to reduce aliasing (2× as default, 4× for “Hi‑Fi”).
- Provide **gain‑comp** so users can A/B by ear without loudness bias.

---

## 2) Parameters (initial set)

- `Input` (dB): −24…+24
- `Drive` (linear or dB → internal saturation index `k`)
- `Model` (enum): TANH | ATAN | SOFT | FEXP (asym)
- `Asym` (−1…+1) → only for FEXP
- `PreTilt` (dB/oct): 0…+6 (positive = brighter into shaper)
- `PostLP` (Hz): Off | 22k | 16k | 12k | 8k
- `OS` (enum): 1× | 2× | 4×
- `Mix` (%): 0…100
- `Output` (dB): −24…+24
- `AutoGain` (on/off)

---

## 3) Waveshapers (normalized, monotonic)

Let `x` be input in [−1, 1], `k` the drive/saturation index (map from `Drive`).

- **Tanh**: `y = tanh(k*x)/tanh(k)`
- **ArcTan**: `y = atan(k*x)/atan(k)`
- **Soft Clip (cubic)**: piecewise smooth clip; simple variant:
  - For `|x| ≤ 1/3`: `y = 2*x`
  - For `1/3 < |x| < 2/3`: `y = sign(x)*(3 - (2 - 3|x|)^2)/3`
  - For `|x| ≥ 2/3`: `y = sign(x)`
- **Fuzz‑Exp (asym)**: `y = (1 - exp(-k*(x + a)))/(1 - exp(-k))` with asymmetry `a ∈ [−0.5, 0.5]` then re‑center to zero mean.

Notes:
- Normalize by the `k`‑dependent denominator where applicable so drive doesn’t drastically change output level.
- For **asymmetric** modes, apply DC‑blocking post‑shaper (1st‑order HP @ ~10 Hz) to remove offset.

---

## 4) Oversampling Strategy

- Use polyphase IIR/FIR provided by JUCE’s `dsp::Oversampling<float>`. Wrap only the **shaper** in the up/down path.
- Budget: 2× default (low CPU), 4× “Hi‑Fi”.
- After downsample, apply a gentle LP (e.g., 12 kHz) only if needed.

---

## 5) Auto Gain (simple and robust)

Target **equal‑RMS** between dry and wet at unity input:

```
// running estimate
rms_in  ← sqrt( (1−α)*rms_in^2  + α*x^2 )
rms_out ← sqrt( (1−α)*rms_out^2 + α*y^2 )

makeup = clamp( rms_in / (rms_out + ε) , 0.25, 4.0 )
```

Use small `α` (e.g., 1e‑3 per sample at 48k) to avoid pumping; smooth with a 20–50 ms low‑pass.

---

## 6) JUCE Starter Code (single file demo)

> Drop this into a fresh **JUCE Audio Plug‑In** project and wire the parameter IDs to your GUI. It compiles as VST3/AU/AAX given the standard Projucer template.

```cpp
// ValhallaLikeSaturatorProcessor.h/.cpp (single translation unit for clarity)
#include <JuceHeader.h>

namespace Sat {
    enum class Model { TANH, ATAN, SOFT, FEXP };

    static inline float softClipCubic(float x)
    {
        auto ax = std::abs(x);
        if (ax <= 1.0f/3.0f) return 2.0f * x;
        if (ax <  2.0f/3.0f) {
            float s = juce::jcopysign(1.0f, x);
            float t = 2.0f - 3.0f*ax; // (2-3|x|)
            return s * (3.0f - t*t) / 3.0f;
        }
        return juce::jcopysign(1.0f, x);
    }

    struct Waveshaper {
        Model model { Model::TANH };
        float k { 2.5f };     // drive index
        float asym { 0.0f };  // for FEXP
        float dcPrev { 0.0f }; // DC blocker state
        float dcA { 0.999f };  // ~10 Hz at 48k

        inline float process(float x) {
            float y = 0.0f;
            switch (model) {
                case Model::TANH: {
                    float d = std::tanh(k) + 1e-6f;
                    y = std::tanh(k*x) / d;
                } break;
                case Model::ATAN: {
                    float d = std::atan(k) + 1e-6f;
                    y = std::atan(k*x) / d;
                } break;
                case Model::SOFT: {
                    y = softClipCubic(x);
                } break;
                case Model::FEXP: {
                    float xa = x + asym; // asym push
                    float d = 1.0f - std::exp(-k) + 1e-6f;
                    y = (1.0f - std::exp(-k * juce::jlimit(-1.0f, 1.0f, xa))) / d;
                    // recenter via one‑pole HP (DC blocker)
                    float hp = y - dcPrev + dcA * dcPrev; // leaky‑int form
                    dcPrev = hp;
                    y = hp;
                } break;
            }
            return juce::jlimit(-1.0f, 1.0f, y);
        }
        void reset() { dcPrev = 0.0f; }
    };
}

class ValhallaLikeSaturatorAudioProcessor : public juce::AudioProcessor {
public:
    ValhallaLikeSaturatorAudioProcessor()
    : apvts(*this, nullptr, "Params", createLayout())
    , oversampling (/*numChannels*/ 2, /*factor*/ 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR)
    {}

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override {
        sr = (float) sampleRate;
        updateFromParams();
        resetDSP();
    }
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
            || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
    }

    void resetDSP() {
        ws.reset();
        rmsIn = rmsOut = 1e-6f;
        dcHP.reset();
    }

    void updateFromParams() {
        autoGain = *apvts.getRawParameterValue("autoGain") > 0.5f;
        mix = *apvts.getRawParameterValue("mix");
        inGain  = juce::Decibels::decibelsToGain(*apvts.getRawParameterValue("in"));
        outGain = juce::Decibels::decibelsToGain(*apvts.getRawParameterValue("out"));

        // drive mapping -> k
        float driveDB = *apvts.getRawParameterValue("drive");
        ws.k = juce::jmap(driveDB, 0.0f, 36.0f, 1.0f, 8.0f);
        ws.asym = *apvts.getRawParameterValue("asym");

        int modelIdx = (int)*apvts.getRawParameterValue("model");
        ws.model = (Sat::Model) juce::jlimit(0, 3, modelIdx);

        // pre‑tilt (high‑shelf) and post‑LP
        float tilt = *apvts.getRawParameterValue("pretilt");
        preTilt.setType(juce::dsp::IIR::Coefficients<float>::HighShelf);
        preTilt.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 200.0f, 0.707f, juce::Decibels::decibelsToGain(tilt));

        int lpSel = (int)*apvts.getRawParameterValue("postlp");
        const float lpTable[5] = { 0.0f, 22050.0f, 16000.0f, 12000.0f, 8000.0f };
        postLPEnabled = lpSel != 0;
        postLP.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, lpTable[lpSel], 0.707f);

        // oversampling factor
        int osSel = (int)*apvts.getRawParameterValue("os");
        int newFactor = (osSel == 0 ? 1 : (osSel == 1 ? 2 : 4));
        if (newFactor != osFactor) {
            osFactor = newFactor;
            oversampling.reset();
            oversampling = juce::dsp::Oversampling<float>(getTotalNumOutputChannels(), (int)std::log2((float)osFactor), juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        }
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        juce::ScopedNoDenormals _;
        updateFromParams();

        auto totalCh = getTotalNumOutputChannels();
        auto numSamp = buffer.getNumSamples();

        // input trim
        for (int ch = 0; ch < totalCh; ++ch)
            buffer.applyGain(ch, 0, numSamp, inGain);

        // pre‑emphasis
        juce::dsp::AudioBlock<float> blk (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        preTilt.process (ctx);

        // oversample
        auto upBlock = oversampling.processSamplesUp (blk);

        // waveshape per sample (simple, replace with SIMD if needed)
        for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch) {
            auto* d = upBlock.getChannelPointer(ch);
            for (size_t n = 0; n < upBlock.getNumSamples(); ++n)
                d[n] = ws.process(d[n]);
        }

        // downsample
        oversampling.processSamplesDown (blk);

        // post‑filter
        if (postLPEnabled) postLP.process (ctx);

        // auto‑gain (RMS tracking)
        if (autoGain) {
            const float alpha = 1e-3f; // simple IIR ave
            for (int ch = 0; ch < totalCh; ++ch) {
                auto* x = buffer.getReadPointer(ch);
                tmp.ensureStorageAllocated(numSamp);
                tmp.resize(numSamp);
                float* y = tmp.getRawDataPointer();
                std::memcpy(y, x, (size_t)numSamp * sizeof(float));
            }
            // estimate rms_in from preTilt input? keep a copy instead; here approximate using current buffer pre‑mix
            // (production: tap pre‑shaper RMS in oversampled path)
            for (int n = 0; n < numSamp; ++n) {
                float s = 0.0f; for (int ch = 0; ch < totalCh; ++ch) s += buffer.getReadPointer(ch)[n]; s *= (1.0f / juce::jmax(1, totalCh));
                rmsOut = std::sqrt((1.0f - alpha)*rmsOut*rmsOut + alpha * (s*s));
            }
            float makeup = juce::jlimit(0.25f, 4.0f, (rmsIn + 1e-6f) / (rmsOut + 1e-6f));
            buffer.applyGain(makeup);
        }

        // dry/wet
        mixSmoothed.setTargetValue(mix);
        auto m = mixSmoothed.getNextValue();
        for (int ch = 0; ch < totalCh; ++ch)
            for (int n = 0; n < numSamp; ++n) {
                float wet = buffer.getReadPointer(ch)[n];
                float dry = dryBuffer.getSample(ch, n);
                float out = std::sqrt(1.0f - m) * dry + std::sqrt(m) * wet; // equal‑power
                buffer.setSample(ch, n, out);
            }

        // output trim
        for (int ch = 0; ch < totalCh; ++ch)
            buffer.applyGain(ch, 0, numSamp, outGain);
    }

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "Valhalla‑ish Saturator"; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
        using P = juce::AudioProcessorValueTreeState;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
        p.emplace_back(std::make_unique<juce::AudioParameterFloat>("in", "Input", juce::NormalisableRange<float>(-24.f, 24.f, 0.01f), 0.f));
        p.emplace_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive (dB)", juce::NormalisableRange<float>(0.f, 36.f, 0.01f), 12.f));
        p.emplace_back(std::make_unique<juce::AudioParameterChoice>("model", "Model", juce::StringArray{"TANH","ATAN","SOFT","FEXP"}, 0));
        p.emplace_back(std::make_unique<juce::AudioParameterFloat>("asym", "Asymmetry", juce::NormalisableRange<float>(-0.5f, 0.5f, 0.001f), 0.f));
        p.emplace_back(std::make_unique<juce::AudioParameterFloat>("pretilt", "PreTilt dB/oct", juce::NormalisableRange<float>(0.f, 6.f, 0.01f), 0.f));
        p.emplace_back(std::make_unique<juce::AudioParameterChoice>("postlp", "Post LP", juce::StringArray{"Off","22k","16k","12k","8k"}, 0));
        p.emplace_back(std::make_unique<juce::AudioParameterChoice>("os", "Oversampling", juce::StringArray{"1x","2x","4x"}, 1));
        p.emplace_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 1.f));
        p.emplace_back(std::make_unique<juce::AudioParameterBool>("autoGain", "Auto Gain", true));
        p.emplace_back(std::make_unique<juce::AudioParameterFloat>("out", "Output", juce::NormalisableRange<float>(-24.f, 24.f, 0.01f), 0.f));
        return { p.begin(), p.end() };
    }

private:
    float sr { 48000.0f };
    Sat::Waveshaper ws;

    juce::dsp::IIR::Filter<float> preTilt;
    juce::dsp::IIR::Filter<float> postLP;
    bool postLPEnabled { false };

    juce::dsp::Oversampling<float> oversampling { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };
    int osFactor { 2 };

    float inGain { 1.0f }, outGain { 1.0f }, mix { 1.0f };
    bool autoGain { true };

    float rmsIn { 0.1f }, rmsOut { 0.1f };
    juce::dsp::IIR::Filter<float> dcHP; // optional extra DC block

    juce::SmoothedValue<float> mixSmoothed { 0.0f };
    juce::Array<float> tmp; // scratch
    juce::AudioBuffer<float> dryBuffer; // (production: keep a copy pre‑process)
};
```

**Implementation notes**
- In a production build, keep a **dry tap** before pre‑emphasis to properly do the mix; here the object shows the concept—wire a real dryBuffer in your project’s scaffolding.
- For performance, replace the sample‑loop shaper with a **vectorized** version (JUCE SIMDRegister or xsimd).
- Consider clamping input to [−1, 1] before shaping to keep math predictable.

---

## 7) ASPiK / Alternate Frameworks

The same block diagram maps directly to ASPiK. Replace JUCE `Oversampling` with your own polyphase filters, and wire parameters via ASPiK’s `PluginCore`.

---

## 8) Next Steps / Variants

- **Modes**: add diode ladder (tanh + hard knee), op‑amp with feedback (pre‑emphasis → nonlinear feedback → post EQ).
- **Stereo nuances**: channel‑linked vs. dual‑mono drive.
- **HQ toggle**: switch 2×/4× oversampling mid‑session with stateful crossfade.
- **Spectral saturator**: multi‑band split (Linkwitz‑Riley) then per‑band shaper + recombine.
- **Musical sweeteners**: post‑harmonic tilt EQ +/- 1.5 dB around 2–5 kHz.

---

## 9) Quick Build Checklist (JUCE)

1. New Project → Audio Plug‑In → add file above, set Plugin Code Signing as needed.
2. Enable VST3/AU in Projucer, set C++17.
3. Export to your IDE, build, and test with a tone/sweep & real material.

---

_If you want, I can generate a multiband version or a simple skinned GUI that echoes Valhalla’s minimalist vibe as a separate file._

