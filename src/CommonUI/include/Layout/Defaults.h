#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

namespace ui { namespace layout {

// Shared UI defaults (header-only) for consistent spacing across plugins.
struct Defaults {
    static constexpr int kPaddingPx              = 8;
    static constexpr int kColGapPx               = 12;
    static constexpr int kBarGapPx               = 6;
    static constexpr int kRowGapPx               = 8;
    static constexpr int kCellMarginPx           = 4;

    static constexpr int kHeaderHeightPx         = 88;
    static constexpr int kTitleRowHeightPx       = 28;
    static constexpr int kChannelLabelRowHeightPx= 36;
    static constexpr int kLargeSliderRowHeightPx = 252;
};

} } // namespace ui::layout

