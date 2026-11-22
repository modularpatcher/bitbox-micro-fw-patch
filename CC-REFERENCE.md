# Bitbox Micro Firmware: CC Reference

This page lists every CC, its channel and the pad types that answer it, and what
to check when one seems dead. [Pad controls](PAD-CONTROLS.md) says what each one
does, [Clip modes](CLIP-MODES.md) covers the Clip pad's tempo, Live Flip modes and
hold buttons, and [devices](DEVICES.md) covers the three that act on the whole
mix. [Gates and CV](GATES-AND-CV.md) covers what gates and CV can do to a pad,
and [External FX](EXTERNAL-FX.md) the sends to your own effects.
To build your controller mappings, print the
[reference card](reference-card.html): every CC and setting on two pages.

## Which CC, Which Channel

### Which Pad Types Work

| Control | CC | Sample | Granular | Clip | Slicer | Multi | Rec |
| ------- | -- | :----: | :------: | :--: | :----: | :---: | :-: |
| Sends | 84, 85 | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| Reverse | 86 | ✅ | ✅ | — | — | ✅ | ❌ |
| Granular | 79–83 | — | ✅ | — | — | — | — |
| Loop | 88, 89 | ✅ | ✅ | — | — | ✅ | ❌ |
| Play Thru | 90 | — | — | — | ✅ | — | ❌ |
| Sustain + LFO | 87, 91–93 | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| Stop | 120/123 | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| Mute, Solo | 94, 95 | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| Dub Throw | 103, 106 | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| Clip modes, Live Flip modes, hold buttons | 78, 102, 104, 105, 107, 112–119 | — | — | ✅ | — | — | ❌ |

**Multi-Sample**: this is a Sample pad with a folder of WAVs loaded into it, not
a separate mode, so it behaves exactly like a Sample pad. Switch it to another
mode and it plays only the first file.

The gaps are the module's, not the patch's:

- **Reverse**: Clip and Slicer pads have no reverse setting.
- **Play Through**: Slicer pads only, since only they have slices.
- **Loop Mode and Loop Crossfade**: Clip and Slicer pads loop their own way,
  with no equivalent setting.
- **Recorded-sample pads**: these have no FX sends in stock firmware. The delay
  and reverb knobs on the FX screen do nothing on them either, and Mute and Solo
  don't reach them.

Sustain and the LFO controls work on Clip and Slicer pads too.

The three devices are not in the table. They act on every pad routed to Out 1/2,
whatever its type.

### MIDI CC Map

