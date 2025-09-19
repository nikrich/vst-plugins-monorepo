cmake_minimum_required(VERSION 3.22)
project(HungryGhostSaturation VERSION 0.1.0 LANGUAGES C CXX)

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
file(GLOB_RECURSE HGS_SOURCES CONFIGURE_DEPENDS
    Source/*.cpp
    Source/*.h
)

juce_add_plugin(HungryGhostSaturation
    COMPANY_NAME "Hungry Ghost"
    BUNDLE_ID com.hungryghost.saturation
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3
    PRODUCT_NAME "Hungry Ghost Saturation"
    MODULES juce_dsp
)

# Sources and settings
target_sources(HungryGhostSaturation PRIVATE ${HGS_SOURCES})

# Embed shared assets (images, fonts) so CommonUI's BinaryData references resolve
file(GLOB_RECURSE HGS_ASSET_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/*.png
    ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/*.jpg
    ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/*.jpeg
    ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/*.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/*.otf
)
# Create a BinaryData target for these assets
juce_add_binary_data(HGSSaturationBinaryData SOURCES ${HGS_ASSET_FILES})

# Ensure JUCE creates JuceHeader.h for us
juce_generate_juce_header(HungryGhostSaturation)

target_compile_features(HungryGhostSaturation PRIVATE cxx_std_17)

# Avoid VST2/VST3 automation replacement conflicts when building only VST3
target_compile_definitions(HungryGhostSaturation PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)

# Link JUCE modules + CommonUI
# (audio_processors implies core, graphics, gui_basics, etc.)
target_link_libraries(HungryGhostSaturation
    PRIVATE
        CommonUI
        HGSSaturationBinaryData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_core
)
