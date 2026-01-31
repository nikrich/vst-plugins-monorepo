cmake_minimum_required(VERSION 3.22)
project(CommonAudio LANGUAGES C CXX)

add_library(CommonAudio INTERFACE)

target_include_directories(CommonAudio INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# Header-only convenience target for shared audio components.
