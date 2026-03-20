# MuseVis: Raspberry Pi Hardware Sound Visualizer Plan

## Context

Build a real-time audio spectrum visualizer on Raspberry Pi. System audio is intercepted via PulseAudio's null-sink/monitor pattern so it plays to speakers *and* feeds the analyzer simultaneously. A C++ program performs FFT on the captured audio, maps results to 16 frequency bands, and drives 256 addressable WS2812B LEDs (16 groups × 16 LEDs/group) as a live spectrum display.

---

## Part 1: System Audio Capture Setup

**Approach: PulseAudio null-sink + loopback module**
This works on Ubuntu/Mint with native PulseAudio, and on Ubuntu 22.04+ with PipeWire (via PulseAudio compatibility layer — no code changes needed).

### Step-by-step

**1. Install deps**
```bash
sudo apt install pulseaudio pulseaudio-utils  # skip on Ubuntu 22.04+ (PipeWire handles it)
```

**2. Create virtual null sink + loopback (manual test)**
```bash
pactl load-module module-null-sink \
    sink_name=musevis_sink \
    sink_properties=device.description="MuseVis_Capture"

# Find real hardware sink name:
pactl list short sinks

pactl load-module module-loopback \
    source=musevis_sink.monitor \
    sink=<YOUR_HARDWARE_SINK_NAME> \
    latency_msec=20

pactl set-default-sink musevis_sink
```

**3. Make persistent** — add to `~/.config/pulse/default.pa`:
```
.include /etc/pulse/default.pa

load-module module-null-sink sink_name=musevis_sink sink_properties=device.description="MuseVis_Capture"
load-module module-loopback source=musevis_sink.monitor sink=<YOUR_HARDWARE_SINK> latency_msec=20
set-default-sink musevis_sink
```

**4. Verify**
```bash
pactl list short sources   # should show musevis_sink.monitor
parec --source=musevis_sink.monitor --rate=44100 --channels=2 --format=s16le | sox -t raw -r 44100 -e signed -b 16 -c 2 - -n stat 2>&1
```

**⚠️ Pi-specific note:** GPIO 18 (PWM) conflicts with the Pi's 3.5mm audio jack. If using 3.5mm audio output, wire LED data to GPIO 10 (SPI MOSI) and use SPI mode in rpi_ws281x instead of PWM. HDMI and USB audio have no conflict.

---

## Part 2: MuseVis C++ Program

### Key design parameters
- Sample rate: **44100 Hz**
- FFT size: **2048 samples** (~21.5 Hz/bin resolution, ~23ms latency)
- Read chunk: **1024 frames** (50% overlap for temporal smoothness)
- Update rate: **60 FPS** (render thread independent of audio thread)
- Frequency bands: **16**, logarithmically spaced 20 Hz–20 kHz
- LEDs: **256 total** (16 bands × 16 LEDs per band, VU-meter style)
- LED protocol: **WS2812B / GRB** via rpi_ws281x, GPIO 18 (PWM), DMA channel 5

### Threading model
```
Audio Thread (pa_simple_read, blocking)
  → 1024-frame reads from musevis_sink.monitor
  → Mix stereo→mono, overlap buffer, Hann window
  → FFTW3 execute
  → Compute 16 band RMS magnitudes → [0,1] normalized
  → Write to SharedState double-buffer, atomic index swap

Render Thread (sleep_until, 60 FPS)
  → Read latest SharedState front buffer
  → Apply attack/decay smoothing per band
  → Map to LED counts (0–16 lit per band)
  → HSV color per band (band 0=red, band 15=violet)
  → ws2811_render() via DMA/PWM
```

Lock-free handoff via double-buffer + atomic index swap — no mutex needed.

### File structure
```
MuseVis/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── audio/
│   │   ├── AudioCapture.h / .cpp     # pa_simple capture thread
│   │   └── RingBuffer.h              # (if needed for overflow protection)
│   ├── dsp/
│   │   ├── FFTProcessor.h / .cpp     # FFTW3, Hann window, overlap, band mapping
│   │   └── BandMapper.h              # log-spaced band boundary computation
│   ├── led/
│   │   ├── LEDController.h / .cpp    # rpi_ws281x wrapper
│   │   └── ColorMapper.h             # HSV→RGB inline
│   └── render/
│       ├── RenderEngine.h / .cpp     # 60 FPS loop, attack/decay, buildFrame
├── include/
│   └── musevis/
│       └── SharedState.h             # atomic double-buffer, constants
└── third_party/
    └── rpi_ws281x/                   # git submodule
```

