cmake_minimum_required(VERSION 3.22)
project(HungryGhostMultibandCompressor VERSION 0.1.0 LANGUAGES C CXX)

# Prefer Release for DSP quality/perf
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Universal build example (optional); harmless if ignored by Makefiles
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

# Pull in vendored JUCE and shared CommonUI (paths relative to this CMakeLists.txt)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../CommonUI ${CMAKE_CURRENT_BINARY_DIR}/CommonUI)

# Collect sources
file(GLOB_RECURSE HG_MBC_SOURCES CONFIGURE_DEPENDS
    Source/*.cpp
    Source/*.h
)

# Embed shared assets (images, fonts) so CommonUI's BinaryData references resolve (optional for now)
file(GLOB_RECURSE HG_MBC_ASSET_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/*.png
    ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/*.jpg
    ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/*.jpeg
    ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/*.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/*.otf
)
# Create a BinaryData target for these assets
juce_add_binary_data(HGMBCBinaryData SOURCES ${HG_MBC_ASSET_FILES})

# Plugin target
juce_add_plugin(HungryGhostMultibandCompressor
    COMPANY_NAME "Hungry Ghost"
    BUNDLE_ID com.hungryghost.mbc
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3
    PRODUCT_NAME "Hungry Ghost Multiband Compressor"
    MODULES juce_dsp
)

# Sources and settings
juce_generate_juce_header(HungryGhostMultibandCompressor)
# Ensure DSP sources are compiled into the plugin target
file(GLOB_RECURSE HG_MBC_DSP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/dsp/*.cpp
)

target_sources(HungryGhostMultibandCompressor PRIVATE ${HG_MBC_SOURCES} ${HG_MBC_DSP_SOURCES})
target_compile_features(HungryGhostMultibandCompressor PRIVATE cxx_std_17)

# Avoid VST2/VST3 automation replacement conflicts when building only VST3
target_compile_definitions(HungryGhostMultibandCompressor PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)

# Link JUCE modules + CommonUI + BinaryData
# (audio_processors implies core, graphics, gui_basics, etc.)
target_link_libraries(HungryGhostMultibandCompressor
    PRIVATE
        CommonUI
        HGMBCBinaryData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_core
)

# ==================== Tests (JUCE UnitTest console app) ====================
juce_add_console_app(HGMBCTests_DSP
    PRODUCT_NAME "HGMBC Tests (DSP)"
)
juce_generate_juce_header(HGMBCTests_DSP)

# Include plugin sources required for tests (headless) and DSP sources
file(GLOB_RECURSE HG_MBC_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
)

# Reuse DSP sources and selectively add processor (avoid editor in headless tests)
target_sources(HGMBCTests_DSP PRIVATE
    ${HG_MBC_DSP_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.h
    ${HG_MBC_TEST_SOURCES}
)

target_include_directories(HGMBCTests_DSP PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(HGMBCTests_DSP PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_dsp
)

target_compile_definitions(HGMBCTests_DSP PRIVATE HG_MBC_HEADLESS_TEST=1)
