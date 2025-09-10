cmake_minimum_required(VERSION 3.15)

project(MyPlugin VERSION 0.0.1)

# Add JUCE (assuming JUCE is cloned as a submodule under ./JUCE)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(JUCE)

juce_add_plugin(MyPlugin
    COMPANY_NAME "MyCompany"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE

    # Plugin formats: VST3, AU, Standalone, etc
    FORMATS VST3

    PRODUCT_NAME "My Plugin"
)

# Add source files (adjust to your project)
target_sources(MyPlugin PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(MyPlugin
    PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
