# Bitbox Micro Firmware: Pad Controls

These are parameters the module already had, but only on the encoder, one at a
time. The patch puts each one on a MIDI CC, so you can play them. Nothing here
is a new sound.

Send them on the pad's own MIDI channel. [The CC reference](CC-REFERENCE.md)
has the numbers. Each one shows in the [pop-up](SETTINGS.md#the-pop-up) as you
send it.

## Building Your Presets

What you get from these controls depends on how the preset is built.

### Building a Preset for This

There is nothing to set up on the module. Every control has a fixed CC that
works on every preset. Set your controller to send those CCs and you are
playing. But **an old preset only gives back what it was built to do.** Eight
Sample pads on one MIDI channel, with the delay turned down and the LFO wired to
nothing, will answer barely a third of these controls.

Here is what matters, in rough order:

- **Give every pad its own MIDI channel.** Put them all on one and every pad
  answers at once.
- **Pick pad modes deliberately.** The granular controls only work on a Granular
  pad, and Play Through only on a Slicer pad. See
  [which pad types work](CC-REFERENCE.md#which-pad-types-work).
- **Park the FX parameters you will drive at their minimum**, so a CC has room
  to travel. See
  [getting the full range](CC-REFERENCE.md#getting-the-full-range). Whatever you
  do not drive stays as saved, so a reverb saved with no decay stays silent.
- **Point the LFO at something**, such as the filter. See [LFO](#lfo).
- **Leave a modulation slot or two spare.** A full pad stops taking new
  routings. See [LFO](#lfo).

### Editors Worth Knowing About

Two browser editors build presets on a computer, with a mouse and a waveform
you can see. Neither is 1010music's, and neither needs this patch.

- [BITBOXER](https://bartbral.github.io/Bitbox-editor/BITBOXER_index.html):
  free, and runs in the browser. It edits pad parameters, effects and whole
  presets, and it reads and writes the XML the module already uses. It handles
  WAV, SFZ and zip archives too.
- [Preset Manager for Bitbox](https://bpm.markijzerman.com/): ten euros, paid
  once. It offers waveform editing with loop and sample markers, effects sends
  and audio preview. It needs a Chromium browser. Bitbox Micro is fully
  supported, and MK2 support is experimental.

Check what either one writes before you trust a whole card to it. Back up any
preset you care about.

## The Controls

The controls fall into six groups. Take them in any order.

### Delay and Reverb

**Delay send and Reverb send**: each pad has its own, so it sits in the effects
by its own amount. They are the fastest way to make a static loop move.
Recorded-sample pads have no FX sends in stock firmware, so there is nothing to
reach.

Four controls set up the delay itself, which the whole preset shares. They
answer on any channel.

- **Beat Sync** locks the delay time to tempo rather than milliseconds.
- **Ping-pong** bounces repeats across the stereo field.
- **Filter on/off** and **Filter width** shape the repeats. Darkening each echo
  is the classic dub move, and width sets how hard.

Delay time and feedback are not here. They already work over MIDI without this
patch.

#### Delay Time and Beat Sync

The delay keeps **two** time values, and Beat Sync picks which one plays. Map a
CC to delay time, switch Beat Sync off, and the CC goes dead. It is still
driving the value that is no longer in use. Map both to the same CC. This is
stock behaviour.

**LFO Rate**: this has the same trap and the same fix. There is one control on
screen and two values underneath, and the LFO's Beat Sync picks between them.

### Playback

**Loop Mode**: None, Forward or Bidirectional, as three even bands across the
throw. Bidirectional plays forwards then backwards, which is lovely or seasick
depending on the material.

**Loop Crossfade**: softens the join. A loop that clicks usually needs a little.
Long crossfades on short loops blur the sound, which is sometimes what you want.

**Reverse**: plays the sample backwards, on Sample, Multi-Sample and Granular
pads. Clip and Slicer pads have no reverse setting.

**Play Through**: on Slicer pads only, this decides whether playback carries on
past the end of a slice into the next.

#### Stopping a Pad

**Stop this pad**: send All Sound Off or All Notes Off, CC 120 or 123, on the
pad's own channel. Either one releases the note. Use it to cut a long clip
cleanly instead of letting it play out. Most DAWs send these when you press
Stop, so stopping the DAW stops the pads too.

### Granular

These five only work on a pad set to **Granular** mode. 1010music marks them
"Mod Target? No", so until now the encoder was the only way in.

**Density**: how many grains play at once, while **Grain Size** sets how long
each one lasts. Between them they take you from a recognisable sample to a
smeared pad to a sparse cloud.

**Window**: the shape each grain fades in and out with. Softer windows sound
smoother. Harder ones click, and that is sometimes the point.

**Scatter**: randomises grain timing, and **Pan Rnd** randomises their place in
the stereo field. Both turn a regular texture into a diffuse one.

Scatter does nothing unless Density is below half. That is stock behaviour. It
will fool you. Send Density at **63 or lower**, because 64 is already a shade
above half. Pull Density down to about a quarter and Scatter is obvious.

Speed is not here. It already works over MIDI without this patch.

These are the controls most likely to cost CPU, Density above all. Watch the
[CPU bar](SETTINGS.md#the-cpu-bar).

### Envelope

**Sustain**: this is the control the patch adds, and the one that makes Decay
work.

Decay is how long the sound takes to fall *from its peak to the sustain level*.
With Sustain at full, Decay has nowhere to go, which is why a decay knob so
often feels dead. Pull Sustain to zero and Decay becomes a **length** control.

#### Try It on a Hi-Hat

| | Sustain | Decay | You get |
| --- | --- | --- | --- |
| **Open, splashy** | high | long | the hat rings on, washes into the next bar, sits back in the mix |
| **Tight, crisp** | low | short | a short chip that punches and gets out of the way |

Both use the same sample on the same pad. Open the hats through a build and
tighten them for the drop, from a knob, while the pattern plays. Then try it on
snares, claps and anything else with a tail.

Attack, Decay and Release could always be modulated. Sustain could not, until
now.

### LFO

Three controls, **Wave**, **Beat Sync** and **Retrigger**, shape the pad's LFO.
It does nothing until you point it at something, so until then all three seem
broken. The manual never says how:

1. Touch the pad, then press **right arrow twice** to reach Pad Parameters.
2. Scroll to **Filter** on the Main page. The three small boxes on the right edge
   mark it as something that can be modulated.
3. Press **right arrow** again for the Modulation Parameters screen.
4. Find a **Source** slot reading `None` and turn the bottom knob to **LFO**.
5. **Turn up the Amount for that slot.** It starts at 0%, so the LFO is connected
   and inaudible until you do. This is the step everyone misses.

The filter is the best first destination, because it is the easiest to hear.

**Wave**: ten shapes, as ten even bands across the throw. **Beat Sync** locks
the rate to the tempo. **Retrigger** restarts the LFO on every note, so a wobble
lines up with your hits instead of drifting against them.

LFO is further round the Source list than you might expect. Keep turning.

If the bottom knob will not change a `None` slot, the pad is full. Each pad has
twelve modulation slots and the firmware keeps three, so a preset with lots of
mapped CCs can use up the rest. It looks like a dead encoder. Free a slot by
removing a mapping you do not use.

### Mute and Solo

Drop pads out with Mute, or isolate them with Solo, from your controller. Solo
the kick for the last bar before a drop, then let go on the one.

Mute is the Mixer's own mute, so the Mixer screen shows it. Solo several pads
and they all play, and when you release the last solo, the mutes you had before
come back. [Keep CCs](SETTINGS.md#keep-ccs) can carry both into the next preset.
The [CC reference](CC-REFERENCE.md) has the numbers.
