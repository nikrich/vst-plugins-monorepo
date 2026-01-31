cmake_minimum_required(VERSION 3.22)
project(MusicGPTClient VERSION 0.1.0 LANGUAGES C CXX)

# Find libcurl
find_package(CURL REQUIRED)

# Library sources
set(MUSICGPT_CLIENT_SOURCES
    src/ExtractionClient.cpp
    src/CurlHttpClient.cpp
    src/JsonParser.cpp
)

set(MUSICGPT_CLIENT_HEADERS
    include/MusicGPTClient.h
    include/ExtractionClient.h
    include/ExtractionJob.h
    include/ExtractionConfig.h
    src/CurlHttpClient.h
    src/JsonParser.h
)

add_library(MusicGPTClient STATIC
    ${MUSICGPT_CLIENT_SOURCES}
    ${MUSICGPT_CLIENT_HEADERS}
)

target_include_directories(MusicGPTClient
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(MusicGPTClient PUBLIC cxx_std_17)

target_link_libraries(MusicGPTClient
    PUBLIC
        CURL::libcurl
        juce::juce_core
        juce::juce_events
)
