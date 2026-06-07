# model_001 IR analysis

Source: `/opt/data/geilalizer/private_nam_models/model_001/`

Reference image used for revised categorization: AMS Neve 1073 Classic product image, `https://www.ams-neve.com/wp-content/uploads/2022/01/1073-Classic-H-Main-Black-OP2-scaled.jpg`.

These files are impulse responses, not NAM models. They are linear WAV IRs and therefore do not provide dynamic input-dependent nonlinearity by themselves.

## File family

All WAV files are:

- stereo WAV
- 48 kHz
- 24 bit
- L/R correlation: 1.0, effectively dual-mono rather than stereo-width IRs

## Revised categorization from the real 1073 front panel

The original 1073 Classic front panel changes the interpretation of the filenames. The numbers on the module are not arbitrary “amounts”; they visually match the stepped mic/line gain switch.

Visible/known 1073 logic:

- mic gain is stepped around 20, 30, 40, 50, 60, 70, 80 dB
- line gain sits on the other side of the same red rotary control, around -20 to +10 dB line positions
- the EQ section is separate from the gain switch
- the famous high shelf is fixed at 12 kHz with selectable boost/cut amount
- the HPF is a separate filter stage, not represented by these file names

So the IR filenames should be treated primarily as 1073 gain-switch capture positions, not as generic “more drive” presets.

Important nuance: because these are IRs, they do not model real nonlinear gain staging. `Neve 80.wav` is not dynamically saturating harder than `Neve 40.wav` inside our software. It is a static capture taken at/labelled after that 1073 gain position. Still, the capture positions are musically meaningful.

## Standard gain-position series

Files:

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

### New machine categories

```text
Line/Utility Clean:       Neve 20, Neve 30
Console Insert Clean:     Neve 40, Neve 45
Classic 1073 Record Path: Neve 50, Neve 55
Vocal/Instrument Push:    Neve 60, Neve 65
Red-Knob Hero Push:       Neve 70
Special Effect / Extreme: Neve 75, Neve 80
```

### Musical interpretation

#### Line/Utility Clean: Neve 20, Neve 30

This is the least “recording-through-a-1073” feeling group.

Use for:

- channels that should stay tight
- bass DI if low end must remain controlled
- kick/snare when transient weight matters more than color
- stems that already sound finished

Do not use as the main Geilalizer default if the product promise is “immediately more expensive”. It may be too polite.

#### Console Insert Clean: Neve 40, Neve 45

This is the first useful console-color zone.

Use for:

- full-band safety mode
- drums when cymbals must not smear
- dense productions with many stems
- subtle “ran through a desk” coloration

This is the best conservative fallback if a source gets cloudy with higher positions.

#### Classic 1073 Record Path: Neve 50, Neve 55

This should now be considered the real default zone.

Reason:

- on the physical 1073, 50 dB is a normal serious mic-pre recording position, not an extreme setting
- likely gives recognizable 1073 tone without jumping straight into the 70/80 hero zone
- measured tails are still controlled compared with 70/80

Recommended first product default:

`Neve 55.wav`

Recommended safer default:

`Neve 50.wav`

#### Vocal/Instrument Push: Neve 60, Neve 65

This is the “expensive forward source” zone.

Use for:

- lead vocal
- guitars
- synth hooks
- snare character
- drum room/parallel color
- anything that should come forward and feel recorded through iron

This is no longer the global default recommendation. It is a stronger per-channel/source flavor.

#### Red-Knob Hero Push: Neve 70

The product image clearly shows the red gain knob in the 70-ish area. Visually and emotionally, this is the iconic “1073 being driven” hero position.

Use for:

- hidden “make it expensive” button internally
- parallel drum density
- vocal attitude mode
- guitar/synth attitude
- selected stems that need character

Do not use on all 24 channels by default. Across many stems it may accumulate too much tail/low-mid thickness.

#### Special Effect / Extreme: Neve 75, Neve 80

This should be treated as extreme/gimmick/parallel, not normal console path.

Use for:

- parallel crush/color bus
- lo-fi or “printed through hot hardware” effect
- loops and single stems
- special machine flavor

Avoid as a full-mix default. The measured long tails around 70/75/80 make this dangerous on dense 24-stem sessions.

## 12 kHz boost variants

Files:

- Neve 50 12k 6db boost.wav
- Neve 60 12k 6db boost.wav
- Neve 70 12k 6db boost.wav
- Neve 80 12k 6db boost.wav

The OG 1073 context makes these easier to name: they are not just “bright variants”; they correspond to the 1073 high-shelf idea, specifically the famous 12 kHz air band with +6 dB boost.

Revised mapping:

```text
1073 Air Default:     Neve 50 12k 6db boost.wav
1073 Air Forward:     Neve 60 12k 6db boost.wav
1073 Air Hero:        Neve 70 12k 6db boost.wav
1073 Air Extreme/FX:  Neve 80 12k 6db boost.wav
```

Use for:

- dull vocals
- dark guitars
- dark synths/keys
- overheads only if not brittle
- special “open the curtain” mode

Avoid as default on:

- hi-hats
- already-bright cymbals
- sharp vocals
- harsh guitars
- dense full mixes

Recommended hidden “Air” setting:

`Neve 50 12k 6db boost.wav` or `Neve 60 12k 6db boost.wav`

Avoid making `Neve 70/80 12k` the default Air setting. Those read more like hero/FX positions.

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

This supports the 1073-front-panel interpretation: lower/mid gain positions are practical channel defaults; 70/80 positions should be treated as hero/parallel/extreme because their static captured response is much more extended.

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

## Updated product recommendation

Previous recommendation was `Neve 60.wav`. After looking at the real 1073 Classic front panel, revise this:

Default channel IR:

`Neve 55.wav`

Safer dense-session default:

`Neve 50.wav`

Subtle clean fallback:

`Neve 45.wav`

Hero/pushed channel color:

`Neve 70.wav`

Hidden Air mode:

`Neve 50 12k 6db boost.wav` or `Neve 60 12k 6db boost.wav`

Internal categories:

```text
LineUtilityClean:        Neve 20, 30
ConsoleInsertClean:      Neve 40, 45
Classic1073RecordPath:   Neve 50, 55
VocalInstrumentPush:     Neve 60, 65
RedKnobHeroPush:         Neve 70
ExtremeParallelFx:       Neve 75, 80
AirDefault:              Neve 50 12k 6dB boost
AirForward:              Neve 60 12k 6dB boost
AirHero:                 Neve 70 12k 6dB boost
AirExtremeFx:            Neve 80 12k 6dB boost
```

## Important limitation

IRs are not input-dependent like NAM. Input Gain can feed an IR louder, but the IR itself will not saturate or compress differently. For “Input fährt die Maschine an”, we still need actual NAM/nonlinear model files.
