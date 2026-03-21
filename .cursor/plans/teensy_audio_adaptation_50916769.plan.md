---
name: teensy audio adaptation
overview: Compare the current Raspberry Pi FFT pipeline against TeensyVisualizer’s hardware filter-bank design, then adapt MuseVis toward a software filter-bank/envelope architecture that should behave more like the reference on real music.
todos:
  - id: compare-current-vs-reference
    content: Document the algorithmic differences between MuseVis FFT analysis and TeensyVisualizer’s MSGEQ7-style band/envelope pipeline.
    status: completed
  - id: add-analyzer-abstraction
    content: Introduce a swappable analyzer interface so AudioCapture can use FFT or filter-bank implementations.
    status: completed
  - id: build-filter-bank-path
    content: Implement a 14-band software filter-bank with envelope followers, noise floor handling, and AGC tuned for Raspberry Pi input.
    status: completed
  - id: adapt-renderers-to-new-bands
    content: Decouple analysis bands from display bands and reduce renderer-side smoothing after the new DSP path is in place.
    status: completed
  - id: validate-on-pi
    content: Verify the new analyzer on Raspberry Pi with captured comparison inputs and tune band centers/release behavior empirically.
    status: in_progress
isProject: false
---

# Teensy-Style Audio Adaptation Plan

## Key Differences

