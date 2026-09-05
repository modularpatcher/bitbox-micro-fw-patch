# Bitbox Micro Firmware: CC Reference

The numbers. Which CC does what, which channel it goes on, which pad types
answer it, and what to check when one appears to do nothing.

See [pad controls](PAD-CONTROLS.md) for what each control does, and
[devices](DEVICES.md) for the two that act on the whole mix. Start at
[the overview](README.md) if you have not installed it yet.

## Which CC, Which Channel

Everything you need to get your controller talking to the module: what works
on which pad type, every CC number in one table, and which channel to send
each one on. Three tables. That is the whole section.

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

**Multi-Sample**: Sample mode, not a separate pad mode. It is a Sample pad
with a folder of WAVs loaded into it. The manual says so twice: loading a
directory "will automatically be changed to Sample", and switching a
multi-sample pad to any other mode leaves it playing only the first file in
the folder. Anything proven on a Sample pad is proven there too.

Two of the gaps belong to the module, not the patch. The Bitbox runs pads
through two different bits of firmware: one for Sample, Multi-Sample and
Granular, another for Clip and Slicer, and they do not have identical
features.

**Reverse**: does not exist in the Clip/Slicer half, since there is no
setting for CC 86 to point at, so it can never work on those pads. **Play
Through** is narrower still: a **Slicer** parameter only, since a Clip pad
has no slices to carry on past. CC 90 does nothing anywhere except a Slicer
pad.

Loop Mode and Loop Crossfade are the same story: Clip and Slicer pads handle
looping their own way and have no equivalent setting.

Sustain and the three LFO controls, by contrast, **do** work on every pad type.
The Clip/Slicer half keeps its parameters in a separate place with two copies
of everything, so those arms hand the value to the firmware's own setter
rather than writing it directly.

**Recorded-sample pads**: no FX sends in stock firmware, either. The delay
and reverb knobs on the FX screen do nothing on one, even with content
recorded, and that is stock behaviour. Confirmed on hardware. Only two
routines send audio to the FX bus, both patched here, so **this covers every
pad type that can use the FX sends.** Giving recorded-sample pads sends
would mean building that capability from scratch, which this does not attempt.

The compressor is not in this table: pad type is irrelevant to it. It sits
on the Out 1/2 bus, post-mix, so it acts on every pad routed there and on
nothing routed to Out 3–6.


### MIDI CC Map

| CC | Controls | Channel | Values | Encoder |
| ---- | -------- | ---------- | ------ | ------- |
| **40** | Compressor on/off | **1 only** | `≥64` on, `<64` off | button |
| **41** | Compressor Threshold | **1 only** | −40 … 0 dB | linear |
| **42** | Compressor Ratio | **1 only** | 1:1 … 20:1 | linear |
| **43** | Compressor Attack | **1 only** | 0.5 … 100 ms | **exponential** |
| **44** | Compressor Release | **1 only** | 10 ms … 1 s | **exponential** |
| **45** | Compressor Makeup gain | **1 only** | −36 … +36 dB | linear |
| **50** | EQ mode | **1 only** | ten bands, one per mode | stepped, 10 |
| **51** | EQ encoder 2 | **1 only** | `0`–`127`, centre neutral | linear |
| **52** | EQ encoder 3 | **1 only** | `0`–`127`, centre neutral | linear |
| **53** | EQ encoder 4 | **1 only** | `0`–`127`, centre neutral | linear |
| **54** | EQ encoder 2 push | **1 only** | `≥64` held, `<64` released | button |
| **55** | EQ encoder 3 push | **1 only** | `≥64` held, `<64` released | button |
| **56** | EQ encoder 4 push | **1 only** | `≥64` held, `<64` released | button |
| **57** | EQ master bypass | **1 only** | `≥64` out, `<64` in | button |
| **59** | EQ filter slope | **1 only** | `≥64` is 24 dB, `<64` is 12 dB | button |
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
| **108** | Delay beat-sync | any (global) | `≥64` on, `<64` off | button |
| **109** | Delay ping-pong | any (global) | `≥64` on, `<64` off | button |
| **110** | Delay filter on/off | any (global) | `≥64` on, `<64` off | button |
| **111** | Delay filter width | any (global) | `0`–`127` | linear |
| **120** | Stop this pad — All Sound Off | pad channel | any value; releases the note | button |
| **123** | Stop this pad — All Notes Off | pad channel | any value; same as 120 | button |

