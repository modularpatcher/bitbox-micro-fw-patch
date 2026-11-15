# Changelog

Changes to the Bitbox Micro firmware patch, newest first. Each entry describes
something you would notice: what the module does differently once it is
flashed, what the patcher does differently to your files, or what changed in
the guide you are reading. None of them covers how any of it is built.

## 2.3.12-mod — November 2026

- Get much more from the gate and CV inputs, as
  [Gates and CV](GATES-AND-CV.md) describes.
- On Clip pads only:
  - Sequence or modulate the Live Flip mode, or choose the Hold FX its gates
    trigger, from a CV input, using CV Action. Each semitone selects the next
    mode.
  - Trigger Hold FX by gate using Gate Action, for as long as the gate is high.
    Choose Roll, Pitch Roll, Freeze, Pull Up, Power Down, Brake, Scratch or
    Random.
  - Set a Clip pad's Clip mode (Vinyl, Jungle or Key Lock) on the module, and
    save it with the preset.
- Reassign gate inputs using Gate In, so one gate can trigger several pads.
  This was a common community request. Thanks to ricardopeixoto on Mod Wiggler
  for asking recently.
- Use Round Robin to alternate between pads that share an input or a MIDI
  channel, or to pick one at random on each hit.
- Record into a pad by gate using Rec In. The first gate starts recording, and
  the next stops it. Replace re-records on every gate, and pads that share an input
  take turns.
- Record into whichever pad you select, by gate, using Gate Rec on the Settings
  page. Thanks to ekloiber, whose looper mod gave us the idea for Gate Rec and
  Replace.
- Duck the mix by gate or MIDI note using the new sidechain on the Stock,
  VariMu and Thunder compressors.
- Mute or solo pads from your controller.
- Maintain mutes, solos and the delay's Beat Sync, Ping-pong and filter
  settings across preset changes using Keep CCs.
- Build your controller mappings from the new printable
  [reference card](reference-card.html): every CC and setting on two pages.
- Read and rebuild the patch's C sources, now included in the `character`
  folder. `--reassemble` checks every release against them.

## 2.3.11-mod — November 2026

- Functionality from 2.3.10-mod is unchanged. A new
  [web patcher](patcher.html) just makes installing easier: drop in
  1010music's `MICRO234.zip` and download the patched `MICRO.BIN`, with nothing
  to install. Earlier versions, back to 2.3.5-mod, are under Advanced.
- Otherwise, a maintenance release that makes the module far more tolerant of
  microSD cards that don't meet the SD specification. Recommended for everyone
  on 2.3.10-mod.
- In testing, some cards went to sleep after 20 to 30 seconds of inactivity,
  then took up to 8 seconds to respond to a read request. The SD Association's
  specification requires SDHC and SDXC cards to respond within 100 ms. Stock
  2.3.4 waits only 5 seconds, after which the module can restart repeatedly
  until it's power cycled.
- Any read after the card has been idle, such as loading a preset, can trigger
  it, on stock and on 2.3.10-mod. In 2.3.10-mod, switching to the Custom theme
  also reads the card.
- 2.3.11-mod keeps such cards awake, waits long enough for a slow answer, and
  recovers a stuck read instead of restarting. If a card still stops
  responding, you see "Please insert microSD card": take the card out and put
  it back.
- Enhanced stability when hot swapping Custom themes: put the card back with
  the module running, then set Theme to another theme and back to Custom.
- In testing, a SanDisk Extreme PRO microSDXC, 64 GB (SDSQXCU-064G-GN6MA),
  responded in 2 ms even after 90 seconds of inactivity. An 8 GB SDHC Class 10
  card made in 2025 took 8.3 seconds after a minute of inactivity, causing
  restart loops on stock 2.3.4. On 2.3.11-mod it ran without a single failure.

## 2.3.10-mod — October 2026

- Loops that really follow your tempo, 24 Live Flip modes to re-cut a loop live, and
  turntablist skills.
