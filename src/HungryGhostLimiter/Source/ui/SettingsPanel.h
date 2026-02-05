#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "Layout.h"
#include <Styling/Theme.h>
#include <Foundation/Typography.h>

class SettingsPanel : public juce::Component
{
public:
    SettingsPanel()
    {
        setInterceptsMouseClicks(true, true);
        setAlwaysOnTop(true);

        titleLabel.setText("SETTINGS", juce::dontSendNotification);
        ui::foundation::Typography::apply(titleLabel, ui::foundation::Typography::Style::Title);
        addAndMakeVisible(titleLabel);

        themeLabel.setText("Theme", juce::dontSendNotification);
        themeLabel.setJustificationType(juce::Justification::centredLeft);
        themeLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
        themeLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
        addAndMakeVisible(themeLabel);

        themeToggle.setButtonText("Dark / Light");
        themeToggle.setClickingTogglesState(true);
        themeToggle.setColour(juce::TextButton::buttonColourId, Style::theme().accent2);
        themeToggle.setColour(juce::TextButton::buttonOnColourId, Style::theme().accent1);
        themeToggle.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        themeToggle.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        themeToggle.onClick = [this]() { toggleTheme(); };
        addAndMakeVisible(themeToggle);

        apiKeyLabel.setText("API Key", juce::dontSendNotification);
        ui::foundation::Typography::apply(apiKeyLabel,
                                         ui::foundation::Typography::Style::Subtitle,
                                         juce::Colours::white.withAlpha(0.9f),
                                         juce::Justification::centredLeft);
        addAndMakeVisible(apiKeyLabel);

        apiKeyInput.setMultiLine(false);
        apiKeyInput.setReturnKeyStartsNewLine(false);
        apiKeyInput.setScrollbarsShown(false);
        apiKeyInput.setCaretVisible(true);
        apiKeyInput.setTextToShowWhenEmpty("Enter API key...", juce::Colours::grey);
        apiKeyInput.setFont(juce::Font(juce::FontOptions(14.0f)));
        apiKeyInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF2A2D35));
        apiKeyInput.setColour(juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha(0.2f));
        apiKeyInput.setColour(juce::TextEditor::focusedOutlineColourId, Style::theme().accent1);
        apiKeyInput.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        addAndMakeVisible(apiKeyInput);

        errorLabel.setText("", juce::dontSendNotification);
        ui::foundation::Typography::apply(errorLabel,
                                         ui::foundation::Typography::Style::Body,
                                         juce::Colour(0xFFFF6B6B));
        addAndMakeVisible(errorLabel);

        saveButton.setButtonText("Save");
        saveButton.setColour(juce::TextButton::buttonColourId, Style::theme().accent2);
        saveButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        saveButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        saveButton.onClick = [this]() { saveApiKey(); };
        addAndMakeVisible(saveButton);

        closeButton.setButtonText("X");
        closeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        closeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white.withAlpha(0.7f));
        closeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.7f));
        closeButton.onClick = [this]() { setVisible(false); };
        addAndMakeVisible(closeButton);

        loadApiKey();
        loadTheme();
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

        themeLabel.setBounds(area.removeFromTop(24));
        area.removeFromTop(4);
        themeToggle.setBounds(area.removeFromTop(32));
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

    void setOnThemeChanged(std::function<void()> callback)
    {
        onThemeChanged = callback;
    }

private:
    juce::Rectangle<float> getCardBounds() const
    {
        const float cardW = 340.0f;
        const float cardH = 300.0f;
        auto area = getLocalBounds().toFloat();
        return juce::Rectangle<float>(cardW, cardH).withCentre(area.getCentre());
    }

    static std::unique_ptr<juce::PropertiesFile> getPropertiesFile()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "HungryGhostLimiter";
        options.folderName = "HungryGhost";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";

        return std::make_unique<juce::PropertiesFile>(options);
    }

    void saveApiKey()
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
            props->saveIfNeeded();
            setVisible(false);
        }
        else
        {
            showError("Failed to save settings");
        }
    }

    void loadApiKey()
    {
        auto props = getPropertiesFile();
        if (props != nullptr)
        {
            juce::String storedKey = props->getValue("apiKey", "");
            if (storedKey.isNotEmpty())
                apiKeyInput.setText(storedKey, juce::dontSendNotification);
        }
    }

    void toggleTheme()
    {
        auto currentVar = Style::currentVariant();
        auto newVar = (currentVar == Style::Variant::Dark) ? Style::Variant::Light : Style::Variant::Dark;

        Style::setVariant(newVar);
        themeToggle.setToggleState(newVar == Style::Variant::Light, juce::dontSendNotification);

        saveTheme(newVar);

        if (onThemeChanged)
            onThemeChanged();
    }

    void saveTheme(Style::Variant variant)
    {
        auto props = getPropertiesFile();
        if (props != nullptr)
        {
            props->setValue("theme", variant == Style::Variant::Dark ? "dark" : "light");
            props->saveIfNeeded();
        }
    }

    void loadTheme()
    {
        auto props = getPropertiesFile();
        if (props != nullptr)
        {
            juce::String themeStr = props->getValue("theme", "dark");
            auto variant = (themeStr == "light") ? Style::Variant::Light : Style::Variant::Dark;
            Style::setVariant(variant);
            themeToggle.setToggleState(variant == Style::Variant::Light, juce::dontSendNotification);
        }
    }

    juce::Label titleLabel;
    juce::Label themeLabel;
    juce::TextButton themeToggle;
    juce::Label apiKeyLabel;
    juce::TextEditor apiKeyInput;
    juce::Label errorLabel;
    juce::TextButton saveButton;
    juce::TextButton closeButton;

    std::function<void()> onThemeChanged;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
