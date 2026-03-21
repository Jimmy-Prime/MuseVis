# MuseVis Specification

## Purpose

MuseVis is a real-time music visualizer written in C++17. The current codebase provides:

- `musevis-tui`: a terminal spectrum analyzer
- `musevis`: a Raspberry Pi LED spectrum visualizer built on `rpi_ws281x`

Both binaries capture live audio from a PulseAudio-compatible source, perform FFT-based spectral analysis, reduce the spectrum into 16 logarithmic bands, and render the resulting magnitudes continuously.

This document describes the behavior implemented in the repository today.

## Build Targets

### `musevis-tui`

- Always built
- Source entry point: `src/main_tui.cpp`
- Intended for terminal-only visualization
- Depends on:
  - `libpulse-simple`
  - `fftw3`
  - `pthread`
  - `libm`

### `musevis`

- Built only when `third_party/rpi_ws281x/CMakeLists.txt` exists
- Source entry point: `src/main.cpp`
- Intended for Raspberry Pi LED output
- Depends on:
  - `libpulse-simple`
  - `fftw3`
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
build/musevis-tui musevis_sink.monitor
build/musevis musevis_sink.monitor
```

If no source is provided, the program prints usage text and exits with status `1`.

The process handles `SIGINT` and `SIGTERM` and shuts down cleanly by stopping worker threads before exiting.

## Core Constants

The following constants are defined in `include/musevis/SharedState.h`:

- Sample rate: `44100 Hz`
- Channels: `2`
- FFT size: `2048`
- Audio chunk size: `1024` frames
- Number of frequency bands: `16`
- LEDs per band: `16`

The chunk size and FFT size create a 50% overlap model: each new chunk advances the analysis window by 1024 frames while retaining the previous 1024 frames.

## Architecture

MuseVis is organized around a shared double-buffered state:

1. The audio capture thread records stereo samples from PulseAudio.
2. The FFT processor converts them to mono, applies a Hann window, runs FFTW, and computes 16 normalized band magnitudes.
3. The processed frame is written to the back buffer in `SharedState`.
4. `SharedState::swapBuffers()` atomically publishes the new frame and increments a frame counter.
5. A renderer thread consumes the front buffer at its own target frame rate.

### Shared State Contract

`SharedState` stores two `BandData` buffers and an atomic `frontIndex`.

- The audio side writes only to the back buffer.
- The render side reads only from the front buffer.
- Publication happens through an atomic front-buffer swap.
- `frameCounter` increments once per completed FFT/band update.

This is intentionally lock-free.

## Audio Capture

Audio capture is implemented in `src/audio/AudioCapture.cpp`.

- Backend: `pa_simple` from `libpulse-simple`
- Stream direction: `PA_STREAM_RECORD`
- Sample format: `PA_SAMPLE_FLOAT32LE`
- Server: default PulseAudio-compatible server
- Source device: the command-line argument passed to the executable

The capture loop:

1. Opens a recording stream named `MuseVis`.
2. Reads `CHUNK_SIZE` stereo frames per iteration.
3. Passes the interleaved float buffer to `FFTProcessor::process()`.
4. Continues until the thread is stopped or `pa_simple_read()` fails.

If `pa_simple_new()` fails, construction of the audio worker raises a runtime exception.

## Spectrum Analysis

Spectrum analysis is implemented in `src/dsp/FFTProcessor.cpp` and `src/dsp/BandMapper.h`.

### Preprocessing

- Stereo input is mixed to mono by averaging left and right samples.
- The processor keeps a rolling overlap buffer of the last `2048` mono samples.
- A Hann window is applied before each FFT.
- FFT execution uses `fftw_plan_dft_r2c_1d()` with `FFTW_ESTIMATE`.

### Band Mapping

The analyzer computes 16 logarithmically spaced bands covering approximately `20 Hz` to `20 kHz`.

- Bin width is derived from `sampleRate / fftSize`
- Each band gets a `[binLow, binHigh)` range
- Each band is guaranteed at least one FFT bin

The TUI labels these bands approximately as:

`20, 31, 49, 77, 122, 193, 306, 485, 769, 1.2k, 1.9k, 3.1k, 4.9k, 7.7k, 12k, 19k`

### Magnitude Computation

For each band:

- FFT bin magnitudes are converted to power via `re^2 + im^2`
- The mean power of the bins in that band is computed
- The square root of that mean is used as the raw band magnitude

### Normalization

The current implementation uses dynamic normalization against a slowly decaying running peak:

- `runningMax` starts at `1e-9`
- Each frame updates it to `max(runningMax * 0.9999, maxBandMagnitude)`
- Each band is mapped to decibels relative to `runningMax`
- A fixed visible dynamic range of `48 dB` is used
- Values are clamped into `[0, 1]`

This means:

- The loudest recent content maps near `1.0`
- Quieter content can remain visible even when strong bass is present
- Absolute loudness is not preserved; the display is adaptive

## Terminal Renderer

Terminal rendering is implemented in `src/render/TerminalRenderer.cpp`.

### Frame Rate and Stale-Data Handling

- Target frame rate: `30 FPS`
- If the shared `frameCounter` stops advancing for more than 6 render frames, the renderer forces displayed magnitudes to zero
- This corresponds to roughly `200 ms` without fresh audio data

### Smoothing

Each band uses attack/decay smoothing:

- Rising values use coefficient `0.2`
- Falling values use coefficient `0.92`

The update equation is:

```text
smoothed = coeff * smoothed + (1 - coeff) * raw
```

The terminal view also maintains a peak-hold marker:

- Peaks rise immediately to the current smoothed value
- Peaks decay by `0.012` per rendered frame

### Visual Output

The terminal renderer:

- Hides the cursor on start and restores it on stop
- Clears the screen and redraws from the top-left every frame
- Displays 16 columns, one per frequency band
- Uses a display height of 16 rows
- Uses 4 character cells per column
- Draws lit segments as 24-bit colored background blocks
- Draws a peak marker as a thin horizontal line
- Uses a hue sweep from red to violet across the band index range
- Uses brighter colors toward the top of each bar

The terminal title line is:

```text
MuseVis — Terminal Spectrum Analyzer
```

## LED Renderer

LED rendering is implemented in `src/render/RenderEngine.cpp`.

### Frame Rate and Stale-Data Handling

- Target frame rate: `60 FPS`
- If the shared `frameCounter` stops advancing for more than 12 render frames, the renderer forces magnitudes to zero
- This is also roughly `200 ms` without fresh audio data

### Smoothing

Each band uses attack/decay smoothing:

- Rising values use coefficient `0.4`
- Falling values use coefficient `0.96`

This produces a faster rise and slower fall than the raw FFT output.

### LED Frame Construction

For each of the 16 bands:

- `smoothed[b]` is scaled to a lit count from `0` to `16`
- The band hue is mapped linearly from `0` to `270` degrees
- LEDs `0..litCount-1` are lit
- Remaining LEDs are left off

The renderer builds a flat pixel array of size:

```text
NUM_BANDS * LEDS_PER_BAND = 256
```

Pixels are encoded in GRB byte order as required by `ws2811`:

```text
0x00GGRRBB
```

## LED Hardware Controller

Hardware output is implemented in `src/led/LEDController.cpp`.

Current configuration:

- GPIO pin: `18`
- DMA channel: `5`
- LED count: `256`
- Brightness: `255`
- Strip type: `WS2811_STRIP_GRB`
- Active channel: `channel[0]`
- `channel[1]` is unused

On startup:

- The controller initializes `ws2811_t`
- It throws a runtime exception if `ws2811_init()` fails

On each render:

- The incoming 256-pixel buffer is copied into the driver buffer
- `ws2811_render()` is called

On shutdown:

- The strip is zeroed
- A final render is issued to blank the LEDs
- `ws2811_fini()` is called

### Hardware Constraint

The code comments note that GPIO 18 uses PWM and may conflict with the Raspberry Pi 3.5mm audio jack. If analog audio output is required, the implementation may need to move to GPIO 10 using SPI mode in `rpi_ws281x`.

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

The TUI build runs:

- Main thread
- Audio capture thread
- Terminal renderer thread

The LED build runs:

- Main thread
- Audio capture thread
- LED renderer thread

There is no explicit back-pressure or queueing layer between capture and rendering. Renderers always consume the latest published band frame.

## Build System

The root `CMakeLists.txt` currently:

- Requires CMake `3.18+`
- Uses C++17
- Uses `pkg-config` to locate PulseAudio and FFTW3
- Compiles with `-Wall -Wextra -O2`

Targets:

- `musevis-tui` includes:
  - `src/main_tui.cpp`
  - `src/audio/AudioCapture.cpp`
  - `src/dsp/FFTProcessor.cpp`
  - `src/render/TerminalRenderer.cpp`
- `musevis` includes:
  - `src/main.cpp`
  - `src/audio/AudioCapture.cpp`
  - `src/dsp/FFTProcessor.cpp`
  - `src/led/LEDController.cpp`
  - `src/render/RenderEngine.cpp`

## Operational Assumptions

The repository currently assumes:

- A PulseAudio-compatible server is available
- The requested source name already exists
- Audio is available as stereo float input at `44100 Hz`
- The LED build runs on hardware supported by `rpi_ws281x`
- The caller has sufficient privileges for GPIO/DMA access when running the LED build

The repository also includes setup guidance in:

- `Plan.md`
- `Next Step Instructions.txt`
- `setup-audio.sh`

`setup-audio.sh` automates null-sink and loopback creation, selects a non-MuseVis hardware sink, moves existing streams, verifies capture with `parec` and `sox`, and concludes by suggesting `build/musevis-tui musevis_sink.monitor`.

## Current Limitations

Based on the current implementation:

- Only one Pulse source can be captured per process instance
- The renderer layout is fixed to 16 bands and 16 LEDs/rows per band
- Runtime configuration is minimal and limited to the source name argument
- No persistent configuration file format is implemented
- No test suite is present in the repository
- Error reporting from the capture loop is minimal after startup failures
- If the audio read loop breaks because of a device/runtime error, the capture thread exits silently and the renderer eventually decays to zero

## Source File Map

- `src/main_tui.cpp`: terminal application entry point
- `src/main.cpp`: LED application entry point
- `src/audio/AudioCapture.h/.cpp`: PulseAudio capture worker
- `src/dsp/FFTProcessor.h/.cpp`: FFT processing and normalization
- `src/dsp/BandMapper.h`: logarithmic band boundary generation
- `src/render/TerminalRenderer.h/.cpp`: terminal visualization
- `src/render/RenderEngine.h/.cpp`: LED frame generation loop
- `src/led/LEDController.h/.cpp`: `rpi_ws281x` wrapper
- `src/led/ColorMapper.h`: HSV-to-RGB conversion
- `include/musevis/SharedState.h`: shared constants and lock-free frame handoff