- Clip modes lock a Clip pad's loop to the clock:
  - Vinyl speeds up and slows down like a turntable, so the pitch rises and
    falls with the tempo.
  - Jungle stretches like an early-'90s sampler, in a Modern or Old Skool
    flavour.
  - Key Lock stretches smoothly with no skips, keeping transients intact.
  - Manual sets a pad's own tempo, off the clock, for slow junglist stretches.
  - In Jungle and Key Lock the pitch is free: sequence notes into the pad and
    the loop plays them, in time.
- Live Flip modes: 24 ways to re-cut a loop live from one encoder, always in time:
  repeats, reshuffles, chops, reverse swells and harmonies. The chops find
  where the sound changes, so vocals and chords work as well as breaks.
- Hold buttons: Pull Up, Power Down, Brake, Baby Scratch, Roll, Pitch Roll
  and Freeze. Let go and the loop drops back in time.
- Three Airwindows compressors beside the stock one: Logical4, VariMu and
  Thunder. Auto makeup gain is improved, with a new Hot setting.
- Themes: pick one of five, or roll your own in the theme maker, which shows
  how readable each colour is.
- The Character chain and compressor settings survive a power-off.
- Preset BPM runs from 1 to 255, up from 40 to 250.
- Lighter on the CPU, same sound.
- Everything from 2.3.9-mod is unchanged and still there.

## 2.3.9-mod — October 2026

- Sixteen Airwindows effects on the master bus, and a pop-up that shows what
  every CC in the patch just did.
- The Character chain: four effect slots across the whole mix, each with an
  Amount, a Tone and four effects. Ports of Chris Johnson's Airwindows
  plugins, described in depth on
  [the Airwindows site](https://www.airwindows.com/).
  - Destruction, lo-fi and breakage: Pockey2, DeRez3, Ring Mod, Fracture2.
  - Overdrive, warm tube to dirty harmonics: Tube2, Drive, Mackity, Huge.
  - Colour, consoles, tape and a transformer: Channel9, IronOxide5, ToTape8,
    Coils2.
  - Saturation, smooth weight and density: PurestDrive, Spiral2, Density3,
    Mojo.
- Pop-ups: every patch CC shows what it changed, in real units. The clock gets
  them too.
- A CPU bar along the bottom of the screen, and a new OSD row that picks the
  pop-ups, the bar, both or neither.
- Preset BPM: set a preset's tempo on the module and save it with the preset.
  With Clk Loss on HOLD, it also captures a clock's tempo.
- Auto makeup for the compressor on CC 46, so the peaks stay level as you
  compress harder.
- Keep CCs, a Settings row that puts your per-pad CCs back after a preset load.
- Fixed: MIDI notes could change compressor, EQ or delay settings.
- Fixed in the guide: the EQ comes before the compressor, not after it.
- Everything from 2.3.8-mod is unchanged and still there.

## 2.3.8-mod — October 2026

- Three new rows on the module's own Settings screen, the first additions that
  are not on a CC.
- Flip UI turns the screen, touch, buttons and knobs over together, for a
  module mounted upside down.
- Clk Loss keeps the last tempo when the clock drops out, instead of falling
  back to 120. Beat-synced delays and LFOs stay where they were, through preset
  changes, until a clock returns.
- ProgChange gains NEW ONLY, which ignores a Program Change for the preset
  already loaded. A sequencer that resends the same request every pattern no
  longer reloads the preset and swallows its first notes.
- All three are on page 2 of TOOLS, Settings, and the module saves them with its
  own settings. A new settings page in the guide covers each one.
- Everything from 2.3.7-mod is unchanged and still there.

## 2.3.7-mod — September 2026

- A master EQ and filter, which the module has never had. Ten algorithms: a DJ filter, a dual
  cut, a band pass, a three-band mixer EQ, tone and filter together, a tilt, a
  notch, a peak, a phaser, and a formant filter.
- The installation and user guide has been completely redeveloped, hopefully
  making it easier to take in, and easier to reach just the part you need.
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
