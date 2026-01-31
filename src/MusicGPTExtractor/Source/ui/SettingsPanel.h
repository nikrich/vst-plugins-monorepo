#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "Layout.h"
#include <Styling/Theme.h>

class SettingsPanel : public juce::Component
{
public:
    std::function<void()> onSettingsSaved;

    SettingsPanel()
    {
        setInterceptsMouseClicks(true, true);
        setAlwaysOnTop(true);

        titleLabel.setText("SETTINGS", juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(titleLabel);

        endpointLabel.setText("API Endpoint", juce::dontSendNotification);
        endpointLabel.setJustificationType(juce::Justification::centredLeft);
        endpointLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
        endpointLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
        addAndMakeVisible(endpointLabel);

        endpointInput.setMultiLine(false);
        endpointInput.setTextToShowWhenEmpty("https://api.musicgpt.example/v1/extract", juce::Colours::grey);
        endpointInput.setFont(juce::Font(juce::FontOptions(14.0f)));
        endpointInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF2A2D35));
        endpointInput.setColour(juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha(0.2f));
        endpointInput.setColour(juce::TextEditor::focusedOutlineColourId, Style::theme().accent1);
        endpointInput.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        addAndMakeVisible(endpointInput);

        apiKeyLabel.setText("API Key", juce::dontSendNotification);
        apiKeyLabel.setJustificationType(juce::Justification::centredLeft);
        apiKeyLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
        apiKeyLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
        addAndMakeVisible(apiKeyLabel);

        apiKeyInput.setMultiLine(false);
        apiKeyInput.setTextToShowWhenEmpty("Enter API key...", juce::Colours::grey);
        apiKeyInput.setFont(juce::Font(juce::FontOptions(14.0f)));
        apiKeyInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF2A2D35));
        apiKeyInput.setColour(juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha(0.2f));
        apiKeyInput.setColour(juce::TextEditor::focusedOutlineColourId, Style::theme().accent1);
        apiKeyInput.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        apiKeyInput.setPasswordCharacter(0x2022);  // bullet character for masking
        addAndMakeVisible(apiKeyInput);

        errorLabel.setText("", juce::dontSendNotification);
        errorLabel.setJustificationType(juce::Justification::centred);
        errorLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        errorLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFFF6B6B));
        addAndMakeVisible(errorLabel);

        saveButton.setButtonText("Save");
        saveButton.setColour(juce::TextButton::buttonColourId, Style::theme().accent2);
        saveButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        saveButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        saveButton.onClick = [this]() { saveSettings(); };
        addAndMakeVisible(saveButton);

        closeButton.setButtonText("X");
        closeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        closeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white.withAlpha(0.7f));
        closeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.7f));
        closeButton.onClick = [this]() { setVisible(false); };
        addAndMakeVisible(closeButton);

        loadSettings();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.7f));

        auto card = getCardBounds();
        auto radius = Style::theme().borderRadius + 4.0f;

        juce::DropShadow ds(juce::Colours::black.withAlpha(0.5f), 20, {});
        ds.drawForRectangle(g, card.toNearestInt());

        g.setColour(juce::Colour(0xFF1E2028));
        g.fillRoundedRectangle(card, radius);

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
        area.removeFromTop(16);

        endpointLabel.setBounds(area.removeFromTop(24));
        area.removeFromTop(4);
        endpointInput.setBounds(area.removeFromTop(36));
        area.removeFromTop(12);

        apiKeyLabel.setBounds(area.removeFromTop(24));
        area.removeFromTop(4);
        apiKeyInput.setBounds(area.removeFromTop(36));
        area.removeFromTop(8);

        errorLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(12);

        auto buttonRow = area.removeFromTop(36);
        saveButton.setBounds(buttonRow.withSizeKeepingCentre(100, 32));
    }

    void showError(const juce::String& message)
    {
        errorLabel.setText(message, juce::dontSendNotification);
    }

    void clearError()
    {
        errorLabel.setText("", juce::dontSendNotification);
    }

    juce::String getApiKey() const
    {
        return apiKeyInput.getText().trim();
    }

    juce::String getEndpoint() const
    {
        return endpointInput.getText().trim();
    }

    bool hasValidApiKey() const
    {
        return getApiKey().isNotEmpty();
    }

    static bool checkApiKeyConfigured()
    {
        auto props = getPropertiesFile();
        if (props == nullptr)
            return false;
        return props->getValue("apiKey", "").isNotEmpty();
    }

    static juce::String loadStoredApiKey()
    {
        auto props = getPropertiesFile();
        if (props == nullptr)
            return {};
        return props->getValue("apiKey", "");
    }

    static juce::String loadStoredEndpoint()
    {
        auto props = getPropertiesFile();
        if (props == nullptr)
            return "https://api.musicgpt.example/v1/extract";
        juce::String endpoint = props->getValue("endpoint", "");
        return endpoint.isEmpty() ? "https://api.musicgpt.example/v1/extract" : endpoint;
    }

private:
    juce::Rectangle<float> getCardBounds() const
    {
        const float cardW = 380.0f;
        const float cardH = 320.0f;
        auto area = getLocalBounds().toFloat();
        return juce::Rectangle<float>(cardW, cardH).withCentre(area.getCentre());
    }

    static std::unique_ptr<juce::PropertiesFile> getPropertiesFile()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "MusicGPTExtractor";
        options.folderName = "HungryGhost";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";

        return std::make_unique<juce::PropertiesFile>(options);
    }

    void saveSettings()
    {
        clearError();

        juce::String key = getApiKey();
        if (key.isEmpty())
        {
            showError("API key cannot be empty");
            return;
        }

        auto props = getPropertiesFile();
        if (props != nullptr)
        {
            props->setValue("apiKey", key);
            props->setValue("endpoint", getEndpoint());
            props->saveIfNeeded();
            setVisible(false);
            if (onSettingsSaved)
                onSettingsSaved();
        }
        else
        {
            showError("Failed to save settings");
        }
    }

    void loadSettings()
    {
        auto props = getPropertiesFile();
        if (props != nullptr)
        {
            juce::String storedKey = props->getValue("apiKey", "");
            if (storedKey.isNotEmpty())
                apiKeyInput.setText(storedKey, juce::dontSendNotification);

            juce::String storedEndpoint = props->getValue("endpoint", "");
            if (storedEndpoint.isNotEmpty())
                endpointInput.setText(storedEndpoint, juce::dontSendNotification);
        }
    }

    juce::Label titleLabel;
    juce::Label endpointLabel;
    juce::TextEditor endpointInput;
    juce::Label apiKeyLabel;
    juce::TextEditor apiKeyInput;
    juce::Label errorLabel;
    juce::TextButton saveButton;
    juce::TextButton closeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
