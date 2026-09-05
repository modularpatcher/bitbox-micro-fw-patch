# Changelog

Changes to the Bitbox Micro firmware patch, newest first. Each entry describes
something you would notice: what the module does differently once it is
flashed, or what the patcher does differently to your files. Nothing about how
either one is built.

## 2.3.7-mod — September 2026

- A master EQ and filter, which the module never had, on the sum of everything
  routed to Out 1 and 2. Ten algorithms on one encoder: a DJ filter, a dual
  cut, a band pass, a three-band mixer EQ, tone and filter together, a tilt, a
  notch, a peak, a phaser, and a vowel morpher.
- Every control is neutral at its centre in every mode, so three centred knobs
  is an EQ doing nothing at all, and the mode dial can be swept without hearing
  it.
- Sixteen of the encoder pushes do something. A kill takes a range out of the
  mix, an open lets a band stop acting, a slam forces a control to its maximum.
  All of them momentary, and letting go hands back the knob's own position
  without a jump.
- A master bypass on CC 57 takes the whole thing out, and puts it back.
- Nine CCs, 50 to 57 and 59, on MIDI channel 1 beside the compressor.
- Everything from 2.3.6-mod is unchanged and still there.

## 2.3.6-mod — September 2026

- The module's master bus compressor is now reachable over MIDI. All five
  parameters that were previously fixed and invisible are on the table:
  Threshold, Ratio, Attack, Release and Makeup gain.
- It arrives configured as a glue compressor, close to a classic SSL-style bus
  compressor recipe — 2:1 ratio, a slow 30 ms attack, a 300 ms release — ready
  to use with no setup, and adjustable from there.
- The compressor answers on MIDI channel 1 only, on CC 40 through 45: the five
  parameters, plus on/off on CC 40. The rest of the patch is per-pad, on each
  pad's own channel. That is deliberate, so a stray message from elsewhere in a
  rack cannot move it by accident.
- Everything from 2.3.5-mod is unchanged and still there.

## 2.3.5-mod — August 2026

- Per-pad Delay send and Reverb send, so each pad can sit in its own place in
  the mix rather than sharing one blend across the whole preset.
- Per-pad Reverse.
- Per-pad Envelope Sustain, later extended to reach Clip and Slicer pads as
  well as the sample-based ones.
- Per-pad Loop Mode and Loop Crossfade.
- Five granular controls that had no way to be touched before: Density, Grain
  Size, Window, Scatter and Pan Random.
- Global delay Beat Sync, Ping-Pong, Filter and Filter Width, shared across the
  whole preset.
- LFO Wave, Beat Sync and Retrigger, reaching every pad type — Sample,
  Multi-Sample, Clip, Slicer and Granular — not only the ones that had them at
  first.
- Per-pad stop for long clips, one pad at a time. Stopping a pad now releases
  the voice rather than cutting it dead.
- The whole control set was regrouped onto a cleaner CC map, so some numbers
  moved even where the control itself did not change.
- The splash screen stopped crediting 1010music on a patched image.
- The patcher stopped being able to overwrite a user's own stock firmware
  file.
- The Scatter control's threshold now reads on the scale people actually
  think in, rather than an internal one.
