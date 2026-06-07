# Product Specification: 24-Spur Standalone Geilalizer

## Identity

24-Spur Standalone Geilalizer is a fixed 24-track tape-machine-style standalone application for macOS. It is a bandmaschine, not a mixer, DAW, plugin, plugin host, NAM loader, IR loader, or EQ.

## Non-negotiable product rules

1. The UI is English.
2. The product is one fixed machine.
3. The first deliverable is a macOS standalone app.
4. No plugin target is part of this scaffold.
5. There are at most 24 mono channels.
6. Stereo imports are split automatically and occupy two mono channels.
7. Mixed sample rates are automatically resampled.
8. Realtime playback and export use the same signal path.
9. Realtime playback is full quality.
10. Export may run faster than realtime without quality loss.
11. No EQ controls are exposed.
12. Invisible technical filters may be used only when necessary for correctness or safety.
13. NAM models are not user-loadable, not visible, and not managed through the UI.
14. NAM integration happens later one model at a time, only after the hidden adapter integration point requires a specific model.

## Channel contract

Each channel is mono and has:

- Editable numeric name
- ARM button
- Input gain range: -24 dB to +24 dB
- Input gain default: 0 dB
- Visible fader
- Pan control
- Meter
- Load/drop affordance for audio import

## Master contract

The master section has:

- Limiter on/off
- Master meter
- Output gain trim knob
- Limiter activity indicator

## Safety limiter contract

- Threshold: 0 dB
- Ceiling: -0.2 dBFS
- Applies to playback and export
- Cannot create a separate export-only sound path

## Export contract

Export format:

- WAV only for MVP
- Sample rates: 44.1 kHz and 48 kHz
- Bit depths: 16-bit and 24-bit
- Range: full recording length or custom minute/second start/end range

## Implementation constraints

- C++20
- JUCE via CMake FetchContent
- CMake standalone app target
- Portable core code where possible so CI/static checks can run outside macOS-specific packaging
- Hidden NAM adapter comments and seams are allowed; user-facing NAM selection/loading is not allowed

## Current scaffold status

The scaffold provides the JUCE application entry point, a macOS-ready standalone CMake target, and an English UI shell with 24 channel strips plus master controls. Audio engine, file import, render/export, limiter, resampling, persistence, and hidden NAM adapter implementations are intentionally future work.