### Frequency band boundaries (16 bands, log-spaced 20–20000 Hz)
| Band | ~Hz range | Bins (at 44100/2048 ≈ 21.5 Hz/bin) |
|------|-----------|--------------------------------------|
| 0 | 20–31 | 0–1 |
| 1 | 31–49 | 1–2 |
| 2 | 49–77 | 2–3 |
| 3 | 77–122 | 3–5 |
| 4 | 122–193 | 5–9 |
| 5 | 193–306 | 9–14 |
| 6 | 306–485 | 14–22 |
| 7 | 485–769 | 22–35 |
| 8 | 769–1219 | 35–56 |
| 9 | 1219–1933 | 56–89 |
| 10 | 1933–3063 | 89–142 |
| 11 | 3063–4856 | 142–225 |
| 12 | 4856–7697 | 225–357 |
| 13 | 7697–12202 | 357–566 |
| 14 | 12202–19342 | 566–897 |
| 15 | 19342–20000 | 897–927 |

*Note: Low bands (0–2) cover only 1–3 bins — apply heavier smoothing to reduce noise.*

### Visual logic per frame
```
for each band b in [0..15]:
    smoothed[b] = attack/decay filter applied to raw magnitude
    litCount = round(smoothed[b] * 16)   // how many LEDs light up
    hue = b / 15.0 * 270°                // red→violet across bands
    for each LED i in [0..15]:
        if i < litCount: color = HSV(hue, 1.0, 1.0)
        else:            color = off
```

### CMakeLists.txt structure
```cmake
cmake_minimum_required(VERSION 3.18)
project(musevis LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE REQUIRED libpulse)
pkg_check_modules(FFTW3 REQUIRED fftw3)
add_subdirectory(third_party/rpi_ws281x)

add_executable(musevis
    src/main.cpp
    src/audio/AudioCapture.cpp
    src/dsp/FFTProcessor.cpp
    src/led/LEDController.cpp
    src/render/RenderEngine.cpp)

target_include_directories(musevis PRIVATE include src
    ${PULSE_INCLUDE_DIRS} ${FFTW3_INCLUDE_DIRS})
target_link_libraries(musevis PRIVATE
    ${PULSE_LIBRARIES} ${FFTW3_LIBRARIES} ws2811 pthread m)
```

### Dependencies
```bash
sudo apt install -y build-essential cmake git pkg-config libpulse-dev libfftw3-dev
git submodule add https://github.com/jgarff/rpi_ws281x.git third_party/rpi_ws281x
```

### Hardware wiring
- LED data line → GPIO 18 (pin 12) through 300–470Ω resistor
- LED power: **external 5V supply** (256 LEDs full-white ≈ 15A — never power from Pi)
- Common ground between Pi and LED power supply

### Build & run
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo build/musevis musevis_sink.monitor
# Or cap for non-root: sudo setcap cap_sys_rawio+ep build/musevis
```

---

## Critical files to create
1. `include/musevis/SharedState.h` — constants (NUM_BANDS=16, LEDS_PER_BAND=16, FFT_SIZE=2048, CHUNK_SIZE=1024), atomic double-buffer
2. `src/dsp/FFTProcessor.cpp` — FFTW3 plan, Hann window, overlap buffer, log band computation, dB normalization
3. `src/audio/AudioCapture.cpp` — pa_simple with PA_SAMPLE_FLOAT32LE, dedicated thread
4. `src/render/RenderEngine.cpp` — 60 FPS loop, attack (0.8) / decay (0.15) smoothing, buildFrame
5. `src/led/LEDController.cpp` — ws2811_init, ws2811_render, GRB packing
6. `CMakeLists.txt` — ties libpulse + fftw3 + rpi_ws281x submodule

---

## Verification
1. **Audio setup**: `parec --source=musevis_sink.monitor | sox ... -n stat` shows non-zero RMS
2. **LED wiring**: test with rpi_ws281x example (`test_ws2811`) before integrating
3. **Full system**: run `musevis`, play music, observe 16 VU-meter columns responding to frequency content
4. **Latency check**: clap near mic and observe LED response — should be perceptibly instant (<100ms)
5. **PWM/SPI conflict check**: if using 3.5mm audio, switch to SPI mode (GPIO 10) in LEDController
