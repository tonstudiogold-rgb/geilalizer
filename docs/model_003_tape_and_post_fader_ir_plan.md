# Model 003 tape and post-fader IR plan

## Tape model received

Source: user-supplied Dropbox folder `Modell 3`.

Selected file:

- `STUDER C37 TUBE TAPE 15 IPS.nam`
- Local private path: `private_nam_models/model_003/STUDER C37 TUBE TAPE 15 IPS.nam`
- Size observed after download: 297397 bytes

## Updated target channel order

Input
→ Input Gain / IR drive
→ Selected hidden 1073 IR/preamp colour
→ `NEVE 5315 ANALOG CONSOLE.nam`
→ `STUDER C37 TUBE TAPE 15 IPS.nam`
→ Hidden fader model/mapping
→ Visible Fader gain
→ Hidden post-fader SSL 4000 G IR mapping
→ Pan
→ Stereo Sum

## Post-fader IR files received

Source: user-supplied Dropbox folder `Modell 4`.

Local private path: `private_nam_models/model_004_post_fader_irs/`

Files:

- `4000 G Channel 0db_dc.wav` — stereo, 96 kHz, 24-bit, 55561 frames, ~578.76 ms
- `4000 G Channel 5db_dc.wav` — stereo, 96 kHz, 24-bit, 53584 frames, ~558.17 ms
- `4000 G Channel 10db_dc.wav` — stereo, 96 kHz, 24-bit, 55330 frames, ~576.35 ms

These are SSL 4000 G channel IRs at three drive/calibration points. Treat them as a hidden fader/console-return coloration family, not as a user-facing selector.

## Post-fader IR distribution rule

The three SSL 4000 G post-fader IRs are selected by the visible fader position, not by track role and not by a fixed channel pattern.

Fader-position mapping:

- Lower third of fader travel → `4000 G Channel 0db_dc.wav`
- Middle third of fader travel → `4000 G Channel 5db_dc.wav`
- Upper third of fader travel → `4000 G Channel 10db_dc.wav`

This means the hidden IR follows the musical fader position:

- low fader = cleaner 0db path
- normal/mid fader = 5db SSL channel colour
- high fader = pushed 10db SSL channel colour

Implementation note:

- The mapping should be based on normalized fader position/travel, not arbitrary channel number.
- If the fader UI range is represented in dB, convert the current fader value to normalized fader travel first, then choose the lower/middle/upper third.
- The selection can be stepped; if audible clicks occur while moving the fader, add a short crossfade/smoothing between IR engines.
- No user-facing IR selector is exposed.

Stereo pair rule:

- If an imported stereo file is split to adjacent mono channels and both faders are linked or moved as a pair, both sides naturally receive the same IR third.
- If L/R faders are intentionally different, their post-fader IRs may differ because the mapping follows each actual fader position.

## Important gain rule

The SSL 4000 G IR coloration stage is after the visible fader gain in the DSP graph, matching the user's “IRs after the fader” wording. It remains hidden and automatically follows lower/middle/upper fader position.
