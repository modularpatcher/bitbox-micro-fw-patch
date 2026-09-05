# Bitbox Micro Firmware: Pad Controls

Parameters the module already had. Each one was reachable only by pressing the
encoder and turning it, one at a time, and the patch puts every one of them on
a MIDI CC instead. Nothing here is new sound. None of it is new code.
It is the same engine, reachable while you play. That is all.

These are addressed per pad, on that pad's own MIDI channel. One pad, one
channel. Eight pads, eight channels. See [the CC reference](CC-REFERENCE.md)
for the numbers.

## Building Your Presets

Two things shape what you get out of this patch: how the preset is put
together, and how easily you can put it together. This covers both.

### Building a Preset for This

Worth knowing before you start, because it shapes what you get out of this.

The controls need no setting up and work on every preset you own from the
moment you flash. But **an old preset will only give you back what it was built
to do.** Eight Sample pads all listening on one MIDI channel, with the delay
turned down and the LFO wired to nothing, will answer barely a third of them,
and none of it is the patch's fault.

The things that matter, in rough order:

- **Give every pad its own MIDI channel.** This is the big one. Per-pad controls
  are addressed by channel, so eight pads on eight channels is eight independent
  sets of controls. All on one channel and every pad answers at once.
- **Pick pad modes deliberately.** The five granular controls do nothing except
  on a Granular pad, and Play Through nothing except on a Slicer pad.
- **Decide which FX parameters you will drive and which you will fix.** The six
  worth mapping, delay time, feedback, cutoff, reverb decay, damping, predelay,
  want parking at their minimum so a CC has room to travel. Whatever you do
  not map is whatever you saved it as, so a reverb with no decay and nothing
  driving it is silence.
- **Point the LFO at something**, the filter is the obvious choice. Three of
  these controls shape an LFO that does nothing until you do.
- **Leave a modulation slot or two spare** for it. Fill all nine CC lanes on a
  pad and there is no room left to route anything, and the module does not warn
  you, the Source box simply stops responding.

