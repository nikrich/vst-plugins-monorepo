cmake_minimum_required(VERSION 3.22)
project(HungryGhostReverb VERSION 0.1.0 LANGUAGES C CXX)

# Prefer Release for DSP quality/perf
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Universal build example (optional); harmless if ignored by Makefiles
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

# Pull in vendored JUCE (shared across plugins)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)
# Pull in shared CommonUI library (header-only)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../CommonUI ${CMAKE_CURRENT_BINARY_DIR}/CommonUI)

# Collect sources
file(GLOB_RECURSE HGR_SOURCES CONFIGURE_DEPENDS
    Source/*.cpp
    Source/*.h
)

juce_add_plugin(HungryGhostReverb
    COMPANY_NAME "Hungry Ghost"
    BUNDLE_ID com.hungryghost.hgreverb
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3
    PRODUCT_NAME "Hungry Ghost Reverb"
    MODULES juce_dsp
)

# Sources and settings
target_sources(HungryGhostReverb PRIVATE ${HGR_SOURCES})

# Ensure JUCE creates JuceHeader.h for us
juce_generate_juce_header(HungryGhostReverb)

target_compile_features(HungryGhostReverb PRIVATE cxx_std_17)

# Avoid VST2/VST3 automation replacement conflicts when building only VST3
target_compile_definitions(HungryGhostReverb PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)

# Link JUCE modules + CommonUI
# (audio_processors implies core, graphics, gui_basics, etc.)
target_link_libraries(HungryGhostReverb
    PRIVATE
        CommonUI
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_core
)

