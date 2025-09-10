cmake_minimum_required(VERSION 3.22)
project(HungryGhostLimiter VERSION 0.1.0 LANGUAGES C CXX)

# Prefer Release for DSP quality/perf
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Universal build example (optional); harmless if ignored by Makefiles
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

# Pull in vendored JUCE
add_subdirectory(JUCE)

# Collect sources
file(GLOB_RECURSE HGL_SOURCES CONFIGURE_DEPENDS
    Source/*.cpp
    Source/*.h
)

# Embed assets (fonts/logo live one level up from this dir)
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets")
juce_add_binary_data(HGLBinaryData
    SOURCES
        ${ASSETS_DIR}/logo.png
        ${ASSETS_DIR}/fonts/Montserrat/Montserrat-Italic-VariableFont_wght.ttf
        ${ASSETS_DIR}/fonts/Montserrat/Montserrat-VariableFont_wght.ttf
)

juce_add_plugin(HungryGhostLimiter
    COMPANY_NAME "Hungry Ghost"
    BUNDLE_ID com.hungryghost.hglimiter
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3
    PRODUCT_NAME "Hungry Ghost Limiter"
)

target_sources(HungryGhostLimiter PRIVATE ${HGL_SOURCES})
target_compile_features(HungryGhostLimiter PRIVATE cxx_std_17)
juce_generate_juce_header(HungryGhostLimiter)

# Avoid VST2/VST3 automation replacement conflicts when building only VST3
target_compile_definitions(HungryGhostLimiter PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)

# Link JUCE modules + binary data
# Keep module list broad to match project includes
# (audio_processors implies core, graphics, gui_basics, etc.)
target_link_libraries(HungryGhostLimiter
    PRIVATE
        HGLBinaryData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_core
)

# Optional: deterministic IDs (uncomment and adjust as needed)
# target_compile_definitions(HungryGhostLimiter PRIVATE
#     JucePlugin_Manufacturer="Hungry Ghost"
#     JucePlugin_ManufacturerCode='HGST'
#     JucePlugin_PluginCode='HGL1'
# )
