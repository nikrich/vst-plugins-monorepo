#pragma once

#include <cstdint>

namespace hgr::dsp {

struct ReverbParameters {
    // User-facing controls
    float mixPercent    = 25.0f;   // 0..100
    float decaySeconds  = 3.0f;    // 0.1..60
    float size          = 1.0f;    // 0.5..1.5
    float predelayMs    = 20.0f;   // 0..200
    float diffusion     = 0.75f;   // 0..1
    float modRateHz     = 0.30f;   // 0.05..3
    float modDepthMs    = 1.50f;   // 0..10
    float hfDampingHz   = 6000.0f; // 1000..16000
    float lowCutHz      = 100.0f;  // 20..300 (reserved)
    float highCutHz     = 18000.0f;// 6k..20k (reserved)
    float width         = 1.0f;    // 0..1
    int   seed          = 1337;    // deterministic phases
    bool  freeze        = false;   // sustain tails
};

} // namespace hgr::dsp

