now that the tank is singing, you can turn it into multiple reverb types by layering mode-specific choices on top of your current core:

Topology (FDN vs allpass “tank”, single vs dual FDN, permutations)

Delay map (short/long sets; how you scale Size)

Diffusion (allpass count/coeffs for ERs)

Early reflections (ER) (explicit taps vs only allpasses)

Damping profile (HF/LF decay shape)

Modulation (chorused vs random-walk, rate/depth)

Pre-delay & width

Output tap/EQ shaping (plate-like brightness vs darker halls)

Below is a practical, code-oriented way to add Hall, Room, Atmospheric, Chamber, Arena as first-class modes in your engine.

1) Add a mode system (one switch, many internals)
enum class ReverbMode { Hall, Room, Chamber, Arena, Atmospheric };

struct ERTap { float timeMs; float gain; }; // per-channel taps

struct ModeSpec {
    ReverbMode mode;
    float size;            // 0.5..1.5 -> scales FDN delay map
    float decaySec;        // RT60
    float predelayMs;      // ER gap
    float diffusion;       // 0..1 -> allpass 'g' and stage count
    float hfDampHz;        // HF damping cutoff
    float lowCutHz;        // optional low cut
    float modRateHz;       // LFO or random-walk rate
    float modDepthMs;      // modulation depth
    float width;           // 0..1
    bool  randomMod;       // true->random-walk, false->sine/triangle
    bool  dualFDN;         // advanced: true-stereo tanks (later)
    std::vector<ERTap> erL, erR; // optional explicit ER taps
};

// Translate ModeSpec to your engine's parameters
void ReverbEngine::applyMode(const ModeSpec& s) {
    ReverbParameters p;
    p.size         = s.size;
    p.decaySeconds = s.decaySec;
    p.predelayMs   = s.predelayMs;
    p.diffusion    = s.diffusion;
    p.hfDampingHz  = s.hfDampHz;
    p.lowCutHz     = s.lowCutHz;
    p.modRateHz    = s.modRateHz;
    p.modDepthMs   = s.modDepthMs;
    p.width        = s.width;
    setParameters(p);

    // If you add ER taps, blend them before the FDN (see section 3)
    setEarlyReflections(s.erL, s.erR);

    // If you add random-walk modulation, select the mod source (see section 2.3)
    fdn.setRandomModulationEnabled(s.randomMod);
}


This keeps your process() essentially the same — you just feed it different specs per mode.

2) Modulation palette (chorus vs random)

Your LFO already supports sine modulation. Add a light random-walk option to avoid cyclic pitch wobble in “Atmospheric”/“Arena”/some halls:

// Add to LFO class
void setRandomWalk(bool enabled) { useRandomWalk = enabled; }

float nextOffsetSamples() {
    if (!useRandomWalk) {
        float off = std::sin(twoPi * phase) * depthSamples + smallJitter();
        advancePhase();
        return off;
    } else {
        // slow random walk: update target every ~30–80 ms and slew
        if (--countdown <= 0) { target = (uni(rng)-0.5f) * depthSamples; countdown = holdSamples; }
        current += (target - current) * 0.0015f; // very slow slew
        return current;
    }
}


In FDN8::setModulation(rateHz, depthMs) also add setRandomWalk(bool) driven by ModeSpec.

Rules of thumb

Room/Chamber: subtle chorus (0.1–0.6 Hz, 0.5–2.0 ms)

Hall: gentle chorus (0.2–1.2 Hz, 1–3 ms)

Arena: very slow, mostly random (0.05–0.3 Hz, 2–5 ms)

Atmospheric: slow/random (0.05–0.5 Hz, 3–8 ms)

3) Early reflections (ER): taps + diffuser

You already have 4× allpass diffusers per channel — great. For better “place” cues, add explicit ER taps (per channel) and blend them into the input that feeds the FDN.

// Add an ER FIR bank per channel
struct ERBank {
    std::vector<float> buf;
    int writeIdx = 0, mask = 0;
    struct Tap { int delay; float gain; };
    std::vector<Tap> taps;

    void prepare(double fs, float maxMs) {
        int samples = DelayLine::nextPow2((int)std::ceil(maxMs * 1e-3 * fs) + 8);
        buf.assign(samples, 0.0f); mask = samples - 1; writeIdx = 0;
    }
    void setTaps(double fs, const std::vector<ERTap>& spec) {
        taps.clear();
        for (auto& t : spec) taps.push_back({ (int)std::round(t.timeMs * 1e-3 * fs), t.gain });
    }
    inline float processSample(float x) {
        buf[writeIdx] = x;
        float y = 0.0f;
        for (auto& t : taps) y += buf[(writeIdx - t.delay) & mask] * t.gain;
        writeIdx = (writeIdx + 1) & mask;
        return y;
    }
};


