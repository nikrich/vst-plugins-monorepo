cmake_minimum_required(VERSION 3.22)
project(HungryGhostMultibandLimiter VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS Source/*.cpp Source/*.h)

juce_add_plugin(HungryGhostMultibandLimiter
    COMPANY_NAME "Hungry Ghost"
    BUNDLE_ID com.hungryghost.mbl
    FORMATS VST3
    PRODUCT_NAME "Hungry Ghost Multiband Limiter"
)

target_sources(HungryGhostMultibandLimiter PRIVATE ${SOURCES})
target_compile_features(HungryGhostMultibandLimiter PRIVATE cxx_std_17)

target_link_libraries(HungryGhostMultibandLimiter PRIVATE
    juce::juce_audio_processors
    juce::juce_dsp
)