- Current MuseVis uses a fixed 16-band FFT pipeline in [src/dsp/FFTProcessor.cpp](src/dsp/FFTProcessor.cpp) and [src/dsp/BandMapper.h](src/dsp/BandMapper.h): stereo is averaged to mono, analyzed with a 2048-point Hann-windowed FFT, reduced into 16 logarithmic bands, then normalized against a single slowly decaying global peak.
- TeensyVisualizer uses 14 hardware-derived bands from two clock-offset MSGEQ7 chips instead of FFT bins, then applies simple per-channel smoothing and a noise-floor based display pipeline. Public reference: [TeensyVisualizer README](https://github.com/dgorbunov/TeensyVisualizer/blob/main/README.md).
- MuseVis currently performs most perceptual shaping indirectly through global dB normalization and render-time attack/decay in [src/render/RenderEngine.cpp](src/render/RenderEngine.cpp) and [src/render/TerminalRenderer.cpp](src/render/TerminalRenderer.cpp). TeensyVisualizer’s behavior is dominated by bandpass/envelope physics first, software smoothing second.
- MuseVis uses 16 analysis/display bands from shared constants in [include/musevis/SharedState.h](include/musevis/SharedState.h). TeensyVisualizer is built around 14 bands and quiet-detection/auto-dim behavior rather than stale-frame blanking alone.
- Resulting likely root-cause mismatch on Pi: the current global `runningMax_` adaptive dB mapping can let bass dominate the reference level, making mids/highs look weak or unstable even when they are musically present.

## Recommended Direction

Replace the FFT-centric band extractor with a software filter-bank plus envelope followers, while preserving the existing capture and renderer threading model at first. This is the closest software analog to the reference hardware and gives you direct control over band sensitivity, decay feel, noise floor, and silence behavior.

```mermaid
flowchart LR
    pulseSource[Pulse Source] --> audioCapture[AudioCapture]
    audioCapture --> monoMix[Mono Mix]
    monoMix --> filterBank[Filter Bank]
    filterBank --> envelopes[Envelope Followers]
    envelopes --> agc[Noise Floor and AGC]
    agc --> sharedState[SharedState]
    sharedState --> renderers[LED and TUI Renderers]
```

## File Strategy

- Modify [include/musevis/SharedState.h](include/musevis/SharedState.h) to separate analysis-band count from display layout, or at minimum allow a 14-band analyzer without hard-coding every renderer around 16 FFT bands.
- Keep [src/audio/AudioCapture.cpp](src/audio/AudioCapture.cpp) as the Pulse capture entry point, but add a thin analyzer abstraction so capture no longer depends directly on FFT-specific processing.
- Replace or wrap [src/dsp/FFTProcessor.h](src/dsp/FFTProcessor.h) and [src/dsp/FFTProcessor.cpp](src/dsp/FFTProcessor.cpp) behind a common analyzer interface.
- Create a new filter-bank implementation, for example [src/dsp/FilterBankProcessor.h](src/dsp/FilterBankProcessor.h) and [src/dsp/FilterBankProcessor.cpp](src/dsp/FilterBankProcessor.cpp), responsible for digital bandpass filters, envelope detection, per-band gain staging, and normalization.
- Add a compact band-definition file, for example [src/dsp/TeensyBandLayout.h](src/dsp/TeensyBandLayout.h), to encode the 14 target channels and any measured/approximated center frequencies.
- Modify [src/render/RenderEngine.cpp](src/render/RenderEngine.cpp) and [src/render/TerminalRenderer.cpp](src/render/TerminalRenderer.cpp) so smoothing/quiet behavior is not fighting the analyzer. The analyzer should own more of the envelope feel; renderers should become thinner.

## Implementation Sequence

### 1. Reproduce and Instrument the Current Mismatch

- Add a lightweight offline analysis mode or debug dump so the same captured audio can be run through both the current FFT analyzer and the future filter-bank analyzer.
- Use a few representative audio clips: bass-heavy, vocal-focused, and percussion-heavy tracks.
- Log per-band values over time to confirm whether the current `runningMax_` logic is flattening non-bass bands.
- Success criterion: one short artifact or log that clearly shows why the current visual output looks wrong on Pi.

### 2. Introduce an Analyzer Boundary

- Create a `BandAnalyzer` interface that consumes interleaved stereo frames and publishes normalized band magnitudes.
- Adapt the existing FFT processor to implement that interface first, without changing behavior.
- Update [src/audio/AudioCapture.cpp](src/audio/AudioCapture.cpp) to depend on the interface rather than directly instantiating `FFTProcessor`.
- Success criterion: current behavior still works, but analyzer choice is swappable.

### 3. Implement a Teensy-Like Software Filter Bank

- Create 14 digital analysis channels approximating the reference project’s intent: separate fixed bands, per-band rectification or RMS, and envelope followers with controllable attack/release.
- Prefer a direct filter-bank design over another FFT post-process so each band has its own temporal behavior, similar to MSGEQ7 output.
- Start with documented/estimated 14-band spacing inspired by the reference project’s published `40 Hz–16 kHz` claim, then leave the exact centers tunable in one place.
- Success criterion: a static test clip produces stable, intuitive per-band motion without global pumping.

### 4. Replace Global Peak Normalization With Per-Band Conditioning

- Remove dependence on a single cross-band `runningMax_` for visual normalization.
- Add per-band floor suppression, optional per-band gain, and a gentler global AGC stage based on average or percentile energy, not the single loudest band.
- Add silence detection modeled after the reference project: quiet-state detection from aggregate band energy, then timed dim/decay behavior instead of only frame-staleness zeroing.
- Success criterion: kick drums no longer suppress the entire rest of the spectrum.

### 5. Resolve the 14-Band vs 16-Band UI Mismatch

- Preserve renderer stability first by keeping the current 16 display columns/segments while mapping 14 analysis bands into display space.
- After the audio behavior looks right, decide whether to keep a 14-to-16 display mapping or fully move the UI/LED layout to 14 columns.
- Recommended default: keep 16 physical display groups initially, because it reduces hardware/render churn while validating the algorithm.
- Success criterion: renderers remain usable during analyzer migration.

### 6. Simplify Renderer-Side Smoothing

- Reduce duplicated smoothing in [src/render/RenderEngine.cpp](src/render/RenderEngine.cpp) and [src/render/TerminalRenderer.cpp](src/render/TerminalRenderer.cpp) once the analyzer owns envelope shaping.
- Keep only light presentation smoothing or peak markers in the renderer.
- Success criterion: a single attack/release model defines motion instead of analyzer and renderer fighting each other.

### 7. Validate on Raspberry Pi Hardware

- Compare current FFT output and new filter-bank output on the same Pi audio source.
- Test silence, speech, broadband music, bass-heavy music, and rapid transients.
- Verify that the new path behaves better without introducing unstable idle flicker, laggy decay, or pinned low-band energy.
- If the result is close but not right, tune only the band centers, release times, and noise floor before considering broader architecture changes.

## Risks and Open Choices

- Exact TeensyVisualizer band centers are not fully spelled out in the README because the hardware uses clock-shifted MSGEQ7 chips; the software plan should treat those band centers as approximations to be tuned empirically.
- If the physical 16-column LED layout must remain unchanged, the cleanest architecture is to separate `analysisBands` from `displayBands` rather than forcing the DSP to stay at 16.
- If CPU use on Pi becomes an issue, use efficient biquad filters or a low-order filter bank instead of expensive overspecified DSP.

## Verification

- Run the existing app on Pi with a known test source before any DSP changes and record the current per-band output.
- After each major DSP step, compare against the same captured material rather than tuning by eye only.
- Verify both [src/main.cpp](src/main.cpp) and [src/main_tui.cpp](src/main_tui.cpp) still render meaningful output from the shared analyzer state.

## References

- TeensyVisualizer repository: [https://github.com/dgorbunov/TeensyVisualizer](https://github.com/dgorbunov/TeensyVisualizer)
- TeensyVisualizer README: [https://github.com/dgorbunov/TeensyVisualizer/blob/main/README.md](https://github.com/dgorbunov/TeensyVisualizer/blob/main/README.md)