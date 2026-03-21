# Teensy Audio Comparison

## Why The Old Pi Output Looked Wrong

The original Raspberry Pi pipeline in MuseVis used:

- a 2048-point FFT with 16 logarithmic bands
- one global running peak (`runningMax_`) for all bands
- dB normalization against that single peak
- renderer-side attack/decay smoothing after the FFT

That design made the visualizer sensitive to bass dominance: one loud low-frequency band could raise the shared reference level and compress the apparent motion of mids and highs.

TeensyVisualizer behaves differently:

- it reads 14 fixed hardware filter-bank channels rather than FFT bins
- each channel already has envelope-like behavior from the MSGEQ7 path
- software post-processing is simpler: smoothing, noise floor handling, quiet detection, and display logic

## What Changed In This Adaptation

MuseVis now includes two analyzer paths:

- `filterbank` (default): Teensy-style software filter bank with 14 analysis bands, per-band envelope followers, noise-floor suppression, and gentler AGC
- `fft`: the original 16-band FFT analyzer, retained for comparison and fallback

Renderers now:

- consume either 14 or 16 analysis bands projected onto the existing 16 display columns
- keep the 16-column/16-group layout
- use quiet-detection fade behavior instead of relying only on stale-frame blanking

## Runtime Comparison

Use the environment to compare both analyzers on the same Pi source:

```bash
MUSEVIS_ANALYZER=fft MUSEVIS_BAND_DUMP=fft.csv build/musevis-tui musevis_sink.monitor
MUSEVIS_ANALYZER=filterbank MUSEVIS_BAND_DUMP=filterbank.csv build/musevis-tui musevis_sink.monitor
```

`MUSEVIS_BAND_DUMP` writes one CSV row per published analysis frame:

- `frame`
- `analyzer`
- `band_0 ... band_13`

That makes it easy to compare:

- whether bass still suppresses the rest of the spectrum
- whether silence decays cleanly
- whether transients produce cleaner per-band motion

For Raspberry Pi validation, a helper script is also available:

```bash
bash scripts/compare-analyzers.sh build/musevis-tui musevis_sink.monitor 10
```

The helper script expects GNU `timeout`, which is standard on Raspberry Pi OS. On macOS, use `gtimeout` from coreutils or run the commands manually.

## Intended Tuning Direction

The new default path is calibrated to approximate the feel of the reference hardware, not to exactly emulate MSGEQ7 analog circuitry. The main tunable surfaces are:

- band centers and widths in `src/dsp/TeensyBandLayout.h`
- filter-bank attack/release and AGC in `src/dsp/FilterBankProcessor.cpp`
- quiet-fade thresholds in `src/render/RenderEngine.cpp` and `src/render/TerminalRenderer.cpp`
