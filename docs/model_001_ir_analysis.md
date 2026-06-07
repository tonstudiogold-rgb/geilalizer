# model_001 IR analysis

Source: `/opt/data/geilalizer/private_nam_models/model_001/`

These files are impulse responses, not NAM models. They are linear WAV IRs and therefore do not provide dynamic input-dependent nonlinearity by themselves.

## File family

All WAV files are:

- stereo WAV
- 48 kHz
- 24 bit
- L/R correlation: 1.0, effectively dual-mono rather than stereo-width IRs

## Practical categories

### Standard level series

- Neve 20.wav
- Neve 30.wav
- Neve 40.wav
- Neve 45.wav
- Neve 50.wav
- Neve 55.wav
- Neve 60.wav
- Neve 65.wav
- Neve 70.wav
- Neve 75.wav
- Neve 80.wav

The number appears to be a preamp/drive/level setting. As the number rises, the IRs generally get longer and show more tail/energy.

Suggested musical mapping:

- 20-45: clean/tight/short
- 50-60: normal/record-ready/default zone
- 65-70: thicker/stronger color
- 75-80: most pushed/heaviest color

### 12 kHz boost variants

- Neve 50 12k 6db boost.wav
- Neve 60 12k 6db boost.wav
- Neve 70 12k 6db boost.wav
- Neve 80 12k 6db boost.wav

These are brighter variants with a clear high/air lift, suitable only where extra top-end is wanted.

Suggested musical mapping:

- use sparingly on dull sources
- avoid as default on already bright drums/vocals/cymbals
- useful as hidden “air” machine flavor, not exposed as user EQ

## Measured structural notes

Peak levels:

- Neve 20-50 standard: about -0.1 dBFS peak
- Neve 50 boost and above: about -1.0 dBFS peak

Tail/decay groupings, approximate 60 dB decay after impulse peak:

- 20-50 standard: about 10 ms, tight
- 50 boost / 55 / 60: about 20-43 ms, medium
- 65: about 70 ms
- 70 / 75 / 80 standard: about 124-126 ms, long
- 80 12k boost: about 246 ms, longest/most extended

## Recommended hidden machine use

Do not integrate these as NAM. Create a separate hidden IR stage, e.g. `HiddenIrAdapter`, after hidden NAM and before fader/pan:

Input
→ Input Gain
→ Hidden NAM Adapter
→ Hidden 1073 IR Adapter
→ Fader
→ Pan
→ Stereo Sum
→ Output Trim
→ Safety Limiter

Suggested first default IR for testing: `Neve 60.wav` or `Neve 50.wav`.

Reason:

- still controlled and not too long
- probably the most “normal record path” zone
- avoids extreme 12k boost and long 75/80 tails as default

Suggested internal categories:

```text
CleanTight:       Neve 20, 30, 40, 45
DefaultRecord:    Neve 50, 55, 60
PushedColor:      Neve 65, 70
HeavyColor:       Neve 75, 80
BrightDefault:    Neve 50/60 12k 6dB boost
BrightPushed:     Neve 70/80 12k 6dB boost
```

## Important limitation

IRs are not input-dependent like NAM. Input Gain can feed an IR louder, but the IR itself will not saturate or compress differently. For “Input fährt die Maschine an”, we still need actual NAM/nonlinear model files.
