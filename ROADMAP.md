# Geilalizer Milestone Roadmap

Status: main is frozen for feature work. Stabilization continues on `milestone/realtime-rescue`.

## Product Contract

Geilalizer is a standalone fixed 24-track tape-machine app.

Non-negotiables:

- Standalone app first.
- Not a DAW.
- Not a plugin.
- Not a plugin host.
- Not a NAM loader.
- Not an IR loader.
- Maximum 24 mono channels.
- Hidden fixed NAM/IR machine chain.
- User only sees musical controls.
- Playback and export must use the same machine path.
- Missing assets must fall back/bypass safely; they must not crash the app.

## M0 — Repository Freeze & Product Contract

- Freeze `main` for feature work.
- Work on `milestone/realtime-rescue`.
- Keep product contract above visible in repo.
- Do not track `private_nam_models/` or proprietary NAM/IR/PDF model packs.
- Pin external dependencies to concrete versions/commits.

Acceptance:

- `private_nam_models/` is ignored by Git and has no tracked files in HEAD.
- Dependency policy is covered by `RepositoryPolicySmoke`.

## M1 — Realtime Audio Rescue

Make the audio callback realtime-safe.

Remove from the audio callback:

- vector construction
- resize
- assign with possible allocation
- file access
- string mutation
- mutex locks / waits
- model loading
- session mutation

Required architecture:

- Preallocate callback buffers in `prepareToPlay()` or equivalent non-realtime preparation.
- GUI-to-audio communication via atomics or a lock-free command/snapshot system.
- Recording growth/allocation outside the callback, or via preallocated/lock-free buffers.

Acceptance:

- 1, 8, 16, and 24 tracks play without dropouts.
- Audio callback performs no heap allocation.
- Audio callback never waits for GUI locks.
- Device changes do not crash the app.

## M2 — Fixed Machine Architecture

Required fixed path:

Input
→ Input Gain
→ hidden preamp/IR colour
→ fixed hidden Neve NAM
→ fixed hidden Studer C37 NAM
→ visible Fader
→ hidden post-fader SSL IR
→ Pan
→ Stereo Sum
→ pre-mixbus safety limiter
→ hidden mixbus IR
→ fixed hidden Studer C37 mixbus NAM
→ Output Trim
→ switchable EMT 266X NAM limiter
→ safety limiter
→ final Hilo NAM
→ final safety limiter
→ Output/Render

No user-visible NAM slots. No user-visible IR slots. No EQ.

## M3 — Asset Loading Separation

Create:

- `AssetRegistry`
- `PreparedMachineState`
- background asset loading
- atomic swap into the audio engine

Move NAM/IR scanning and loading out of realtime preparation.

## M4 — Clickless Limiter Switching

The EMT limiter ON/OFF must not hard switch.

Implement:

- dry path
- wet EMT path
- 64–256 sample crossfade
- optional gain matching
- always-on final safety limiter

Acceptance:

- Limiter can be switched during playback without clicking.
- Export and playback use the same limiter behavior.

## M5 — Playback/Export Parity

Playback and export must share the same signal path.

Acceptance:

- Offline export matches realtime render.
- Export supports 44.1/48 kHz and 16/24 bit WAV.
- Full-length and custom range export work.
- Mixed sample rates are resampled safely.

## M6 — GUI Cleanup

GUI is only a control surface.

Visible controls:

- 24 channels
- editable numeric channel names
- ARM
- input gain
- fader
- pan
- meter
- master EMT limiter ON/OFF
- output trim
- master meter
- safety-limiter activity

GUI must not contain DSP decisions, routing logic, model loading, or file scanning inside visual/control callbacks.

## M7 — Reproducible Build & Alpha Release

Acceptance:

- Fresh clone builds on macOS.
- CI passes.
- CoreSmoke, NamAdapterSmoke, ProductE2E, and RepositoryPolicySmoke pass.
- Release artifact contains no private models.
- Tag release as `v0.1.0-alpha` only after M1–M6 are green.