[Which Channel to Send On](CC-REFERENCE.md#which-channel-to-send-on) covers the
first point, and [Which Pad Types Work](CC-REFERENCE.md#which-pad-types-work)
the second. FX parameters, the LFO and modulation slots each get their own
section below. The short version: half an hour spent on a
preset built for this is worth more than any single control on its own.

### Editors Worth Knowing About

Building a preset on the module means the encoder, one parameter at a time. Two
browser editors do the same job on a computer, with a mouse and a waveform you
can see. Neither is mine, neither is 1010music's, and neither needs this patch.
They are listed because pointing a CC at a preset is quicker when the preset was
quick to build.

- [BITBOXER](https://bartbral.github.io/Bitbox-editor/BITBOXER_index.html):
  free, and runs in the browser. Edits pad parameters, effects and whole
  presets, and reads and writes the XML the module already uses. It handles WAV,
  SFZ and zip archives too.
- [Preset Manager for Bitbox](https://bpm.markijzerman.com/): ten euros, one
  payment. Waveform editing with loop and sample markers, effects sends, and
  audio preview before you commit. It wants a Chromium browser. Bitbox Micro is
  fully supported; MK2 is experimental.

Check what either one writes before you trust a whole card to it. Keep a backup
of a preset you care about.

## The Controls

Five groups. Take them in any order.

### Delay and Reverb

**Delay send and Reverb send**: per pad, so each of your eight can sit in
the effects by a different amount, changed live. This is the pair people reach for
first, and it is the fastest way to make a static loop move.

Recorded-sample pads are the exception: they have no FX sends in stock firmware
at all, so there is nothing for the control to reach. That is the module, not this
patch.

**The delay itself**: shared by the whole preset, and four of its controls are
here. These answer on any channel:

**Beat Sync**: locks the delay time to tempo rather than milliseconds.
**Ping-pong** bounces repeats across the stereo field. **Filter on/off** and
**Filter width** shape the tone of the repeats: darkening each successive
echo is the classic dub move, and width is how aggressively it happens.

The delay's time and feedback are not here, because the module already lets you
modulate them: they work over MIDI without this patch.

#### Delay Time and Beat Sync

The delay keeps **two** separate time values and beat-sync decides which one the
engine actually reads. So a CC mapped to delay time works beautifully until you
switch beat-sync off, at which point it goes completely dead, because your CC
is still faithfully driving a parameter nobody is listening to any more.

Map both destinations to the same CC and the problem evaporates. This is a
preset-level quirk and nothing to do with the patch. It was here first.

**LFO Rate**: exactly the same shape, for exactly the same reason: one
control on screen, two parameters underneath, and BeatSync picking between
them. If your LFO Rate knob dies the moment you flip that switch, this is why,
and the same fix applies.

### Playback

These decide what the sample actually does when a pad is triggered, and they are
the first thing in the chain.

**Loop Mode**: picks between None, Forward and Bidirectional. On a controller
it is three even bands across the throw. Bidirectional runs the sample
forwards then backwards, which is either lovely or seasick depending on the
material.

**Loop Crossfade**: softens the join. A loop that clicks usually just needs a
little of this. Long crossfades on short loops start to blur the sound, which
is sometimes what you want.

**Reverse**: plays the sample backwards. It works on Sample, Multi-Sample and
Granular pads only: Clip and Slicer pads have no reverse setting in the
module at all, so there is nothing for the control to reach.

**Play Through**: is a Slicer control and only a Slicer control. It decides
whether playback carries on past the end of a slice into the next one. On any
other pad type it does nothing, because there are no slices to carry on past.

#### Stopping a Pad

**Stop this pad**: answers to the two standard MIDI panic messages, All Sound
Off and All Notes Off, on that pad's own channel. Either one releases the note.

It matters most on long clips. A four-bar loop that you have triggered and now
want gone would otherwise play to its end, and this cuts it cleanly from wherever
your controller sits. Both messages behave identically here. Use whichever
your controller sends more conveniently.


### Granular

These five only do anything on a pad set to **Granular** mode, and they are the
part of this patch the module fights hardest. They are the Granular page
controls 1010music marks "Mod Target? No": no three boxes on the screen to
indicate that modulation is possible, and MIDI Learn looks straight through
them. Until now the encoder was the only way in.

**Density**: how many grains play at once. **Grain Size**: how long each one
lasts. Between them they take you from a recognisable sample to a smeared pad to
a sparse cloud of fragments.

**Window**: the shape each grain fades in and out with. Softer windows sound
smoother; harder ones click and click is sometimes the point.

**Scatter**: randomises grain timing, and **Pan Rnd** randomises their position in
the stereo field. Both turn a regular texture into a diffuse one.

Scatter is the one that will fool you. It does nothing whatsoever unless
Density is below half. The manual says so plainly, and it is the module's
behaviour rather than anything this patch introduces. Sweep Scatter with Density
up. You will hear nothing and blame the patch. Pull Density down to a quarter
first, where the grains are sparse enough to hear individually, and Scatter's
effect on their timing is obvious. In CC terms, that threshold is **CC 63 or
lower**: CC 64 already lands a shade above half, so one step further up puts
Scatter back to doing nothing.

Speed is not here, deliberately. The module already marks it "Mod Target?
Yes", so it works over MIDI without any of this.

These are also the ones most likely to cost you CPU. Density is the one the
manual warns about.


### Envelope

**Sustain**: the control this patch adds, and Decay is why it matters.

Decay is how long the sound takes to fall *from its initial peak down to the
sustain level*. So with Sustain at full, Decay has nowhere to travel and does
nothing you can hear, which is exactly why a decay knob so often feels dead.
Pull Sustain down and it comes alive: at zero, the pad falls from its peak to
silence over the decay time, and Decay becomes a **length** control.

#### The Hi-Hat, Which Is the Case That Makes It Click

Load a hi-hat and you have two very different instruments in one sample,
depending entirely on where these two sit:

| | Sustain | Decay | You get |
| --- | --- | --- | --- |
| **Open, splashy** | high | long | the hat rings on, washes into the next bar, sits back in the mix |
| **Tight, crisp** | low | short | a short chip that punches and gets out of the way |

Same sample. Same pad. The difference between a hat that drives a track and one
that clutters it is those two controls, and until now Sustain was frozen at
whatever the preset happened to be saved with.

Now you can move between the two while the pattern is playing. Open the hats
through a build, tighten them for the drop, and do it from a knob rather than by
loading a different preset. That is the whole argument for putting Sustain on a
CC, and once you have heard it on a hat you will start using it on snares, claps
and anything else with a tail.

**Attack, Decay and Release**: always modulation targets, so you could
already automate those. Sustain never was, which meant you could drive Decay all
you liked while the one control that decides whether Decay does anything stayed
fixed. Both are yours now.


### LFO

Three controls here, **Wave**, **Beat Sync** and **Retrigger**, and they shape
an LFO rather than making a sound themselves. If that LFO is not pointed at
anything, all three will seem broken, and this catches people because **the
manual never explains how to connect it.**

The LFO is a modulation *source*, like velocity or the mod wheel. It does nothing
until you give it a destination:

1. Touch the pad, then **right arrow twice** to Pad Parameters.
2. Scroll to **Filter** on the Main page. The three small boxes on the right edge
   mark it as something that can be modulated.
3. **Right arrow** again for the Modulation Parameters screen.
4. Find a **Source** slot reading `None` and turn the bottom knob to **LFO**.
5. **Turn up the Amount for that slot.** It starts at 0%, so the LFO is connected
   and inaudible until you do. This is the step everyone misses.

Filter is the best first destination because it is the most obvious to the ear,
and it doubles as your test rig for the three LFO controls.

**Wave**: picks the shape, ten of them, as ten even bands across the throw.
**Beat Sync** locks the rate to the tempo instead of running free. **Retrigger**
restarts the LFO from the top on every note, which is the difference between a
wobble that lines up with your hits and one that drifts against them.

If the bottom knob will not change a `None` slot, the pad is full. Each pad
has twelve modulation slots, three of which the firmware claims for itself, and a
preset with lots of mapped CCs can easily use the rest. It reads exactly like a
dead encoder. Free a slot by removing a mapping you are not using. It will
spring back.

Worth knowing: the Source list on that screen is longer than the manual suggests.
The MIDI chapter lists the MIDI sources, the CV chapter lists EXT 1–8, and no page
lists the whole thing. LFO is in there, several clicks further round than you
might stop.

