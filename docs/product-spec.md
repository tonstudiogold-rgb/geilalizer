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
14. NAM integration remains fixed and internal: only product-selected models are wired into hidden slots, never exposed as a loader or selector.

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

Fixed hidden per-channel signal order:

Input
→ Input Gain / IR drive
→ Selected hidden 1073 IR/preamp colour
→ Fixed hidden Neve 5315 console NAM line stage
→ Fixed hidden Studer C37 tape NAM
→ Future hidden fader model/mapped fader files
→ Visible Fader gain
→ Hidden post-fader SSL 4000 G IR mapping by fader third
→ Pan
→ Stereo Sum
→ Mixbus heat measurement from summed level only
→ Pre-mixbus-IR safety limiter, ceiling -0.2 dBFS
→ Hidden Model 5 SSL 4000 G mixbus IR selected by summed level
→ Fixed hidden Studer C37 tape NAM again on the stereo mixbus
→ Output Trim/Gain
→ Switchable hidden EMT 266X NAM limiter, on/off only
→ Safety limiter, ceiling -0.2 dBFS
→ Fixed hidden final Hilo NAM 1
→ Final safety limiter, ceiling -0.2 dBFS
→ Output / Render

Model 5 mixbus IR selection is hidden and purely level-dependent; fader positions and active-channel counts are not used:

| Summed peak before pre-IR safety limiter | Hidden Model 5 IR |
| --- | --- |
| below -12 dBFS | `4000 G mixbuss 0db_dc.wav` |
| -12 dBFS to below -7 dBFS | `4000 G mixbuss 5db_dc.wav` |
| -7 dBFS to below -3 dBFS | `4000 G mixbuss 10db_dc.wav` |
| -3 dBFS and hotter | `4000 G mixbuss extreme 1_dc.wav` |

Important: the visible fader remains the user's level control. The SSL 4000 G IR family is a hidden post-fader coloration stage after that visible fader gain and before pan/sum: lower fader third = 0db, middle third = 5db, upper third = 10db.

## Master contract

The master section has:

- EMT 266X NAM limiter on/off (the only user-switchable hidden NAM limiter)
- Master meter
- Output gain trim knob; default/middle position is neutral gain
- Limiter activity indicator for fixed safety limiting

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
- Hidden NAM adapter uses NeuralAmpModelerCore internally when `GEILALIZER_ENABLE_NAMCORE=ON`; user-facing NAM selection/loading is not allowed

## Current scaffold status

The scaffold provides the JUCE application entry point, a macOS-ready standalone CMake target, an English UI shell with 24 channel strips plus master controls, shared realtime/render core processing, safety limiters, hidden IR stages including level-selected Model 5 mixbus IRs, and fixed hidden NAM slots for channel console/tape, second mixbus tape, optional EMT 266X limiter, and final Hilo NAM 1 when private model assets are present. File import, WAV export writing, resampling, persistence, and production UI wiring remain future work.