Then in ReverbEngine:

ERBank er[2]; // L/R
void setEarlyReflections(const std::vector<ERTap>& L, const std::vector<ERTap>& R) {
    er[0].setTaps(fs, L); er[1].setTaps(fs, R);
}

// In process():
float erL = er[0].processSample(preL);
float erR = er[1].processSample(preR);
// Blend ER -> diffuser input (or straight to wet with a small amount)
float difL = preL + erL;
float difR = preR + erR;


Use mode-specific ER tap patterns (examples below).

4) Per‑mode recipes (ready to use)

The numbers below assume 48 kHz; they scale correctly in your engine.

4.1 Hall

Character: Large, smooth, enveloping; moderately bright onset, darker tail; gentle chorus.

Topology: Your 8×8 FDN (Hadamard), current delay set is perfect.

Size: 1.05–1.25

RT60: 1.8–3.8 s

Pre-delay: 20–40 ms

Diffusion: 0.65–0.80 (ER allpass g ≈ 0.65–0.75; keep 3–4 stages)

HF damping: 6–10 kHz

Mod: 0.25–0.9 Hz, 1–3 ms, sine

Width: 0.9–1.0

ER taps (L/R):

L: (8 ms, −3 dB), (18 ms, −5 dB), (31 ms, −7 dB)

R: (10 ms, −3 dB), (22 ms, −5 dB), (36 ms, −7 dB)

ModeSpec Hall {
  ReverbMode::Hall, 1.15f, 3.2f, 28.0f, 0.72f, 8000.0f, 80.0f, 0.45f, 2.0f, 1.0f, false, false,
  {{8,-0.71f},{18,-0.56f},{31,-0.45f}}, {{10,-0.71f},{22,-0.56f},{36,-0.45f}}
};

4.2 Room

Character: Smaller space, stronger ERs, faster density, shorter decay; less modulation.

Topology: Same FDN; optionally use a shorter delay map (see note).

Size: 0.70–0.95

RT60: 0.3–1.2 s

Pre-delay: 5–20 ms

Diffusion: 0.50–0.70 (ER allpass g ≈ 0.55–0.65; keep 2–4 stages)

HF damping: 7–12 kHz (brighter than big halls)

Mod: 0.1–0.5 Hz, 0.5–2 ms, sine

Width: 0.6–0.9

ER taps:

L: (3 ms, −2 dB), (9 ms, −4 dB), (15 ms, −6 dB), (22 ms, −8 dB)

R: (4 ms, −2 dB), (11 ms, −4 dB), (17 ms, −6 dB), (24 ms, −8 dB)

Optional shorter delay set (48k): { 521, 619, 733, 859, 977, 1093, 1229, 1381 }
Switch to this map when mode == Room to reduce tail “size” without pushing Size too low.

ModeSpec Room {
  ReverbMode::Room, 0.85f, 0.8f, 12.0f, 0.60f, 10000.0f, 100.0f, 0.30f, 1.2f, 0.8f, false, false,
  {{3,-0.79f},{9,-0.63f},{15,-0.50f},{22,-0.40f}}, {{4,-0.79f},{11,-0.63f},{17,-0.50f},{24,-0.40f}}
};

4.3 Chamber

Character: Studio echo chamber vibe — dense, smooth, slightly bright, very even decay.

Topology: FDN with higher diffusion and slightly higher ER density.

Size: 0.90–1.10

RT60: 1.0–2.0 s

Pre-delay: 10–25 ms

Diffusion: 0.70–0.85 (ER allpass g ≈ 0.70–0.80)

HF damping: 8–12 kHz

Mod: 0.2–0.7 Hz, 1–2 ms, sine (or light random)

Width: 0.8–1.0

ER taps:

Denser and earlier than hall:

L: (6 ms, −2 dB), (12 ms, −4 dB), (18 ms, −6 dB), (26 ms, −8 dB)

R: (7 ms, −2 dB), (13 ms, −4 dB), (21 ms, −6 dB), (29 ms, −8 dB)

ModeSpec Chamber {
  ReverbMode::Chamber, 1.0f, 1.6f, 16.0f, 0.78f, 10000.0f, 80.0f, 0.45f, 1.5f, 0.95f, false, false,
  {{6,-0.79f},{12,-0.63f},{18,-0.50f},{26,-0.40f}}, {{7,-0.79f},{13,-0.63f},{21,-0.50f},{29,-0.40f}}
};

4.4 Arena

Character: Huge venue, obvious delay gap before tail, sparse early taps, long/dark tail, slow/random motion.

