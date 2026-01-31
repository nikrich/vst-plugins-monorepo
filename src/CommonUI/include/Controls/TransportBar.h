#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <Styling/Theme.h>

namespace ui { namespace controls {

class TransportBar : public juce::Component {
public:
    static constexpr int kHeight = 50;

    TransportBar()
    {
        playButton.setButtonText(">");
        playButton.setClickingTogglesState(true);
        playButton.onClick = [this] {
            updatePlayButtonText();
            if (onPlayPauseChanged)
                onPlayPauseChanged(playButton.getToggleState());
        };
        addAndMakeVisible(playButton);

        seekSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        seekSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        seekSlider.setRange(0.0, 1.0, 0.001);
        seekSlider.onValueChange = [this] {
            updateTimeDisplay();
            if (onSeekChanged)
                onSeekChanged(seekSlider.getValue());
        };
        addAndMakeVisible(seekSlider);

        timeLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(timeLabel);

        updateTimeDisplay();
        updatePlayButtonText();
    }

    void setPlaying(bool isPlaying)
    {
        playButton.setToggleState(isPlaying, juce::dontSendNotification);
        updatePlayButtonText();
    }

    bool isPlaying() const { return playButton.getToggleState(); }

    void setPosition(double normalizedPosition)
    {
        seekSlider.setValue(normalizedPosition, juce::dontSendNotification);
        updateTimeDisplay();
    }

    double getPosition() const { return seekSlider.getValue(); }

    void setTotalDuration(double seconds)
    {
        totalDurationSeconds = seconds;
        updateTimeDisplay();
    }

    double getTotalDuration() const { return totalDurationSeconds; }

    juce::Slider& getSeekSlider() { return seekSlider; }
    juce::TextButton& getPlayButton() { return playButton; }

    std::function<void(bool)> onPlayPauseChanged;
    std::function<void(double)> onSeekChanged;

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(8, 4);

        auto playArea = bounds.removeFromLeft(40);
        playButton.setBounds(playArea.reduced(0, 4));

        bounds.removeFromLeft(8);

        auto timeArea = bounds.removeFromRight(100);
        timeLabel.setBounds(timeArea);

        bounds.removeFromRight(8);

        seekSlider.setBounds(bounds.reduced(0, 8));
    }

    void paint(juce::Graphics& g) override
    {
        auto& t = Style::theme();
        g.setColour(t.panel);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), t.borderRadius);
    }

private:
    juce::TextButton playButton;
    juce::Slider seekSlider;
    juce::Label timeLabel;
    double totalDurationSeconds = 0.0;

    void updatePlayButtonText()
    {
        playButton.setButtonText(playButton.getToggleState() ? "||" : ">");
    }

    void updateTimeDisplay()
    {
        double currentSeconds = seekSlider.getValue() * totalDurationSeconds;
        timeLabel.setText(formatTime(currentSeconds) + " / " + formatTime(totalDurationSeconds),
                          juce::dontSendNotification);
        auto& t = Style::theme();
        timeLabel.setColour(juce::Label::textColourId, t.text);
    }

    static juce::String formatTime(double seconds)
    {
        int mins = static_cast<int>(seconds) / 60;
        int secs = static_cast<int>(seconds) % 60;
        return juce::String::formatted("%d:%02d", mins, secs);
    }
};

} } // namespace ui::controls
