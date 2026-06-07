# Geilalizer

24-Spur Standalone Geilalizer is a macOS standalone tape-machine-style audio application scaffold built with C++20, JUCE, and CMake.

## Product rules

- UI language is English.
- This is one fixed band machine, not a mixer, DAW, plugin, plugin host, NAM loader, IR loader, or EQ.
- Standalone app first; no plugin target is generated.
- 24 mono channels maximum.
- Stereo files must automatically occupy two mono channels.
- Channel controls: editable numeric name, ARM, input gain from -24 dB to +24 dB defaulting to 0 dB, visible fader, pan, and meter.
- Master controls: limiter on/off, master meter, output gain trim knob, and limiter activity.
- Safety limiter: threshold 0 dB, ceiling -0.2 dBFS, protecting both playback and export.
- Export: WAV at 44.1 kHz or 48 kHz, 16-bit or 24-bit, full length or a custom minute/second range.
- Mixed sample rates are automatically resampled.
- No EQ controls. Invisible technical filters are allowed only when absolutely required.
- Realtime playback is full quality. Export uses the same signal path and may run faster than realtime without quality loss.
- NAM models are hidden implementation details, not user-loadable and not visible in the UI. Integration will happen later one NAM model at a time when a specific model is needed.

## Repository layout

```text
CMakeLists.txt                         JUCE FetchContent standalone app target
src/App.h, src/App.cpp                 JUCE application/window entry point
src/MainComponent.h, src/MainComponent.cpp
                                       Initial English standalone UI scaffold
docs/product-spec.md                   Product constraints and implementation contract
docs/plans/2026-06-07-geilalizer-mvp.md
                                       MVP plan
.github/workflows/macos-build.yml      macOS CMake configure/build workflow
```

## Build locally

Prerequisites:

- CMake 3.22 or newer
- A C++20 compiler
- macOS with Xcode for the intended standalone app build

Configure and build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On macOS, the app bundle is produced by the `Geilalizer` standalone GUI app target.

## Scope note

This scaffold intentionally contains only the application shell and first visible control surface. Engine, import, render, limiter, resampling, export, and hidden NAM adapter code should be added behind these product rules without exposing NAM loading or EQ-style controls to users.