| CC | Controls | Channel | Values | Encoder |
| ---- | -------- | ---------- | ------ | ------- |
| **14–17** | Character Amount: Destruction, Overdrive, Colour, Saturation | **1 only** | `0` off … `127` full | linear |
| **18–21** | Character Tone, same order | **1 only** | `0`–`127`, starts at centre | linear |
| **22–25** | Character pick effect, same order | **1 only** | four bands, one per effect | stepped, 4 |
| **26–29** | Character next effect, same order | **1 only** | any value above `0` | button |
| **40** | Compressor on/off | **1 only** | `≥64` on, `<64` off | button |
| **41** | Compressor Threshold | **1 only** | −40 … 0 dB | linear |
| **42** | Compressor Ratio | **1 only** | 1:1 … 20:1 | linear |
| **43** | Compressor Attack | **1 only** | 0.5 … 100 ms | **exponential** |
| **44** | Compressor Release | **1 only** | 10 ms … 1 s | **exponential** |
| **45** | Compressor Makeup gain | **1 only** | −36 … +36 dB | linear |
| **46** | Compressor auto makeup | **1 only** | any value above `0` steps Off, Normal, Hot | button |
| **47** | Compressor type | **1 only** | four bands: Stock / Logical4 / VariMu / Thunder | stepped, 4 |
| **48** | Compressor next type | **1 only** | any value above `0` | button |
| **49** | Compressor sidechain, SC In | **1 only** | ten bands: Self / Gate 1–8 / MIDI 16 | stepped, 10 |
| **50** | EQ mode | **1 only** | ten bands, one per mode | stepped, 10 |
| **51** | EQ encoder 2 | **1 only** | `0`–`127`, centre neutral | linear |
| **52** | EQ encoder 3 | **1 only** | `0`–`127`, centre neutral | linear |
| **53** | EQ encoder 4 | **1 only** | `0`–`127`, centre neutral | linear |
| **54** | EQ encoder 2 push | **1 only** | `≥64` held, `<64` released | button |
| **55** | EQ encoder 3 push | **1 only** | `≥64` held, `<64` released | button |
| **56** | EQ encoder 4 push | **1 only** | `≥64` held, `<64` released | button |
| **57** | EQ master bypass | **1 only** | `≥64` out, `<64` in | button |
| **59** | EQ filter slope | **1 only** | `≥64` is 24 dB, `<64` is 12 dB | button |
| **63** | External FX Return Lvl | **1 only** | −57.5 … +6 dB, `115` is 0 dB | linear |
| **78** | Baby Scratch | pad channel | `≥64` held, `<64` released | button |
| **79** | Granular Density | pad channel | `0`–`127` | linear |
| **80** | Granular Grain Size | pad channel | `0`–`127` | linear |
| **81** | Granular Window | pad channel | `0`–`127` | linear |
| **82** | Granular Scatter | pad channel | `0`–`127` | linear |
| **83** | Granular Pan Rnd | pad channel | `0`–`127` | linear |
| **84** | Delay send | pad channel | `0`–`127`, same curve as the knob | linear |
| **85** | Reverb send | pad channel | `0`–`127`, same curve | linear |
| **86** | Reverse | pad channel | `≥64` on, `<64` off | button |
| **87** | Envelope Sustain | pad channel | `0`–`127` | linear |
| **88** | Loop Mode | pad channel | three bands: None / Forward / Bidirectional | stepped, 3 |
| **89** | Loop Crossfade | pad channel | `0`–`127` | linear |
| **90** | Play Through | pad channel | `≥64` on, `<64` off | button |
| **91** | LFO Wave | pad channel | ten bands across the waveform list | stepped, 10 |
| **92** | LFO Beat Sync | pad channel | `≥64` on, `<64` off | button |
| **93** | LFO Retrigger | pad channel | `≥64` on, `<64` off | button |
| **94** | Mute | pad channel | `≥64` muted, `<64` plays | button |
| **95** | Solo | pad channel | `≥64` soloed, `<64` released | button |
| **102** | Brake | pad channel | `≥64` held, `<64` released | button |
| **103** | Dub Throw, delay | pad channel | `≥64` throws, `<64` lets go | button |
| **104** | Freeze | pad channel | `≥64` held, `<64` released | button |
| **105** | Pull Up | pad channel | `≥64` held, `<64` released | button |
| **106** | Dub Throw, reverb | pad channel | `≥64` throws, `<64` lets go | button |
| **107** | Power Down | pad channel | `≥64` held, `<64` released | button |
| **108** | Delay beat-sync | any (global) | `≥64` on, `<64` off | button |
| **109** | Delay ping-pong | any (global) | `≥64` on, `<64` off | button |
| **110** | Delay filter on/off | any (global) | `≥64` on, `<64` off | button |
| **111** | Delay filter width | any (global) | `0`–`127` | linear |
| **112** | Clip mode: Stock, Vinyl, Jungle, Key Lock | pad channel | any value above `0` steps to the next | button |
| **113** | Clip tempo: clock or Manual | pad channel | any value above `0` switches | button |
| **114** | Manual tempo | pad channel | 1 … 255 BPM | linear |
| **115** | Jungle cycle | pad channel | short to long | **exponential** |
| **116** | Jungle flavour: Modern or Old Skool | pad channel | any value above `0` switches | button |
| **117** | Live Flip mode | pad channel | 25 bands: Off, then the 24 modes | stepped, 25 |
| **118** | Roll | pad channel | `≥64` held, `<64` released | button |
| **119** | Pitch Roll | pad channel | `≥64` held, `<64` released | button |
| **120** | Stop this pad — All Sound Off | pad channel | any value; releases the note | button |
| **123** | Stop this pad — All Notes Off | pad channel | any value; same as 120 | button |

