cmake_minimum_required(VERSION 3.22)
project(MusicGPTExtractor VERSION 0.1.0 LANGUAGES C CXX)

# Prefer Release for performance
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Universal build (optional)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

# Pull in vendored JUCE
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)
# Pull in shared CommonUI library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../CommonUI ${CMAKE_CURRENT_BINARY_DIR}/CommonUI)

# Find libcurl
find_package(CURL REQUIRED)

# Collect sources
file(GLOB_RECURSE MGE_SOURCES CONFIGURE_DEPENDS
    Source/*.cpp
    Source/*.h
)

# Embed assets
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../assets")
juce_add_binary_data(MGEBinaryData
    SOURCES
        ${ASSETS_DIR}/logo.png
        ${ASSETS_DIR}/logo-img.png
        "${ASSETS_DIR}/ui/kit-03/background/background-03.png"
        ${ASSETS_DIR}/fonts/Montserrat/Montserrat-VariableFont_wght.ttf
)

juce_add_plugin(MusicGPTExtractor
    COMPANY_NAME "Hungry Ghost"
    BUNDLE_ID com.hungryghost.musicgptextractor
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3
    PRODUCT_NAME "MusicGPT Extractor"
)

target_sources(MusicGPTExtractor PRIVATE ${MGE_SOURCES})
target_compile_features(MusicGPTExtractor PRIVATE cxx_std_17)
juce_generate_juce_header(MusicGPTExtractor)

target_compile_definitions(MusicGPTExtractor PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MusicGPTExtractor
    PRIVATE
        MGEBinaryData
        CommonUI
        CURL::libcurl
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_core
)