Every control the patch adds, on thirty-six numbers. Write them down somewhere.

**Every one of them has been heard on the module**: nothing in this table
rests on reading the code and hoping.

There is nothing to set up. Every one of these is fixed in the firmware and
live on every pad of every preset the moment you flash. You do not map them,
assign them, learn them or save them: send the CC and it works. That is the
opposite of every other CC on the module, which you *do* assign yourself, per
preset, at a cost of one modulation slot each.

Fixed numbers work here where MIDI Learn cannot. MIDI Learn works through the
modulation system, exactly what these controls cannot reach: the reason they
needed patching. Fixed CCs cost no modulation slot and work the same on every
preset. The numbers clear the four CCs stock firmware already claims
(1, 7, 10, 64) and the MIDI spec's reserved 96–101 range. **CC 120 and 123
are the exceptions: Channel Mode messages** (All Sound Off, All Notes Off),
which most DAWs already send on stop.


### Which Channel to Send On

**CC 79 through 93 and 120/123**: per-pad. Send them on the MIDI channel that
pad is already set to listen on, the same channel you use to trigger it.
Eight pads on eight channels answer independently.

**CC 108 through 111**: global. They answer on any channel, because they
control the one delay the whole preset shares.

**CC 40 through 45**: the compressor, and they answer on channel 1 only.

**CC 50 through 57, and 59**: the master EQ and filter, on channel 1 only as
well. See [the EQ and filter](EQ-FILTER.md) for what each one does in each of the
ten modes. Send CC 50 as plain 7-bit: a 14-bit pair would put its second half on
CC 82, inside the per-pad range above.

That last one is the odd one out. It is deliberate. **CC 40 to 45 sit in the
fine-adjust range that almost nothing actually uses.** Formally, CC 32 to 63 are
the LSB partners of CC 0 to 31, so CC 40 is the low half of CC 8, but hardly
any device sends those, which leaves the numbers effectively free. It also
means some gear uses them for other things.

"Effectively free" is not the same as free, and this is the one block in the
patch sitting on numbers with a defined meaning. The other patched globals, CC
108 to 111, live in 102–119. That is genuinely undefined. So the compressor is
restricted to a single channel, to stop a stray message from elsewhere in your
rig quietly changing it while you are not looking.

If the compressor does not respond to anything, this is why: put your
controller on MIDI channel 1.

## Setting Up Your Controller

Do this once. It sticks. Every preset behaves the same after that. Nothing to
redo, ever.

### Recommended Controller

The per-pad CCs, 79 to 93, plus 120 and 123, send on each pad's own MIDI
channel, so a controller needs to set the MIDI channel per control, not
merely per device. Many controllers assign one channel per device or preset
and cannot do this.