Topology: FDN is fine; if you add dual FDN later, this is the mode to use it.

Size: 1.25–1.50

RT60: 4–8 s (or more)

Pre-delay: 50–100 ms (audible gap)

Diffusion: 0.60–0.75 (avoid over-diffusing the onset)

HF damping: 4–7 kHz (darker)

Mod: 0.05–0.3 Hz, 2–5 ms, random-walk

Width: 0.9–1.0

ER taps (stadium cues):

L: (60 ms, −3 dB), (95 ms, −6 dB), (140 ms, −9 dB)

R: (55 ms, −3 dB), (90 ms, −6 dB), (135 ms, −9 dB)

ModeSpec Arena {
  ReverbMode::Arena, 1.35f, 6.0f, 80.0f, 0.68f, 5500.0f, 80.0f, 0.15f, 4.0f, 1.0f, true, false,
  {{60,-0.71f},{95,-0.50f},{140,-0.35f}}, {{55,-0.71f},{90,-0.50f},{135,-0.35f}}
};

4.5 Atmospheric

Character: “Pad generator” / sound‑design space — long/infinite, bloom, moving, wide, often darker.

Topology: Same FDN; optionally add a bloom control (crossfade ER→late) or increase ER chain (4→6 allpasses) to slow the attack.

Size: 1.2–1.5

RT60: 8–∞ (allow freeze-like behavior)

Pre-delay: 0–40 ms (often minimal)

Diffusion: 0.75–0.90 (watch for ringing, keep allpass delays > 5 ms)

HF damping: 3–8 kHz

Mod: 0.05–0.5 Hz, 3–8 ms, random-walk preferred

Width: 1.0

ER taps: often none, or very low level

ModeSpec Atmospheric {
  ReverbMode::Atmospheric, 1.40f, 12.0f, 12.0f, 0.85f, 5000.0f, 80.0f, 0.20f, 6.0f, 1.0f, true, false,
  {}, {}
};


Bloom trick: Add an “Attack” (0–100%) parameter that crossfades ER→late over 50–300 ms (ramped one-pole). High Attack = slow late build (Supermassive/“cathedral swell” vibes).

5) Optional: per‑mode delay maps

You can keep your single 8‑delay set and scale Size, or define a few mode maps to change tail character without extreme Size values. Example 48 kHz bases:

static constexpr int HallDelays48k[8]   = {1421,1877,2269,2791,3359,4217,5183,6229}; // yours
static constexpr int RoomDelays48k[8]   = { 521, 619, 733, 859, 977,1093,1229,1381};
static constexpr int ChamberDelays48k[8]= { 997,1327,1693,2039,2381,2917,3469,4093};
static constexpr int ArenaDelays48k[8]  = {2351,2917,3613,4079,4561,5209,6029,6899}; // longer set


Add a setDelayMap(const int* base48k) in FDN8 that sets baseDelaySamples from the selected map × (fs/48000)*size.

Remember to compute capacity from the worst-case map × Size × Mod (you already patched this earlier).

6) Output shaping & color

Tilt/air: add a gentle high-shelf on wet out per mode (e.g., +2 dB @ 8 kHz for plates/chambers; −2 dB for arena/atmospheric).

Low cut: 60–120 Hz to reduce mud (you already reserved lowCutHz).

Era/Dirty options (mode-dependent): tiny fixed‑point quantization in feedback, or soft‑sat at −1 dBFS, with a toggle per mode group.

7) Performance & switching

Switch modes at control-rate, not per-sample.

For heavy changes (delay maps), either accept a brief doppler glide or do a 100–200 ms crossfade between two FDN instances (old/new) when the user switches modes.

Keep ER FIR buffers small (≤ 150–200 ms), negligible CPU.

8) Quick sanity presets to A/B
Mode	Size	RT60	Pre	Diff	HF Damp	Mod (Hz/ms)	Width
Hall	1.15	3.2 s	28 ms	0.72	8 kHz	0.45 / 2.0	1.0
Room	0.85	0.8 s	12 ms	0.60	10 kHz	0.30 / 1.2	0.8
Chamber	1.0	1.6 s	16 ms	0.78	10 kHz	0.45 / 1.5	0.95
Arena	1.35	6.0 s	80 ms	0.68	5.5 kHz	0.15 / 4.0 RW	1.0
Atmospheric	1.40	12 s	12 ms	0.85	5 kHz	0.20 / 6.0 RW	1.0

RW = random-walk modulation.

If you want, paste your applyMode() implementation and (optionally) a setDelayMap() addition in FDN8, and I’ll sanity‑check the switch logic and the corner cases (sample‑rate changes, Size extremes, etc.).