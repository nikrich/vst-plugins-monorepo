#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <Styling/Theme.h>
#include <array>
#include <functional>

namespace ui { namespace controls {

class StemSelector : public juce::Component {
public:
    enum class Stem {
        Vocals,
        Instrumental,
        Bass,
        Drums,
        Guitar,
        Piano,
        Keys,
        Strings,
        Winds,
        MaleVocal,
        FemaleVocal,
        LeadVocal,
        BackVocal,
        RhythmGuitar,
        SoloGuitar,
        AcousticGuitar,
        ElectricGuitar,
        KickDrum,
        SnareDrum,
        Toms,
        HiHat,
        Ride,
        Crash,
        Count
    };

    static constexpr int kNumStems = static_cast<int>(Stem::Count);
    static constexpr int kNumColumns = 4;

    StemSelector()
    {
        for (int i = 0; i < kNumStems; ++i)
        {
            auto& btn = buttons[i];
            btn.setButtonText(getStemLabel(static_cast<Stem>(i)));
            btn.setClickingTogglesState(true);
            btn.onClick = [this, i]() { stemToggled(i); };
            addAndMakeVisible(btn);
        }

        setDefaults();
    }

    void setDefaults()
    {
        for (int i = 0; i < kNumStems; ++i)
            buttons[i].setToggleState(false, juce::dontSendNotification);

        buttons[static_cast<int>(Stem::Vocals)].setToggleState(true, juce::dontSendNotification);
        buttons[static_cast<int>(Stem::Instrumental)].setToggleState(true, juce::dontSendNotification);
    }

    bool isSelected(Stem stem) const
    {
        return buttons[static_cast<int>(stem)].getToggleState();
    }

    void setSelected(Stem stem, bool selected, juce::NotificationType notify = juce::sendNotificationAsync)
    {
        buttons[static_cast<int>(stem)].setToggleState(selected, notify);
    }

    std::vector<Stem> getSelectedStems() const
    {
        std::vector<Stem> result;
        for (int i = 0; i < kNumStems; ++i)
            if (buttons[i].getToggleState())
                result.push_back(static_cast<Stem>(i));
        return result;
    }

    std::function<void(Stem, bool)> onStemChanged;

    void resized() override
    {
        auto area = getLocalBounds();
        const int numRows = (kNumStems + kNumColumns - 1) / kNumColumns;
        const int gapX = 8;
        const int gapY = 8;
        const int cellW = (area.getWidth() - (kNumColumns - 1) * gapX) / kNumColumns;
        const int cellH = (area.getHeight() - (numRows - 1) * gapY) / numRows;

        for (int i = 0; i < kNumStems; ++i)
        {
            int col = i % kNumColumns;
            int row = i / kNumColumns;
            int x = area.getX() + col * (cellW + gapX);
            int y = area.getY() + row * (cellH + gapY);
            buttons[i].setBounds(x, y, cellW, cellH);
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::transparentBlack);
    }

    static juce::String getStemLabel(Stem stem)
    {
        switch (stem)
        {
            case Stem::Vocals:         return "Vocals";
            case Stem::Instrumental:   return "Instrumental";
            case Stem::Bass:           return "Bass";
            case Stem::Drums:          return "Drums";
            case Stem::Guitar:         return "Guitar";
            case Stem::Piano:          return "Piano";
            case Stem::Keys:           return "Keys";
            case Stem::Strings:        return "Strings";
            case Stem::Winds:          return "Winds";
            case Stem::MaleVocal:      return "Male Vocal";
            case Stem::FemaleVocal:    return "Female Vocal";
            case Stem::LeadVocal:      return "Lead Vocal";
            case Stem::BackVocal:      return "Back Vocal";
            case Stem::RhythmGuitar:   return "Rhythm Gtr";
            case Stem::SoloGuitar:     return "Solo Gtr";
            case Stem::AcousticGuitar: return "Acoustic Gtr";
            case Stem::ElectricGuitar: return "Electric Gtr";
            case Stem::KickDrum:       return "Kick";
            case Stem::SnareDrum:      return "Snare";
            case Stem::Toms:           return "Toms";
            case Stem::HiHat:          return "Hi-Hat";
            case Stem::Ride:           return "Ride";
            case Stem::Crash:          return "Crash";
            default:                   return "";
        }
    }

    static juce::String getStemId(Stem stem)
    {
        switch (stem)
        {
            case Stem::Vocals:         return "vocals";
            case Stem::Instrumental:   return "instrumental";
            case Stem::Bass:           return "bass";
            case Stem::Drums:          return "drums";
            case Stem::Guitar:         return "guitar";
            case Stem::Piano:          return "piano";
            case Stem::Keys:           return "keys";
            case Stem::Strings:        return "strings";
            case Stem::Winds:          return "winds";
            case Stem::MaleVocal:      return "male_vocal";
            case Stem::FemaleVocal:    return "female_vocal";
            case Stem::LeadVocal:      return "lead_vocal";
            case Stem::BackVocal:      return "back_vocal";
            case Stem::RhythmGuitar:   return "rhythm_guitar";
            case Stem::SoloGuitar:     return "solo_guitar";
            case Stem::AcousticGuitar: return "acoustic_guitar";
            case Stem::ElectricGuitar: return "electric_guitar";
            case Stem::KickDrum:       return "kick_drum";
            case Stem::SnareDrum:      return "snare_drum";
            case Stem::Toms:           return "toms";
            case Stem::HiHat:          return "hi_hat";
            case Stem::Ride:           return "ride";
            case Stem::Crash:          return "crash";
            default:                   return "";
        }
    }

    void setLookAndFeel(juce::LookAndFeel* lnf)
    {
        for (auto& btn : buttons)
            btn.setLookAndFeel(lnf);
    }

private:
    std::array<juce::ToggleButton, kNumStems> buttons;

    void stemToggled(int index)
    {
        if (onStemChanged)
            onStemChanged(static_cast<Stem>(index), buttons[index].getToggleState());
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemSelector)
};

} } // namespace ui::controls
