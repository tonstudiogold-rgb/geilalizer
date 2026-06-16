# Geilalizer Final Source Review — milestone/realtime-rescue

Status: PASS

Reviewed branch: `milestone/realtime-rescue`
Reviewed source commit: `fd9c37b39469e16bba15a54d92cf0af560671852` (`Gate macOS release upload to main`)
Review date: 2026-06-15

This review is the final pre-merge gate for `milestone/realtime-rescue` before any merge to `main` or new `macos-latest` release. No merge/release work was performed as part of this review document.

## Related review artifact

The macOS artifact inspection from GitHub Actions run `27519014310` is documented in:

- `docs/release-audits/fd9c37b-artifact-inspection.md`

That artifact inspection passed and found no private NAM assets in the artifact ZIP, unpacked ZIP app, DMG/APFS listing, or extracted DMG app.

## Source scope

Diff reviewed against `origin/main`:

- `.github/workflows/macos-build.yml`
- `.gitignore`
- `CMakeLists.txt`
- `ROADMAP.md`
- `src/MainComponent.cpp`
- `src/MainComponent.h`
- `src/core/AudioEngine.cpp`
- `src/core/AudioEngine.h`
- `src/core/TapeMachineState.h`
- `src/dsp/HiddenIrAdapter.*`
- `src/dsp/HiddenMixbusIrAdapter.*`
- `src/dsp/HiddenPostFaderIrAdapter.*`
- `src/dsp/MachineProcessor.*`
- release-gate tests under `tests/`

Commit range reviewed:

```text
fd9c37b Gate macOS release upload to main
6a642d4 Fix pre-merge realtime lifecycle issues
f888f7c Crossfade EMT limiter bypass
904b348 Move machine asset loading out of prepare
a235cad Add realtime playback stress coverage
9364bb3 Guard private model bundling for public builds
02e66ac Make audio callback realtime-safe
a9ca330 Freeze repo for realtime rescue milestone
```

## Required gates

| Gate | Result | Evidence |
| --- | --- | --- |
| No private assets in repo | PASS | `git ls-files` found no `*.nam` and no tracked `private_nam_models/`. Working-tree scan excluding `.git`/build also found none. `.gitignore` explicitly ignores `private_nam_models/` and `*.nam`. |
| No private assets in build artifact | PASS | `docs/release-audits/fd9c37b-artifact-inspection.md`; local report `/tmp/geilalizer-artifact-27519014310/artifact-inspection-report.txt`; ZIP and DMG scans reported zero forbidden entries. |
| No release upload outside `main` | PASS | `.github/workflows/macos-build.yml` gates the release upload step with `if: github.ref == 'refs/heads/main'`; `gh release upload` occurs only inside that step. |
| `GEILALIZER_BUNDLE_PRIVATE_MODELS=OFF` in CI release path | PASS | CI configure command uses `-DGEILALIZER_BUNDLE_PRIVATE_MODELS=OFF`; CMake option defaults to `OFF`; private model copy is guarded by `if(GEILALIZER_BUNDLE_PRIVATE_MODELS AND EXISTS ...)`. |
| Realtime callback without new allocations/locks | PASS | `MainComponent::getNextAudioBlock` uses preallocated buffers and atomics/shared buffer snapshots; no `std::lock_guard`, `trackMutex`, `.resize`, `.assign`, `juce::File`, file reader/writer, or session mutation tokens in the callback. Enforced by `RealtimeCallbackSourceAudit`. |
| Realtime callback without filesystem/model loading | PASS | Callback delegates to `audioEngine.processRealtime(...)` using prepared state; no `loadDefaultAssets`, `applyPreparedMachineState`, filesystem, or model-loading calls in callback. |
| EMT Limiter Bypass clickless | PASS | `MachineProcessor::processEmtLimiterBypass` uses dry/wet scratch buffers and per-frame crossfade over clamped 64-256 samples; hard if-switch bypass is rejected by `LimiterBypassClickPolicy`. |
| `prepare()` without asset/filesystem/model loading | PASS | `MachineProcessor::prepare` only sizes scratch buffers, prepares adapters, computes bypass crossfade length, and resets DSP state. Asset/model discovery moved to `loadDefaultAssetsOffAudioThread`. Enforced by `MachinePrepareAssetPolicy`. |
| `applyPreparedMachineState` status in app lifecycle | PASS | Current lifecycle: `MainComponent` calls `audioEngine.loadDefaultAssetsBeforeAudioStart()` before `setAudioChannels(channelCount, 2)`. `AudioEngine::loadDefaultAssetsBeforeAudioStart()` loads default assets off the audio path and immediately applies prepared state via `MachineProcessor::applyPreparedMachineState(...)`. `prepareToPlay` and `getNextAudioBlock` do not load/apply assets. Status is therefore: applied once during construction before audio starts; not called in `prepareToPlay`; not called in realtime callback. |

