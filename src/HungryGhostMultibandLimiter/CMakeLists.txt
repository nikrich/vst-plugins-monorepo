cmake_minimum_required(VERSION 3.22)
project(HungryGhostMultibandLimiter VERSION 0.1.0 LANGUAGES C CXX)

# Prefer Release for DSP quality/perf
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Universal build example (optional); harmless if ignored by Makefiles
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

# Pull in vendored JUCE (moved to repo-level vendor/JUCE for reuse)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)
# Pull in shared CommonUI library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../CommonUI ${CMAKE_CURRENT_BINARY_DIR}/CommonUI)

# Collect sources
file(GLOB_RECURSE HGMBL_SOURCES CONFIGURE_DEPENDS
    Source/*.cpp
    Source/*.h
)

# Embed assets (fonts/logo) from repo root /assets
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../assets")
juce_add_binary_data(HGMBLBinaryData
    SOURCES
        ${ASSETS_DIR}/logo.png
        ${ASSETS_DIR}/logo-img.png
        "${ASSETS_DIR}/ui/kit-03/middle knob/mk-final.png"
        "${ASSETS_DIR}/ui/kit-03/background/background-03.png"
        "${ASSETS_DIR}/ui/kit-03/big knob/sb-final.png"
        "${ASSETS_DIR}/ui/kit-03/slider/sl-final.png"
        "${ASSETS_DIR}/ui/kit-06/button/001.png"
        "${ASSETS_DIR}/ui/kit-06/button/002.png"
        ${ASSETS_DIR}/fonts/Montserrat/Montserrat-Italic-VariableFont_wght.ttf
        ${ASSETS_DIR}/fonts/Montserrat/Montserrat-VariableFont_wght.ttf
)

juce_add_plugin(HungryGhostMultibandLimiter
    COMPANY_NAME "Hungry Ghost"
    BUNDLE_ID com.hungryghost.mbl
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3
    PRODUCT_NAME "Hungry Ghost Multiband Limiter"
    MODULES juce_dsp
)

target_sources(HungryGhostMultibandLimiter PRIVATE ${HGMBL_SOURCES})
target_compile_features(HungryGhostMultibandLimiter PRIVATE cxx_std_17)
juce_generate_juce_header(HungryGhostMultibandLimiter)

# Avoid VST2/VST3 automation replacement conflicts when building only VST3
target_compile_definitions(HungryGhostMultibandLimiter PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)

# Link JUCE modules + binary data
# Keep module list broad to match project includes
# (audio_processors implies core, graphics, gui_basics, etc.)
target_link_libraries(HungryGhostMultibandLimiter
    PRIVATE
        HGMBLBinaryData
        CommonUI
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_core
)

# ==================== Tests (JUCE UnitTest console apps) ====================
# DSP-focused tests (no GUI)
juce_add_console_app(HGMBLTests_DSP
    PRODUCT_NAME "HGMBL Tests (DSP)"
)
juce_generate_juce_header(HGMBLTests_DSP)

target_sources(HGMBLTests_DSP PRIVATE
    tests/MultibandLimiterDSPTests.cpp
)

target_include_directories(HGMBLTests_DSP PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(HGMBLTests_DSP PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_dsp
)
