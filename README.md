# Geilalizer

24-Spur Standalone Geilalizer is a macOS standalone tape-machine-style audio application scaffold built with C++20, JUCE, and CMake.

## Product rules

- UI language is English.
- This is one fixed band machine, not a mixer, DAW, plugin, plugin host, NAM loader, IR loader, or EQ.
- Standalone app first; no plugin target is generated.
- 24 mono channels maximum.
- Stereo files must automatically occupy two mono channels.
- Channel controls: editable numeric name, ARM, input gain from -24 dB to +24 dB defaulting to 0 dB, visible fader, pan, and meter.
- Channel signal order: Input -> Input Gain driving selected hidden IR/preamp colour -> fixed hidden Neve 5315 console NAM -> fixed hidden Studer C37 tape NAM -> future hidden fader model/mapped fader files -> visible Fader gain -> hidden post-fader SSL 4000 G IR mapping by fader third -> Pan -> Stereo Sum -> pre-mixbus-IR safety limiter -> hidden Model 5 SSL 4000 G mixbus IR selected only from summed level -> fixed hidden Studer C37 tape NAM again on the mixbus -> Output Trim/Gain -> switchable EMT 266X NAM limiter -> safety limiter -> final Hilo NAM 1 -> final safety limiter -> Output/Render.
- Master controls: EMT 266X NAM limiter on/off, master meter, neutral-midpoint output gain trim knob, and safety-limiter activity.
- Safety limiter: threshold 0 dB, ceiling -0.2 dBFS, protecting both playback and export.
- Export: WAV at 44.1 kHz or 48 kHz, 16-bit or 24-bit, full length or a custom minute/second range.
- Mixed sample rates are automatically resampled.
- No EQ controls. Invisible technical filters are allowed only when absolutely required.
- Realtime playback is full quality. Export uses the same signal path and may run faster than realtime without quality loss.
- NAM models are hidden implementation details, not user-loadable and not visible in the UI. The fixed console/tape/mixbus-tape/final Hilo NAM slots and switchable EMT limiter NAM are loaded internally through NeuralAmpModelerCore when model assets are present.

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

Configure and build the macOS app:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On macOS, the app bundle is produced by the `Geilalizer` standalone GUI app target.

Headless Linux/server smoke-test build without JUCE GUI dependencies:

```sh
cmake -S . -B build-namtest -DCMAKE_BUILD_TYPE=Release -DGEILALIZER_BUILD_JUCE_APP=OFF -DGEILALIZER_ENABLE_NAMCORE=ON
cmake --build build-namtest --target CoreSmoke NamAdapterSmoke ProductE2E
ctest --test-dir build-namtest --output-on-failure
```

`GEILALIZER_ENABLE_NAMCORE=ON` fetches NeuralAmpModelerCore and compiles the hidden NAM stages with `NAM_SAMPLE_FLOAT`. `NamAdapterSmoke` loads and processes the private NAM assets when present; the broader headless smoke/E2E tests run with real NAMCore processing disabled by CTest so they stay fast while still validating the fixed hidden-stage bindings and shared render/playback path.

## Scope note

This scaffold now contains the application shell, first visible control surface, core playback/render engine, safety limiters, hidden 1073/channel SSL IR stages, hidden Model 5 level-selected mixbus IR stage, and NAMCore-backed console/tape/mixbus-tape/EMT-limiter/final-Hilo slots. Remaining work should continue behind these product rules without exposing NAM/IR loading or EQ-style controls to users.