The **[Neuzeit Instruments Drop](https://www.neuzeit-instruments.com/products/drop/)** is one controller that can. Its manual says
each control element mapped to a device "can either send to the default
channel or a specific channel 1-16": one assignment can override its
device's default channel. In practice, one Drop device slot covers all eight
pads plus the global controls, since every assignment carries its own channel
and CC number, leaving the other seven slots free.

Any controller with the same per-control channel capability works as well.


### Encoder Types

The **Encoder** column in the [MIDI CC map](#midi-cc-map) is a suggestion for
how to set up each control on your controller. It changes the feel, not what
the firmware does: the module always maps a CC evenly across the parameter's
range.

Almost everything wants a plain linear encoder. Two do not:

**Attack and Release**: anything but linear. Both are times, and times are not
heard evenly. Attack runs 0.5 ms to 100 ms, the difference between 0.5 ms and
5 ms is enormous, and on a linear encoder every useful fast setting is crammed
into the first few percent of travel. You want the curve that **starts
gradually and then ramps up**. Vendors name it inconsistently, so go by shape.
On the Drop it is **Exponential minus**, which sags low and climbs late; the
plus variant rises early and would make the crowding worse. Test it at halfway
travel. Attack should be sending about `8` there, and Release about `12`. Near
`64` is still linear.

**Ratio**: a borderline case, and linear is fine. The gentle, musical
settings live in the bottom quarter of the throw: 2:1 sits at about
5% and 4:1 at about 16%, so move slowly down there. Not one to go out of your
way for.

**Loop Mode and LFO Wave**: lists, not sweeps. Use a stepped encoder. The
firmware divides the CC range into even bands, one per entry, so it needs
exactly the right number of positions:

| Control | Steps | The list |
| ------- | ----- | -------- |
| **Loop Mode** | **3** | None / Forward / Bidirectional |
| **LFO Wave** | **10** | the ten waveforms, in the module's order |

Set to ten steps, one click moves cleanly to the next waveform and never lands
between two. Set it to anything else and you will skip shapes or hit the same
one twice, which reads as a broken control.

If your controller cannot do stepped encoders, a slow linear sweep still
works: you just have to find the bands by ear, and the edges are easy to sit
on by accident.

The switches want buttons: anything marked `≥64 on` is a toggle. Set it up as
a button sending 0 and 127 rather than as an encoder you have to sweep.


### Getting the Full Range

This is about the module rather than the patch, but it will catch you out the
first time and it looks exactly like a broken mapping.

A CC mapped to a parameter **through a preset** moves that parameter *upward
from whatever value it was saved with*. It does not sweep the full range. Save a
parameter sitting halfway and your CC reaches the top half of it and stops,
looking for all the world like a broken cable.

So before you save, set the parameter to its **minimum** on the device:

| Parameter | Set it to |
| --- | --- |
| Delay time, Beat Sync on | `1/64`, the first entry |
| Delay time, Beat Sync off | `0%` |
| Delay feedback | `0` |
| Delay cutoff | `0`, and see the note below |
| Reverb decay, damping, pre-delay | `0` |

Now the CC has somewhere to go.

Two things worth knowing:

Cutoff reads oddly. The screen shows it running negative for low-pass and
positive for high-pass, but it is stored as a plain 0-1000 with the midpoint as
neutral. So its minimum really is zero: it just means fully low-passed, and
the delay will sound very dark until the CC brings it up.

Your preset will sound different unmodulated. Feedback at zero is a single
slap rather than a delay, so the preset is duller before any MIDI arrives. That
is the trade for full CC range, and it is only worth making on parameters you
really are going to drive.

None of this applies to any CC in this patch. Every one of them is
written directly rather than through the modulation system, so they all cover
their full range no matter what the pad was saved with. The rule above is about
the CCs you map yourself in a preset.

### Use 14-Bit CC

Worth doing, and worth understanding why it helps, because it is not the reason
you would guess.

The module widens every incoming CC internally, but on the Control Change path
it reads only the value byte and shifts it: **the LSB of a 14-bit CC pair is
discarded**. Your lovely high-resolution controller sends 16,384 values and the
module cheerfully throws away 16,256 of them. It can land on 128 positions and
no more, however you send them, and there is no getting between them.

What 14-bit mode does change is the *feel*. In 7-bit, one detent is one value,
and on a sweep-heavy parameter that lurches. In 14-bit the controller only
advances the MSB once every 128 internal steps, so the same physical turn sends
far fewer messages and the parameter moves smoothly instead of leaping.

Recommended for anything you sweep by hand. Filter cutoff and resonance are
your own CC lanes rather than anything this patch adds, and they are listed
because they show the problem most clearly:

| | |
|---|---|
| **Filter cutoff, resonance** | The two that step most obviously |
| **Granular Density, Window, Scatter** | Slow sweeps are the point |
| **Delay and reverb sends** | Riding a send wants smooth |
| **Envelope Sustain, Loop Crossfade** | Both step audibly in 7-bit |
| **Delay filter Width** | Bandwidth wants the same smoothing |

Leave the switches on plain 7-bit: Reverse, Play Through, LFO Beat Sync, LFO
Retrigger, and the delay's Beat Sync, Ping-Pong and Filter. They only want to
know which side of halfway you are on, and giving them extra precision to ignore
helps nobody. The same goes for the two banded controls, Loop Mode and LFO
Wave: they divide the range into three and ten steps. Finer resolution buys
nothing.

## When a Control Seems Dead

One test settles most of it. Try the encoder. If a control does nothing, the
question is whether the module can do it at all, and the encoder answers that
faster than any amount of MIDI. Things the module will not do are listed in
[limitations](LIMITATIONS.md). Read those next.


### Does the Encoder Do It?

Turn the same parameter by hand on the device. If the encoder changes it,
your MIDI is fine and the fault is elsewhere. If the encoder does not change
it either, the patch is not your problem. You are asking for something the
module itself does not do, and no quantity of MIDI will talk it round.

The one exception is the stop pair, CC 120 and 123. There is no encoder for
stopping a single pad, so that is the one thing here the module cannot do by
hand.