## Lifecycle notes for `applyPreparedMachineState`

Current path:

1. `MainComponent::MainComponent()` initializes realtime state.
2. `audioEngine.loadDefaultAssetsBeforeAudioStart();`
3. UI components are initialized.
4. `setAudioChannels(channelCount, 2);` starts the JUCE audio device path.
5. `prepareToPlay(...)` runs after audio start/device changes and only prepares buffers/DSP; it does not load/apply assets.
6. `getNextAudioBlock(...)` performs realtime processing without locks/allocations/file access.

This satisfies the realtime rescue requirement that hidden NAM/IR asset discovery and model binding are outside the CoreAudio/JUCE prepare path and outside the audio callback.

## Verification commands and results

### Source/policy evidence

Collected with:

```bash
git ls-files | grep -Ei '(^|/)private_nam_models(/|$)|\.nam$' || true
find . -path ./.git -prune -o -path ./build -prune -o \
  \( -iname '*.nam' -o -path '*private_nam_models*' \) -print | sort || true
grep -RIn "gh release\|macos-latest\|github.ref\|GEILALIZER_BUNDLE_PRIVATE_MODELS" .github CMakeLists.txt || true
grep -RIn "loadDefaultAssetsBeforeAudioStart\|applyPreparedMachineState\|loadDefaultAssetsOffAudioThread\|setAudioChannels\|prepareToPlay" src tests || true
```

Result: no tracked/private asset files; release upload is main-gated; release configure keeps private model bundling OFF; asset apply lifecycle is before `setAudioChannels`.

### Linux local build/test verification

Configuration:

```bash
cmake -S . -B build-final-review \
  -DCMAKE_BUILD_TYPE=Release \
  -DGEILALIZER_BUILD_JUCE_APP=OFF \
  -DGEILALIZER_ENABLE_NAMCORE=OFF \
  -DGEILALIZER_BUNDLE_PRIVATE_MODELS=OFF
```

Build target set:

```bash
cmake --build build-final-review --config Release --parallel --target \
  CoreSmoke NamAdapterSmoke ProductE2E FunctionalSmoke SignalChainProbeSmoke \
  SignalIrAudit TapeMachineBehavior RepositoryPolicySmoke RealtimeCallbackSourceAudit \
  RealtimePlaybackStress MachinePrepareAssetPolicy LimiterBypassClickPolicy
```

CTest result:

```text
100% tests passed, 0 tests failed out of 12
```

Passing tests:

- `CoreSmoke`
- `NamAdapterSmoke`
- `ProductE2E`
- `FunctionalSmoke`
- `SignalChainProbeSmoke`
- `SignalIrAudit`
- `TapeMachineBehavior`
- `RepositoryPolicySmoke`
- `RealtimeCallbackSourceAudit`
- `RealtimePlaybackStress`
- `MachinePrepareAssetPolicy`
- `LimiterBypassClickPolicy`

Linux limitation: this local verification did not build the JUCE macOS `.app` target. The macOS CI app artifact for source commit `fd9c37b` was separately built by GitHub Actions run `27519014310` and inspected in `fd9c37b-artifact-inspection.md`.

## Known non-blocking CI warning

GitHub Actions reports that Node.js-20-based Actions will be forced to Node 24 later. This is not a current release blocker for this review, but should be cleaned up after the architecture/release review.

## Final verdict

PASS. `milestone/realtime-rescue` is approved for the next owner-approved step: merge to `main`, then repeat main checks, run macOS CI on `main`, inspect the main artifact, and only then create/update `macos-latest` from `main`.
