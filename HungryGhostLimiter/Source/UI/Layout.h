#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

namespace Layout
{
// Overall UI
inline constexpr int kPaddingPx              = 8;   // outer window padding
inline constexpr int kColGapPx               = 12;  // gap between columns
inline constexpr int kRowGapPx               = 8;   // gap between rows (within columns)
inline constexpr int kCellMarginPx           = 4;   // margin around grid items
inline constexpr int kHeaderHeightPx         = 88;  // header (logo + subtitle)

// Fixed column widths
inline constexpr int kColWidthThresholdPx    = 220;
inline constexpr int kColWidthCeilingPx      = 220;
inline constexpr int kColWidthControlPx      = 180;
inline constexpr int kColWidthMeterPx        = 80;

// Third column (controls) row heights
inline constexpr int kReleaseRowHeightPx     = 160;
inline constexpr int kLookAheadRowHeightPx   = 140;
inline constexpr int kTogglesRowHeightPx     = 36;

// Threshold/Ceiling internal rows
inline constexpr int kTitleRowHeightPx       = 28;
inline constexpr int kChannelLabelRowHeightPx= 20;
inline constexpr int kLargeSliderRowHeightPx = 252; // combined L/R sliders row (fits 760x460)
inline constexpr int kLinkRowHeightPx        = 24;

// Meter column
inline constexpr int kMeterLabelHeightPx     = 20;
inline constexpr int kMeterHeightPx          = 320; // fits 760x460 content height

inline constexpr int kTotalColsWidthPx =
    kColWidthThresholdPx + kColWidthCeilingPx + kColWidthControlPx + kColWidthMeterPx
    + 3 * kColGapPx;
}

