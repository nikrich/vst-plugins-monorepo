#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "Layout.h"
#include <Styling/Theme.h>

class CreditConfirmDialog : public juce::Component
{
public:
    std::function<void()> onAccept;
    std::function<void()> onCancel;

    CreditConfirmDialog()
    {
        setInterceptsMouseClicks(true, true);
        setAlwaysOnTop(true);

        titleLabel.setText("Confirm Extraction", juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(titleLabel);

        stemsHeaderLabel.setText("Selected Stems:", juce::dontSendNotification);
        stemsHeaderLabel.setJustificationType(juce::Justification::centredLeft);
        stemsHeaderLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
        stemsHeaderLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
        addAndMakeVisible(stemsHeaderLabel);

        stemsListLabel.setText("", juce::dontSendNotification);
        stemsListLabel.setJustificationType(juce::Justification::centredLeft);
        stemsListLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
        stemsListLabel.setColour(juce::Label::textColourId, Style::theme().accent1);
        addAndMakeVisible(stemsListLabel);

        creditsHeaderLabel.setText("Estimated Credits:", juce::dontSendNotification);
        creditsHeaderLabel.setJustificationType(juce::Justification::centredLeft);
        creditsHeaderLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
        creditsHeaderLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
        addAndMakeVisible(creditsHeaderLabel);

        creditsValueLabel.setText("0", juce::dontSendNotification);
        creditsValueLabel.setJustificationType(juce::Justification::centredLeft);
        creditsValueLabel.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
        creditsValueLabel.setColour(juce::Label::textColourId, Style::theme().fillTop);
        addAndMakeVisible(creditsValueLabel);

        extractButton.setButtonText("Extract");
        extractButton.setColour(juce::TextButton::buttonColourId, Style::theme().accent1);
        extractButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF121315));
        extractButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF121315));
        extractButton.onClick = [this]() {
            setVisible(false);
            if (onAccept)
                onAccept();
        };
        addAndMakeVisible(extractButton);

        cancelButton.setButtonText("Cancel");
        cancelButton.setColour(juce::TextButton::buttonColourId, Style::theme().panel);
        cancelButton.setColour(juce::TextButton::textColourOnId, Style::theme().text);
        cancelButton.setColour(juce::TextButton::textColourOffId, Style::theme().text);
        cancelButton.onClick = [this]() {
            setVisible(false);
            if (onCancel)
                onCancel();
        };
        addAndMakeVisible(cancelButton);

        closeButton.setButtonText("X");
        closeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        closeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white.withAlpha(0.7f));
        closeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.7f));
        closeButton.onClick = [this]() {
            setVisible(false);
            if (onCancel)
                onCancel();
        };
        addAndMakeVisible(closeButton);
    }

    void paint(juce::Graphics& g) override
    {
        // Semi-transparent overlay background
        g.fillAll(juce::Colours::black.withAlpha(0.7f));

        auto card = getCardBounds();
        auto radius = Style::theme().borderRadius + 4.0f;

        // Drop shadow
        juce::DropShadow ds(juce::Colours::black.withAlpha(0.5f), 20, {});
        ds.drawForRectangle(g, card.toNearestInt());

        // Card background
        g.setColour(juce::Colour(0xFF1E2028));
        g.fillRoundedRectangle(card, radius);

        // Card border
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.drawRoundedRectangle(card, radius, 1.0f);
    }

    void resized() override
    {
        auto card = getCardBounds();
        auto inner = card.reduced(20.0f);

        closeButton.setBounds(static_cast<int>(card.getRight() - 36),
                              static_cast<int>(card.getY() + 8), 28, 28);

        auto area = inner.toNearestInt();
        titleLabel.setBounds(area.removeFromTop(32));
        area.removeFromTop(20);

        stemsHeaderLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(4);
        stemsListLabel.setBounds(area.removeFromTop(24));
        area.removeFromTop(16);

        creditsHeaderLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(4);
        creditsValueLabel.setBounds(area.removeFromTop(28));
        area.removeFromTop(24);

        auto buttonRow = area.removeFromTop(36);
        auto buttonWidth = 100;
        auto gap = 12;
        auto totalButtonWidth = buttonWidth * 2 + gap;
        auto startX = (buttonRow.getWidth() - totalButtonWidth) / 2;

        cancelButton.setBounds(buttonRow.getX() + startX, buttonRow.getY(), buttonWidth, 32);
        extractButton.setBounds(buttonRow.getX() + startX + buttonWidth + gap, buttonRow.getY(), buttonWidth, 32);
    }

    void setStems(const juce::StringArray& stemNames)
    {
        if (stemNames.isEmpty())
        {
            stemsListLabel.setText("All stems (default)", juce::dontSendNotification);
        }
        else
        {
            stemsListLabel.setText(stemNames.joinIntoString(", "), juce::dontSendNotification);
        }
    }

    void setCredits(float estimate)
    {
        juce::String formatted;
        if (estimate >= 1000.0f)
        {
            formatted = juce::String(estimate / 1000.0f, 1) + "k";
        }
        else
        {
            formatted = juce::String(estimate, estimate == std::floor(estimate) ? 0 : 2);
        }
        creditsValueLabel.setText(formatted, juce::dontSendNotification);
    }

private:
    juce::Rectangle<float> getCardBounds() const
    {
        const float cardW = 380.0f;
        const float cardH = 280.0f;
        auto area = getLocalBounds().toFloat();
        return juce::Rectangle<float>(cardW, cardH).withCentre(area.getCentre());
    }

    juce::Label titleLabel;
    juce::Label stemsHeaderLabel;
    juce::Label stemsListLabel;
    juce::Label creditsHeaderLabel;
    juce::Label creditsValueLabel;
    juce::TextButton extractButton;
    juce::TextButton cancelButton;
    juce::TextButton closeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreditConfirmDialog)
};
