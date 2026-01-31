#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

namespace Layout
{
// Window dimensions (600x700)
inline constexpr int kWindowWidth             = 600;
inline constexpr int kWindowHeight            = 700;

// Padding and gaps
inline constexpr int kPaddingPx               = 12;
inline constexpr int kColGapPx                = 12;
inline constexpr int kRowGapPx                = 10;

// Header
inline constexpr int kHeaderHeightPx          = 70;

// Waveform display
inline constexpr int kWaveformMinHeightPx     = 200;

// Stem controls panel (right side)
inline constexpr int kStemControlsWidthPx     = 180;
inline constexpr int kStemRowHeightPx         = 50;

// Transport bar
inline constexpr int kTransportHeightPx       = 50;

// Button sizes
inline constexpr int kButtonHeightPx          = 36;
inline constexpr int kIconButtonSizePx        = 40;

// Slider dimensions
inline constexpr int kSliderHeightPx          = 24;
inline constexpr int kSliderLabelWidthPx      = 60;

}
