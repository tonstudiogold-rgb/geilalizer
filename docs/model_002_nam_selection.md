# Model 002 NAM selection

Source: user-supplied Dropbox folder `Modell 2`.

Selected file:

- `NEVE 5315 ANALOG CONSOLE.nam`
- Local private path: `private_nam_models/model_002/NEVE 5315 ANALOG CONSOLE.nam`
- Size observed after download: 407934 bytes

## Product role

This is the first fixed hidden channel/line NAM candidate for the 24-Spur Standalone Geilalizer.

It is treated as the per-channel Neve/console line stage after the selected hidden IR/preamp colour and before the later tape and fader coloration stages. It is not a user-loadable NAM, not a masterbus model, and not a visible model selector.

## Required per-channel order

Input
→ Input Gain / IR drive
→ Selected hidden 1073 IR/preamp colour
→ `NEVE 5315 ANALOG CONSOLE.nam`
→ Future fixed hidden tape model
→ Future hidden fader model / individually mapped fader files
→ Visible Fader gain
→ Hidden post-fader SSL 4000 G IR mapping
→ Pan
→ Stereo Sum

## Integration constraint

The `.nam` asset remains under `private_nam_models/`, which is intentionally git-ignored. Source code may reference the product role and adapter seam, but should not expose user-facing NAM loading or selection.
