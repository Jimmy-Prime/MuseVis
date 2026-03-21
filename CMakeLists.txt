cmake_minimum_required(VERSION 3.18)
project(musevis LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Dependencies ─────────────────────────────────────────────────────────────

find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE REQUIRED libpulse-simple)
pkg_check_modules(FFTW3 REQUIRED fftw3)

# ── TUI target (no hardware dependencies) ────────────────────────────────────

add_executable(musevis-tui
    src/main_tui.cpp
    src/audio/AudioCapture.cpp
    src/dsp/FFTProcessor.cpp
    src/render/TerminalRenderer.cpp
)

target_include_directories(musevis-tui PRIVATE
    include
    src
    ${PULSE_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
)

target_link_libraries(musevis-tui PRIVATE
    ${PULSE_LIBRARIES}
    ${FFTW3_LIBRARIES}
    pthread
    m
)

target_compile_options(musevis-tui PRIVATE -Wall -Wextra -O2)

# ── LED target (requires rpi_ws281x submodule) ───────────────────────────────

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/rpi_ws281x/CMakeLists.txt")
    add_subdirectory(third_party/rpi_ws281x)

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
        third_party/rpi_ws281x
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
else()
    message(STATUS
        "rpi_ws281x submodule not found — skipping LED target.\n"
        "  To enable: git submodule add https://github.com/jgarff/rpi_ws281x.git third_party/rpi_ws281x")
endif()
