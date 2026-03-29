# MuseVis Specification

## Purpose

MuseVis is a real-time music visualizer written in C++17. The repository provides:

- `musevis-tui`: a terminal visualizer
- `musevis`: a Raspberry Pi LED visualizer built on `rpi_ws281x`

Both binaries capture live audio from a PulseAudio-compatible source, analyze it with a TeensyVisualizer-inspired 14-band software filter bank, and render the resulting magnitudes continuously.

## Build Targets

### `musevis-tui`

- Always built
- Entry point: `src/main_tui.cpp`
- Intended for terminal-only visualization
- Depends on:
  - `libpulse-simple`
  - `pthread`
  - `libm`

### `musevis`

- Built only when `third_party/rpi_ws281x/CMakeLists.txt` exists
- Entry point: `src/main.cpp`
- Intended for Raspberry Pi LED output
- Depends on:
  - `libpulse-simple`
  - `rpi_ws281x`
  - `pthread`
  - `libm`

## Runtime Interface

Both binaries require a single positional argument:

```text
<pulse-source>
```

Example:

```bash
./run-tui.sh musevis_sink.monitor
sudo ./run-led.sh musevis_sink.monitor
```

If no source is provided, the program prints usage text and exits with status `1`.

The process handles `SIGINT` and `SIGTERM` and shuts down cleanly by stopping worker threads before exiting.

## Core Constants

The following constants are defined in `include/musevis/SharedState.h`:

- Sample rate: `44100 Hz`
- Channels: `2`
- Audio chunk size: `1024` frames
- Number of analysis/display bands: `14`
- LEDs per band: `20`
- Total LED count: `280`

## Architecture

MuseVis is organized around a shared double-buffered state:

1. The audio capture thread records stereo samples from PulseAudio.
2. A swappable analyzer processes the stereo chunk and publishes 14 normalized band magnitudes.
3. `SharedState::swapBuffers()` atomically publishes the new frame and increments a frame counter.
4. A renderer thread consumes the latest published frame at its own target frame rate.

### Shared State Contract

`SharedState` stores two `BandData` buffers and an atomic `frontIndex`.

- The audio side writes only to the back buffer.
- The render side reads only from the front buffer.
- Publication happens through an atomic front-buffer swap.
- `frameCounter` increments once per completed analyzer update.

This handoff is intentionally lock-free.

## Audio Capture

Audio capture is implemented in `src/audio/AudioCapture.cpp`.

- Backend: `pa_simple` from `libpulse-simple`
- Stream direction: `PA_STREAM_RECORD`
- Sample format: `PA_SAMPLE_FLOAT32LE`
- Source device: the command-line argument passed to the executable

The capture loop:

1. Opens a recording stream named `MuseVis`.
2. Reads `CHUNK_SIZE` stereo frames per iteration.
3. Passes the interleaved float buffer to a `BandAnalyzer`.
4. Continues until the thread is stopped or `pa_simple_read()` fails.

`AudioCapture` no longer depends on a specific DSP implementation. The default analyzer is selected by `src/dsp/AnalyzerFactory.cpp`.

## Spectrum Analysis

The active analyzer is implemented in `src/dsp/FilterBankProcessor.cpp`.

### Band Layout

The analyzer uses a fixed 14-band layout defined in `src/dsp/TeensyBandLayout.h`, with centers spanning `40 Hz` to `16 kHz`:

`40, 63, 100, 160, 250, 400, 630, 1.0k, 1.6k, 2.5k, 4.0k, 6.3k, 10k, 16k`

Each band definition also carries:

- a band-pass `Q`
- a display label
- a per-band gain trim

### Processing Pipeline

For each incoming chunk:

1. Stereo audio is mixed to mono.
2. Each band runs through its own biquad band-pass filter.
3. Filter output is rectified and fed into an envelope follower.
4. Per-band gain is applied.
5. A fixed noise gate suppresses very low idle energy.
6. A global AGC reference is computed from mean active-band energy instead of the single loudest band.
7. Magnitudes are clamped into `[0, 1]` and published to `SharedState`.

### Silence Handling

The analyzer tracks aggregate energy across the band set.

- If mean active-band energy remains below the quiet threshold for several frames, it publishes zeros.
- This makes silence handling analyzer-driven instead of relying only on stale-frame detection in the renderers.

## Terminal Renderer

Terminal rendering is implemented in `src/render/TerminalRenderer.cpp`.

### Frame Rate and Safety Blank

- Target frame rate: `30 FPS`
- If the shared `frameCounter` stops advancing for more than 6 render frames, the renderer forces magnitudes to zero as a fallback

### Presentation Smoothing

The terminal renderer now uses only light presentation smoothing from `src/render/PresentationSmoothing.h`.

- The analyzer owns the primary envelope motion
- The renderer applies a quick rise, moderate fall, and small quiet-tail clamp
- Peak hold remains as a terminal-only visual affordance

### Visual Output

The terminal renderer:

- Hides the cursor on start and restores it on stop
- Clears the screen and redraws from the top-left every frame
- Displays 14 columns, one per analysis band
- Uses a display height of 20 rows
- Uses 4 character cells per column
- Draws lit segments as 24-bit colored background blocks
- Draws a peak marker as a thin horizontal line
- Uses a hue sweep from red to violet across the band index range

## LED Renderer

LED rendering is implemented in `src/render/RenderEngine.cpp`.

### Frame Rate and Safety Blank

- Target frame rate: `60 FPS`
- If the shared `frameCounter` stops advancing for more than 12 render frames, the renderer forces magnitudes to zero as a fallback

### Frame Construction

For each of the 14 bands:

