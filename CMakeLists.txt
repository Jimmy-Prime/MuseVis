cmake_minimum_required(VERSION 3.18)
project(musevis LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)

# ── Dependencies ─────────────────────────────────────────────────────────────

find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE REQUIRED libpulse-simple)

# ── TUI target (no hardware dependencies) ────────────────────────────────────

add_executable(musevis-tui
    src/main_tui.cpp
    src/audio/AudioCapture.cpp
    src/dsp/AnalyzerFactory.cpp
    src/dsp/FilterBankProcessor.cpp
    src/render/TerminalRenderer.cpp
)

target_include_directories(musevis-tui PRIVATE
    include
    src
    ${PULSE_INCLUDE_DIRS}
)

target_link_libraries(musevis-tui PRIVATE
    ${PULSE_LIBRARIES}
    pthread
    m
)

target_compile_options(musevis-tui PRIVATE -Wall -Wextra -O2)

if(BUILD_TESTING)
    find_program(BASH_EXECUTABLE bash)

    add_executable(shared_state_contract_test
        tests/shared_state_contract_test.cpp
    )
    target_include_directories(shared_state_contract_test PRIVATE include src)
    target_link_libraries(shared_state_contract_test PRIVATE m)
    add_test(NAME shared_state_contract_test COMMAND shared_state_contract_test)

    add_executable(teensy_band_layout_test
        tests/teensy_band_layout_test.cpp
    )
    target_include_directories(teensy_band_layout_test PRIVATE include src)
    target_link_libraries(teensy_band_layout_test PRIVATE m)
    add_test(NAME teensy_band_layout_test COMMAND teensy_band_layout_test)

    add_executable(audio_capture_boundary_test
        tests/audio_capture_boundary_test.cpp
    )
    target_include_directories(audio_capture_boundary_test PRIVATE include src)
    target_link_libraries(audio_capture_boundary_test PRIVATE m)
    add_test(NAME audio_capture_boundary_test COMMAND audio_capture_boundary_test)

    add_executable(filter_bank_processor_test
        tests/filter_bank_processor_test.cpp
        src/dsp/FilterBankProcessor.cpp
    )
    target_include_directories(filter_bank_processor_test PRIVATE include src)
    target_link_libraries(filter_bank_processor_test PRIVATE m)
    add_test(NAME filter_bank_processor_test COMMAND filter_bank_processor_test)

    add_executable(filter_bank_balance_test
        tests/filter_bank_balance_test.cpp
        src/dsp/FilterBankProcessor.cpp
    )
    target_include_directories(filter_bank_balance_test PRIVATE include src)
    target_link_libraries(filter_bank_balance_test PRIVATE m)
    add_test(NAME filter_bank_balance_test COMMAND filter_bank_balance_test)

    add_executable(presentation_smoothing_test
        tests/presentation_smoothing_test.cpp
    )
    target_include_directories(presentation_smoothing_test PRIVATE include src)
    target_link_libraries(presentation_smoothing_test PRIVATE m)
    add_test(NAME presentation_smoothing_test COMMAND presentation_smoothing_test)

    add_executable(terminal_presentation_test
        tests/terminal_presentation_test.cpp
    )
    target_include_directories(terminal_presentation_test PRIVATE include src)
    target_link_libraries(terminal_presentation_test PRIVATE m)
    add_test(NAME terminal_presentation_test COMMAND terminal_presentation_test)

    if(BASH_EXECUTABLE)
        add_test(NAME build_scripts_test
            COMMAND "${BASH_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/build_scripts_test.sh")
        add_test(NAME run_scripts_test
            COMMAND "${BASH_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/run_scripts_test.sh")
    endif()
endif()

# ── LED target (requires rpi_ws281x submodule) ───────────────────────────────

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/rpi_ws281x/CMakeLists.txt")
    # EXCLUDE_FROM_ALL prevents rpi_ws281x's "test" target from conflicting
    # with CTest's reserved "test" target.
    add_subdirectory(third_party/rpi_ws281x EXCLUDE_FROM_ALL)

    add_executable(musevis
        src/main.cpp
        src/audio/AudioCapture.cpp
        src/dsp/AnalyzerFactory.cpp
        src/dsp/FilterBankProcessor.cpp
        src/led/LEDController.cpp
        src/render/RenderEngine.cpp
    )

    target_include_directories(musevis PRIVATE
        include
        src
        third_party/rpi_ws281x
        ${PULSE_INCLUDE_DIRS}
    )

    target_link_libraries(musevis PRIVATE
        ${PULSE_LIBRARIES}
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