There is nothing to set up on the module. Each control has a fixed CC that works
on every preset, with no MIDI Learn and no modulation slot. Set your controller
to send these CCs on the channel shown and you are playing. Every one but the
pad stop, Mute and Solo opens a [pop-up](SETTINGS.md#the-pop-up). Mute and Solo
show on the Mixer screen instead.

### Which Channel to Send On

- **CC 79 to 95, and 120/123**: per pad. Send them on the channel the pad
  listens on, the one you trigger it with. Eight pads on eight channels answer
  independently.
- **CC 78, 102, 104, 105, 107 and 112 to 119**: the [Clip modes](CLIP-MODES.md),
  per Clip pad, on its channel in the same way.
- **CC 103 and 106**: [Dub Throw](EXTERNAL-FX.md#dub-throw), per pad, on its
  channel. Set them up as momentary buttons.
- **CC 108 to 111**: the delay, which the whole preset shares. Send them on
  any channel.
- **CC 14 to 29**: the [Character chain](CHARACTER.md), channel 1 only.
- **CC 40 to 49**: the [compressor](COMPRESSOR.md), channel 1 only. All four
  types share CC 41–45. Each uses them differently.
- **CC 50 to 57, and 59**: the [EQ and filter](EQ-FILTER.md), channel 1 only.
- **CC 63**: [External FX](EXTERNAL-FX.md)'s Return Lvl, channel 1 only.

If a device ignores everything, put your controller on MIDI channel 1.

## Setting Up Your Controller

Do this once. Every preset behaves the same after that.

### Recommended Controller

The per-pad CCs go out on each pad's own channel, so your controller must set
the MIDI channel per control, not just per device. Many cannot.

The **[Neuzeit Instruments Drop](https://www.neuzeit-instruments.com/products/drop/)**
can. Each control sends on its device's default channel or on any channel you
pick. One Drop device slot covers all eight pads plus the global controls,
leaving the other seven slots free. Any controller that sets the channel per
control works as well.

### Encoder Types

The **Encoder** column in the [MIDI CC map](#midi-cc-map) suggests how to set up
each control. It changes the feel, not what the module does. Almost everything
wants a plain linear encoder. These are the exceptions:

**Attack and Release**: use a curve that **starts gradually and then ramps up**,
so the useful fast settings are not crammed into the first few percent. Vendors
name it differently, so go by shape. On the Drop it is **Exponential minus**.
The plus variant makes the crowding worse. At halfway, Attack should send about
`8` and Release about `12`. If either sends about `64`, the curve is still
linear.

**Ratio**: linear is fine. The musical settings sit low, though, with 2:1 at
about 5% and 4:1 at about 16%, so move slowly down there.

**Loop Mode, LFO Wave and Character pick effect**: these are lists, not sweeps.
Use a stepped encoder with exactly this many steps. Any other count skips
entries or hits one twice.

| Control | Steps | The list |
| ------- | ----- | -------- |
| **Loop Mode** | **3** | None / Forward / Bidirectional |
| **LFO Wave** | **10** | the ten waveforms, in the module's order |
| **Character pick effect** | **4** | the bank's four effects, in order |

Without stepped encoders, a slow linear sweep still works. Find the bands by
ear.

**Switches**: set up anything marked `≥64 on` as a button sending 0 and 127.

### Getting the Full Range

This is about CCs you map yourself in a preset. The patch's own CCs always
cover their full range.

A CC mapped **through a preset** moves its parameter *upward from the saved
value*. Save a parameter halfway and the CC only reaches the top half. It looks
exactly like a broken mapping.

Before you save, set each parameter you will drive to its **minimum**:

| Parameter | Set it to |
| --- | --- |
| Delay time, Beat Sync on | `1/64`, the first entry |
| Delay time, Beat Sync off | `0%` |
| Delay feedback | `0` |
| Delay cutoff | `0`, and see the note below |
| Reverb decay, damping, pre-delay | `0` |

Delay cutoff's minimum, `0`, means fully low-passed, though the screen shows it
as negative. The delay sounds very dark until the CC brings it up.

Your preset will sound different before any MIDI arrives. Feedback at zero is a
single slap, not a delay. Do this only for parameters you really will drive.

### Use 14-Bit CC

Use it for CCs you map yourself in a preset, such as filter cutoff and
resonance. The module still lands on only 128 positions, because it discards
the second half of the pair. But a 14-bit sweep sends far fewer messages, so the
parameter moves smoothly instead of lurching.

Keep the patch's CCs **7-bit**. A 14-bit pair sends its second half 32 numbers
higher. Something else lives there. For the Character chain, that is auto makeup
or the EQ. For CC 50, it is a pad's granular window. Granular Density's second
half lands on the delay's filter width, and Loop Crossfade's on Reset All
Controllers.

## When a Control Seems Dead

Two tests settle most cases. Look for the pop-up, then try the encoder. Things
the module cannot do are in [limitations](LIMITATIONS.md).

### Is There a Pop-Up?

A pad pop-up appears only when a pad on that channel took the CC. If none
appears, check the channel. Then check the pad type. For the three devices, no
pop-up means nothing arrived on channel 1.

A pop-up with no sound means the CC arrived and the cause is elsewhere, such as
an LFO pointed at nothing, or Scatter with Density up.

No pop-up opens for the value already showing, or with OSD set to OFF or CPU.

On a Clip pad with CV Action set to Flip In, CC 117 leaves the Live Flip mode
alone. Its pop-up shows the input instead, such as `CV 3: Rotate`.

### Does the Encoder Do It?

Turn the parameter by hand. If the encoder changes it, your MIDI is fine.
The fault is elsewhere. If it does not, the module cannot do it, and no MIDI
will change that. The stop pair, CC 120 and 123, is the one control with no
encoder.
