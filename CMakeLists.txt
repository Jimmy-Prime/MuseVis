cmake_minimum_required(VERSION 3.18)
project(musevis LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Dependencies ─────────────────────────────────────────────────────────────

find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE REQUIRED libpulse)
pkg_check_modules(FFTW3 REQUIRED fftw3)

# rpi_ws281x — populate with: git submodule update --init
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/rpi_ws281x/CMakeLists.txt")
    message(FATAL_ERROR
        "rpi_ws281x submodule not found.\n"
        "Run: git submodule update --init")
endif()
add_subdirectory(third_party/rpi_ws281x)

# ── Executable ───────────────────────────────────────────────────────────────

add_executable(musevis
    src/main.cpp
    src/audio/AudioCapture.cpp
    src/dsp/FFTProcessor.cpp
    src/led/LEDController.cpp
    src/render/RenderEngine.cpp
)

target_include_directories(musevis PRIVATE
    include
    src
    third_party/rpi_ws281x      # exposes ws2811.h
    ${PULSE_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
)

target_link_libraries(musevis PRIVATE
    ${PULSE_LIBRARIES}
    ${FFTW3_LIBRARIES}
    ws2811
    pthread
    m
)

target_compile_options(musevis PRIVATE -Wall -Wextra -O2)
