# Bitbox Micro Firmware: The Compressor

Your Bitbox has always had a bus compressor across Out 1 and 2, but the module
only lets you switch it on and off. Threshold, ratio, attack, release and makeup
gain were fixed. This patch puts all five on MIDI, adds auto makeup, and starts
you on a classic glue setting. It also adds three Airwindows compressors to
choose from. Duck the mix from a gate or a MIDI note with its sidechain.

## Where It Sits

It acts on the whole Out 1/2 mix, delay and reverb returns included, after the
Character chain and the EQ. See [where they sit](DEVICES.md#where-they-sit).
Pads on Out 3–6 skip it, and that comes in useful below.

## How a Compressor Works

Here it is in one line:

> A compressor turns the loud parts down, then you turn the whole thing back up.
> The result is a smaller gap between loud and quiet, which sounds denser,
> steadier, and louder at the same peak level.

## What Each Control Does

- **Threshold**: how loud a sound has to get before the compressor acts. Bring
  it down and more of the music gets caught.
- **Ratio**: how hard it clamps above the threshold. At 2:1, a sound 10 dB over
  comes out 5 dB over. At 10:1 it comes out 1 dB over. Low ratios shape. High
  ratios flatten.
- **Attack**: how quickly it reacts. Fast attacks squash the front of a hit.
  Slow attacks let it through first, which keeps drums punchy.
- **Release**: how quickly it lets go. Short releases pump and breathe. Long
  releases are smoother.
- **Makeup gain**: how much level you put back afterwards.

## How It Arrives

Switch it on and you get glue: gentle bus compression that pulls a mix together.
The starting values are close to the classic SSL bus compressor setting.

| Control | Starts at | Which is |
| ------- | --------- | -------- |
| **Threshold** | 70% | −12 dB |
| **Ratio** | 5% | 2:1 |
| **Attack** | 30% | 30 ms |
| **Release** | 29% | 300 ms |
| **Makeup gain** | 50% | 0 dB |

The slow 30 ms attack matters most. It lets the front of every kick and snare
through before the compressor clamps. That is the difference between glue and
mush.

How much it does depends on how loud your mix is:

| If your mix peaks around | You get |
| --- | --- |
| −1 dBFS (hot) | about 5–6 dB of gain reduction — firm, still musical |
| −6 dBFS (sensible headroom) | about 3 dB — textbook glue |
| −12 dBFS | nothing |
| quieter than that | nothing at all |

Aim for peaks around **−6 dBFS** going in. Makeup starts at 0 dB, so switching
on can only make things slightly quieter, never louder. Once you like the
threshold, bring makeup up until the compressor sounds equally loud on and
bypassed. Match the levels first. Then compare.

If your mix runs much hotter, the compressor works harder and you will want
more makeup. If it runs cold, bring the threshold down rather than reaching for
makeup.

## Glue on a Button

If your controller can send several CCs from one button, set one up to send
these six. Then experiment as much as you like. One press brings the glue back.

| Send | CC | Value | Which is |
| ---- | -- | ----- | -------- |
| **Compressor on** | 40 | `127` | on |
| **Threshold** | 41 | `90` | −12 dB |
| **Ratio** | 42 | `7` | 2:1 |
| **Attack** | 43 | `38` | 30 ms |
| **Release** | 44 | `37` | 300 ms |
| **Makeup gain** | 45 | `64` | 0 dB |

Send all six on MIDI channel 1. Include CC 40. Without it, a press can restore
five settings into a compressor that is still off, and you hear nothing change.

Set the button to **momentary, not toggle**. Some controllers call it temporary
or trigger. A latching button holds its values, and your encoders cannot move
them.

After a press, the module sits on the glue values while your encoders are
wherever you left them. Turn one and the parameter jumps to meet it. If your
controller offers pickup, also called soft takeover or catch, switch it on for
these five. Each encoder then waits until it passes the module's value.

To hear what your module did before you patched it, set up a second button:
threshold `115`, ratio `20`, attack `12`, release `31`, makeup `57`. That is
−4 dB, 4:1, 10 ms, 250 ms and −4 dB of makeup. It is a headroom trim, not a
musical setting.

## Four Compressors

CC 47 selects the compressor type, and each press of CC 48 steps to the next.
Stock is the module's own compressor, described above. The other three are
ports of Chris Johnson's Airwindows compressors:

- Logical4: a punchy bus compressor with some saturation.
- VariMu: a valve-style compressor that gets thick and heavy when pushed.
- Thunder: a one-knob compressor that adds weight to the low end.

All four use CC 41 to 45, for different controls:

| CC | Stock | Logical4 | VariMu | Thunder |
| -- | ----- | -------- | ------ | ------- |
| **41** | Threshold | Threshold | Intensity | Thunder |
| **42** | Ratio | Ratio | — | — |
| **43** | Attack | Speed | Speed | — |
| **44** | Release | Dry/Wet | Dry/Wet | — |
| **45** | Makeup | Makeup | Output | Output |

CC 40 turns the selected type on and off. The module starts on Stock, and the
glue settings and Glue on a Button above apply to Stock. Each type remembers
its own settings. Switching is click-free. Only the selected type uses any
processing power.

VariMu crushes hard at high Intensity. For weight without the crush, set
Dry/Wet to about half.

## Auto Makeup

Auto makeup sets the makeup gain for you, so the level stays the same as you
compress harder. Each press of CC 46 on channel 1 steps it through Off, Normal
and Hot. It works on every type.

- Normal: for a mix with headroom.
- Hot: for a mix that is already loud. On a quiet mix it adds too much gain.

While it is on, CC 45 is ignored, including the makeup a glue button sends. The
pop-up says `Makeup  Auto` if you move it. Switch auto off and the makeup stays
where auto left it until you move CC 45.

## Sidechain

Use the sidechain to duck the whole mix from a gate or a MIDI note, for the
classic pumping sound. Choose what the compressor listens to with SC In, on CC 49
on channel 1. Its range is split into ten steps.

| SC In | The mix ducks |
| ----- | ------------- |
| Self | as usual: the compressor listens to the mix itself |
| Gate 1 to 8 | while that input is high |
| MIDI 16 | while a note is held on MIDI channel 16, deeper for a harder note |

Try it with a kick. Send the kick's gate to input 8, set the kick pad's
[Gate In](GATES-AND-CV.md#gate-in) to 8, and set SC In to Gate 8. Every kick now
plays and pumps the mix. Route the kick pad to Out 3/4 so it skips the
compressor and stays loud.

The compressor's own settings shape the duck. On Stock, Ratio sets how deep it
goes and Release how quickly the mix swells back. Stock, VariMu and Thunder
take the sidechain. Logical4 does not, and its pop-up says `SC In  N/A`.


## Setting It by Ear

There is no gain reduction meter. Your ears are the meter.

To find the threshold, bring it down slowly while something busy plays. At
first nothing happens. Then the loud parts start to lean back a little. Leave it
just past that point. If it sounds squashed or breathless, you have gone too
far.

The others are quicker to hear:

- **Ratio**: sweep it on a busy loop, from transparent to obviously squashed.
- **Release**: set it short on a kick loop. It should pump hard. Release is by
  far the most audible.
- **Attack**: set it long on the same loop, and the click of the kick comes
  back.
- **Makeup gain**: it changes the level, and nothing else.

## Two Things That Confuse

Nothing happens while the compressor is off. It remembers the five settings and
applies them all when you switch it on.

Makeup gain runs from −36 dB to +36 dB and starts at 0 dB, in the middle. The
bottom, −36 dB, is silence. If your encoder is parked at the bottom when you
first touch it, the sound vanishes. Nothing is broken. Turn it up to halfway.

## What It Will Not Do

The [sidechain](#sidechain) takes a gate or a MIDI note, not audio. The module
has no spare audio input to listen to, so a kick on another channel cannot key
it directly. Send the kick's gate or MIDI note instead.

Stock has no wet/dry mix, though Logical4 and VariMu do, on CC 44. For Stock,
use the outputs instead. Put the same sample on
two pads with the same MIDI channel and Pad Note, route one to Out 1/2 and the
other to Out 3/4, and mix them outside the box. Squash the first hard and blend
it underneath. That is parallel compression, at the cost of a pad and an output
pair.

## What Power-On Keeps

Settings survive a preset change. Through power-off, the module also keeps the
type, the Airwindows types' settings, auto makeup and SC In. Stock returns to the glue
settings above. See
[what they have in common](DEVICES.md#what-they-have-in-common).