- The lightly smoothed band magnitude is scaled to a lit count from `0` to `20`
- The band hue is mapped linearly from `0` to `270` degrees
- LEDs are lit according to the serpentine layout (see below)
- Remaining LEDs are left off

### Serpentine LED Layout

The physical LED strip uses a serpentine (zigzag) wiring pattern:

- Even bands (0, 2, 4, …): LEDs run low-to-high (LED 0 = bottom of band)
- Odd bands (1, 3, 5, …): LEDs run high-to-low (last LED in the band = bottom)

Concretely, with 20 LEDs per band:

- Band 0: LEDs 0–19, index 0 = lowest magnitude
- Band 1: LEDs 20–39, index 39 = lowest magnitude
- Band 2: LEDs 40–59, index 40 = lowest magnitude
- Band 3: LEDs 60–79, index 79 = lowest magnitude
- …and so on for all 14 bands.

The renderer builds a flat pixel array of size:

```text
NUM_BANDS * LEDS_PER_BAND = 280
```

Pixels are encoded in GRB byte order as required by `ws2811`.

## LED Hardware Controller

Hardware output is implemented in `src/led/LEDController.cpp`.

Current configuration:

- GPIO pin: `18`
- DMA channel: `5`
- LED count: `280`
- Brightness: `255`
- Strip type: `WS2811_STRIP_GRB`
- Active channel: `channel[0]`

On startup the controller initializes `ws2811_t` and throws on failure. On shutdown it clears the strip and calls `ws2811_fini()`.

## Color Mapping

Color conversion is implemented in `src/led/ColorMapper.h`.

- Input: HSV
- Output: 8-bit RGB
- Saturation `0` produces grayscale
- Hue values are wrapped with `fmod(h, 360.0f)`

Both renderers use the same HSV-to-RGB conversion logic.

## Threading Model

Current thread layout:

- Main thread
- One audio capture thread
- One renderer thread

There is no explicit back-pressure or queueing layer between capture and rendering. Renderers always consume the latest published band frame.

## Build System

The root `CMakeLists.txt` currently:

- Requires CMake `3.18+`
- Uses C++17
- Includes CTest via `include(CTest)` and honors `BUILD_TESTING`
- Uses `pkg-config` to locate PulseAudio
- Compiles with `-Wall -Wextra -O2`

Helper scripts in the repository root build into distinct directories to avoid stale-binary confusion:

- `build-tui.sh` -> `build/tui/musevis-tui`
- `build-led.sh` -> `build/led/musevis`
- `run-tui.sh` builds then executes `build/tui/musevis-tui`
- `run-led.sh` builds then executes `build/led/musevis`

Targets:

- `musevis-tui` includes:
  - `src/main_tui.cpp`
  - `src/audio/AudioCapture.cpp`
  - `src/dsp/AnalyzerFactory.cpp`
  - `src/dsp/FilterBankProcessor.cpp`
  - `src/render/TerminalRenderer.cpp`
- `musevis` includes:
  - `src/main.cpp`
  - `src/audio/AudioCapture.cpp`
  - `src/dsp/AnalyzerFactory.cpp`
  - `src/dsp/FilterBankProcessor.cpp`
  - `src/led/LEDController.cpp`
  - `src/render/RenderEngine.cpp`

## Verification Assets

The repository now includes focused DSP and contract tests under `tests/`.

- When CMake is available, `CMakeLists.txt` registers them with CTest.
- When CMake is unavailable, the pure C++ tests can still be compiled manually with a local compiler by including `include/` and `src/`, and linking `src/dsp/FilterBankProcessor.cpp` for the analyzer tests.

These tests cover:

- 14-band shared-state contract
- band-layout contract
- analyzer injection boundary
- filter-bank selectivity, silence handling, and decay
- presentation smoothing behavior

## Operational Assumptions

The repository assumes:

- A PulseAudio-compatible server is available
- The requested source name already exists
- Audio is available as stereo float input at `44100 Hz`
- The LED build runs on hardware supported by `rpi_ws281x`
- The caller has sufficient privileges for GPIO/DMA access when running the LED build

## Current Limitations

Based on the current implementation:

- Only one Pulse source can be captured per process instance
- The renderer layout is fixed to 14 bands and 20 LEDs/rows per band
- Runtime configuration is minimal and limited to the source name argument
- No persistent configuration file format is implemented
- Error reporting from the capture loop is minimal after startup failures
- If the audio read loop breaks because of a device/runtime error, the capture thread exits silently and the renderer eventually decays to zero
- Final band tuning is still empirical and should be checked on Raspberry Pi hardware with real program material

## Source File Map

- `src/main_tui.cpp`: terminal application entry point
- `src/main.cpp`: LED application entry point
- `src/audio/AudioCapture.h/.cpp`: PulseAudio capture worker
- `src/dsp/BandAnalyzer.h`: analyzer interface
- `src/dsp/AnalyzerFactory.h/.cpp`: default analyzer selection
- `src/dsp/BiquadFilter.h`: band-pass filter helper
- `src/dsp/FilterBankProcessor.h/.cpp`: Teensy-style filter-bank analyzer
- `src/dsp/TeensyBandLayout.h`: 14-band layout and gain trims
- `src/render/PresentationSmoothing.h`: lightweight renderer smoothing helpers
- `src/render/TerminalRenderer.h/.cpp`: terminal visualization
- `src/render/RenderEngine.h/.cpp`: LED frame generation loop
- `src/led/LEDController.h/.cpp`: `rpi_ws281x` wrapper
- `src/led/ColorMapper.h`: HSV-to-RGB conversion
- `include/musevis/SharedState.h`: shared constants and lock-free frame handoff
