cmake_minimum_required(VERSION 3.28)

project(rom C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(src/common)
add_subdirectory(src/compiler)
add_subdirectory(src/vm)
add_subdirectory(src/simulator)
add_subdirectory(tools)

enable_testing()
add_subdirectory(tests)
