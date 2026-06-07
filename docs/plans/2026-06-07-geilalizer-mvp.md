# 24-Track Standalone Geilalizer MVP Implementation Plan

> For Hermes: Use subagent-driven-development/orchestrator. Build a vertical slice now; do not request NAM files until the hidden NAM adapter integration point is ready and a single specific model is required.

Goal: Build a macOS standalone 24-mono-channel tape-machine-style audio app scaffold with realtime playback/export architecture and hidden NAM-ready processing slots.

Architecture: JUCE/CMake standalone app. Every channel is mono; stereo imports split into two mono channels. Realtime playback and faster-than-realtime render use the same engine path. NAM models are not user-loadable and not visible; request and integrate exactly one NAM model at a time later.

Tech Stack: C++20, JUCE via CMake FetchContent, macOS Standalone target first, portable core code for CI/static checks.

Hard product rules:
- English UI.
- This is one fixed machine, not a mixer, DAW, plugin host, NAM loader, IR loader, or EQ.
- 24 mono channels max.
- Stereo files auto-occupy two mono channels.
- Channel controls: editable numeric name, ARM, input gain -24..+24 dB default 0, visible fader, pan, meter.
- Master: limiter on/off, master meter, output gain trim knob, limiter activity.
- Safety limiter threshold 0 dB, ceiling -0.2 dBFS, protects playback and export.
- Export WAV 44.1/48 kHz, 16/24 bit, full length or custom minute/second range.
- Mixed samplerates auto-resample.
- No EQ controls. Only invisible technical filters if absolutely required.
- Realtime playback is full quality; export uses same signal path and may run faster than realtime without quality loss.

Tasks:

1. Project scaffold
- Create CMakeLists.txt with JUCE FetchContent and Standalone target.
- Create src/App.h/.cpp, src/MainComponent.h/.cpp, src/PluginProcessor-free core not needed because standalone only.
- Add README and docs/product-spec.md.
- Add .gitignore and GitHub Actions macOS build workflow.

2. Core model and engine
- Create ChannelState, SessionState, AudioEngine, MachineProcessor, SafetyLimiter, RenderEngine.
- Implement constants: 24 channels, input gain range, limiter ceiling, export rates/bit depths.
- Implement mono channel model and stereo split import API placeholders.
- Implement hidden NAM adapter placeholder with explicit one-at-a-time integration comments.

3. UI vertical slice
- 24 channel strips with number/name, ARM, input gain, fader, pan, meter placeholder, load/drop affordance.
- Master section with limiter toggle, output trim knob, master meter placeholder, limiter activity.
- Transport: Load, Play, Stop, Export.
- Export dialog model showing recording length and full/custom range fields.

4. Verification
- Run git diff --check.
- Run cmake configure if available. Linux configure may fail due GUI deps; document exact result.
- Commit locally. GitHub push is blocked until auth is provided.
