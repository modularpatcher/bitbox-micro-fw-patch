# Bitbox Micro Firmware: The Compressor

A bus compressor across Out 1 and 2, and all five of its parameters on MIDI.
See [devices](DEVICES.md) for where it sits, and
[the EQ and filter](EQ-FILTER.md) for the other one.

Your Bitbox has a real bus compressor across Out 1 and 2. It has always been
there. It has always been running. And it has always been shaping every note
you play through those outputs. The module gives you one control over it: **On** or
**Off**.

Threshold, ratio, attack, release, makeup gain: all fixed, all invisible, all
unreachable. Not buried three menus deep. Not hidden behind the modulation
system. Simply never exposed, to anyone, by any route. Which is why it is the
most underused feature in the module: not because it is bad, but because a
compressor you cannot adjust is a compressor you cannot use for anything except
the one thing its designer picked for you.

Everything else in this patch takes a control you could already reach and makes
it playable. **This one hands you five parameters that have never been on the
table at all**, and turns a fixed safety net into an instrument you can shape
while the music runs.

## Where It Sits

It is on the sum of every pad routed to Out 1/2, including the delay and reverb
returns. This is a mix bus compressor, not a channel strip, and it hears the
whole thing at once. See [where they sit](DEVICES.md#where-they-sit) for the
chain, which it shares with the EQ.

Two things follow that matter here. Pads on Out 3–6 bypass it completely, which
is your clean path and is useful below. And the safety limiter behind it means
you can push the compressor hard without worrying that you will damage
anything.

## How a Compressor Works

This guide is not the place to learn that, and there are far better resources
than anything that would fit here. But a one-line version, so the controls below
make sense:

> A compressor turns the loud parts down, then you turn the whole thing back up.
> The result is a smaller gap between loud and quiet, which sounds denser,
> steadier, and louder at the same peak level.

The five controls decide *how loud is loud enough to act on*, *how much to turn
it down*, *how fast to react*, *how fast to let go*, and *how much to put back*.

## What Each Control Does

**Threshold**: how loud a sound has to get before the compressor pays any
attention. Everything below it passes untouched. Bring it down and more of the
music gets caught.

**Ratio**: how hard it clamps once the threshold is crossed. At 2:1, a sound
going 10 dB over the threshold only comes out 5 dB over. At 10:1 it comes out 1 dB
over. Low ratios shape. High ratios flatten.

**Attack**: how quickly it reacts. Fast attacks catch the very front of a
transient and squash it. Slow attacks let the initial hit through and only then
clamp down, which is what keeps drums punchy.

**Release**: how quickly it lets go again. Short releases pump and breathe with
the track. Long releases are smoother and less obvious.

**Makeup gain**: how much level you put back afterwards, to compensate for what
the compression took away.

## How It Arrives

You do not have to configure anything. Switch it on and you have gentle,
musical bus compression, the sort that pulls a mix together rather than
announcing itself. "Glue" is the usual word for it.

The starting values are deliberately close to the classic SSL bus compressor
setting, which is the most-copied mix-glue recipe there is:

| Control | Starts at | Which is |
| ------- | --------- | -------- |
| **Threshold** | 70% | −12 dB |
| **Ratio** | 5% | 2:1 |
| **Attack** | 30% | 30 ms |
| **Release** | 29% | 300 ms |
| **Makeup gain** | 50% | 0 dB |

Three of those five are literally switch positions on an SSL: **2:1**, a slow
**30 ms** attack, and a **300 ms** release. The slow attack is the important one:
it is what lets the front of every kick and snare through before the compressor
clamps, and it is the difference between glue and mush.

Why −12 dB and 2:1, specifically. Threshold is the one setting that depends
on how loud your material is, and a default has to work without knowing that. At
2:1, being wrong is cheap: a sound 10 dB over the threshold comes out only 5 dB
over, so even if the threshold is far lower than ideal you get a gentle squeeze
rather than a crushed mix. Pair that with −12 dB and you get roughly:

| If your mix peaks around | You get |
| --- | --- |
| −1 dBFS (hot) | about 5–6 dB of gain reduction — firm, still musical |
| −6 dBFS (sensible headroom) | about 3 dB — textbook glue |
| −12 dBFS | nothing |
| quieter than that | nothing at all |

It does the right thing at sensible levels, stays musical if you run hot, and
steps aside if you run quiet.

On input level and makeup gain. Aim to have your mix peaking somewhere
around **−6 dBFS** before the compressor. That is good practice anyway. It is
the level these defaults are built around. Makeup gain starts at 0 dB, so
switching the compressor on can only ever make things slightly *quieter*, never
louder, never clipping. Once you have set a threshold you like, bring makeup up
until engaged and bypassed sound about the same loudness. That is the honest way
to judge whether it is helping. Match the levels first. Then compare.

If you run much hotter than −6 dBFS, the compressor works harder. You will
want more makeup. If you run cold, it will barely engage and you should bring the
threshold down rather than reaching for makeup.

## Glue on a Button

If your controller can send several CCs from one button press, **set one up to
send these six.** This is the single most useful thing you can do with the
compressor.

| Send | CC | Value | Which is |
| ---- | -- | ----- | -------- |
| **Compressor on** | 40 | `127` | on |
| **Threshold** | 41 | `90` | −12 dB |
| **Ratio** | 42 | `7` | 2:1 |
| **Attack** | 43 | `38` | 30 ms |
| **Release** | 44 | `37` | 300 ms |
| **Makeup gain** | 45 | `64` | 0 dB |

All six on **MIDI channel 1**, as ordinary `0`–`127` values. **Include CC 40.**
Without it the button restores five settings into a compressor that may still
be switched off, and you hear nothing change.

Each value is the nearest CC step to the figure beside it, which lands within a
fraction of a dB, or a few milliseconds, of the target. Well under what anyone
can hear. [How It Arrives](#how-it-arrives) explains why these five.

Set the button to **momentary, not toggle**. A latching button holds its values
down, and your encoders cannot move the parameters out from under it. Momentary
sends the six once, on press, then gets out of the way. Controllers call it
momentary, temporary or trigger, as against toggle or latch.

One consequence, and it is not a fault. After a press the module sits on the
glue values while your encoders are still wherever you left them, so the two
disagree. Turn one and the parameter jumps to meet it. If your controller
offers **pickup**, also called soft takeover or catch, switch it on for these
five. The encoder then waits until you sweep past the value the module is
really on, and takes over from there.

It gives you a home to return to. Modify the settings as much as you like, and
when you want the glue back, one press restores it instantly. Knowing the way
back is what makes the rest worth trying.

If you want to hear what your module did before you patched it, a second button
does that: threshold `115`, ratio `20`, attack `12`, release `31`, makeup `57`.
That is −4 dB, 4:1, 10 ms, 250 ms and −4 dB of makeup, which is a headroom trim
rather than a musical setting. It is worth hearing once, to know what was
changed on your behalf.

## Setting It by Ear

**The module's screen**: never shows any of this, and there is no gain reduction
meter. That is true of everything this patch adds, the display does not redraw
when a CC arrives. A compressor is invisible anyway. Your controller is the
display. If you turn an encoder on the module itself, the module wins.

**Finding the threshold**: the only fiddly part. There is a technique:

> Bring the **threshold** down slowly while something busy is playing. At first
> nothing happens. Then the loud parts start to lean back a little. **That point
> is the threshold**, leave it just past there. If it starts sounding squashed
> or breathless, you have gone too far.

The others are quicker to hear:

- **Ratio**: sweep it top to bottom on a busy loop. Transparent, then obviously
  squashed.
- **Release**: short, on a kick loop. It should pump hard. The most audible of
  the four by a wide margin.
- **Attack**: long, on the same loop. The click of the kick comes back.
- **Makeup gain**: level, and nothing else.

## Two Things That Confuse

Nothing happens while the compressor is off. The five settings are
remembered. But nothing applies them until it is running. So if you set
everything up with it switched off and hear no change, that is expected: switch
it on and it all arrives at once.

**Makeup gain has two zeroes**, and they are nowhere near each other. It runs
from −36 dB to +36 dB, and it arrives at 0 dB, which is the middle of the
throw. The bottom of the throw is −36 dB, which is silence. So if that encoder
happens to be parked at the bottom when you first touch it, the sound vanishes
and it looks like something has broken. It has not. Turn it up to halfway.

## What It Will Not Do

There is no sidechaining. You cannot duck the mix from a kick on another
channel, or key the compressor from anything but the signal passing through it.
The detector is wired to the audio it is compressing and there is no key input to
redirect it, so this is a limit of the module rather than something the patch
chose not to expose.

What gets you close: ordinary bus compression already ducks the mix when the kick
lands, that is what glue compression does, and with a fast attack and a loud kick
it pumps convincingly. The difference is that the kick is compressed too. And if
you sequence CCs, you can draw a threshold envelope in time with the kick, which
is automation rather than sidechaining but reaches a similar place.

There is no wet/dry mix either. The module cannot blend compressed and
uncompressed signal: there is no mix control and no way to add one.

What you can do instead: **pads routed to Out 3–6 skip the compressor entirely.**
Put the same sample on two pads with the same MIDI channel and Pad Note, route one
to Out 1/2 and the other to Out 3/4, and mix them outside the box. Squash the
compressed one hard and blend it underneath. That is parallel compression, using
stock routing.

It costs a pad and an output pair, so most people will not bother, but it is
there if you want it.

## They Last Until Power Off

Your compressor settings survive a preset change, which nothing else in this
patch does, and a power cycle puts them back to the glue values under
[How It Arrives](#how-it-arrives). That behaviour is shared with the EQ and is
set out in [what they have in common](DEVICES.md#what-they-have-in-common).

Used well, this is the control that makes eight pads sound like one record.
Everything else here shapes a sound; the compressor is what makes all of them sit
together, breathe together and hit as a single thing. That is what glue means.
It is why this is the addition worth learning properly.
