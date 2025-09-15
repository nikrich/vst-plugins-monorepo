cmake_minimum_required(VERSION 3.22)
project(CommonUI LANGUAGES C CXX)

add_library(CommonUI INTERFACE)

target_include_directories(CommonUI INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# Header-only convenience target for shared UI headers.
