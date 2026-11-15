# Bitbox Micro Firmware: Clip Modes

Turn your Clip pads into a jungle sampler. Drop in a break at any tempo and it
locks to your clock, stretched the way early-'90s samplers did it, or sped up
like a record. Pitch it, sequence melodies into it, and re-cut it live with 24
Live Flip modes. Then reach for the hold buttons. Rewind it, power down the decks,
brake it or scratch it, and let go to land back on the beat.

Stock Clip Sync keeps a loop in time by jumping back into place when it drifts.
Clip modes stretch it instead, so a 170 BPM break plays smoothly at 134 and
follows the tempo as it changes.

Play every control by CC on a Clip pad's own MIDI channel. You can also set the
Clip mode on the module, trigger Hold FX by gate and sequence the Live Flip mode
by CV, as [Gates and CV](GATES-AND-CV.md) describes.

## Choosing a Clip Mode

Each press of CC 112 steps the pad to the next Clip mode. Set it on the module
with Clip Tempo, just below Sync, and it saves with the preset. CC 112 changes
that setting too.

| Clip mode | Sound | Pitch |
| ------ | ----- | ----- |
| **Stock** | the module's own Clip Sync: jumps back into time when the loop drifts | unchanged |
| **Vinyl** | speeds up or slows down, like a record | follows the speed |
| **Jungle** | the grainy stretch of an early-'90s sampler | unchanged |
| **Key Lock** | a clean stretch that keeps transients intact | unchanged |

Vinyl behaves like a turntable. A 134 break played at 120 drops about two
semitones. The pad's Pitch knob has no effect in Vinyl.

Jungle stretches by repeating or skipping short slices of the loop. CC 115 sets
their length. Short slices buzz, and long ones repeat audibly. CC 116
switches between Modern, with smooth crossfades, and Old Skool, with the
rougher fades of the old samplers.

Key Lock places each join where the sound lines up and away from drum hits, so
the loop sounds untouched.

## Manual Tempo

CC 113 takes the pad off the clock and gives it its own tempo. Press it again
to rejoin the clock. It rejoins in time.

CC 114 sets the Manual tempo, from 1 to 255 BPM. The first time you switch to
Manual, the pad starts at the current tempo. After that it remembers the last
tempo you set. CC 114 has no effect while the pad follows the clock.

For a slow, stretched break, put the pad in Jungle, switch to Manual and halve
the tempo. A pad in Stock switches to Vinyl when you press CC 113.

## Pitch

In Jungle and Key Lock, pitch and timing are independent. The pad's Pitch knob,
pitch modulation and MIDI Keys change the key, and the loop stays on the beat.
You can sequence a melody into a vocal chop, or drop a break by a fourth.

## Live Flip Modes

CC 117 selects a Live Flip mode, which re-cuts the loop as it plays, in time.
Its range is split into 25 steps, starting with Off. The modes need Vinyl, Jungle
or Key Lock. A sequencer's pitch CV can pick them by note, with
[Flip In](GATES-AND-CV.md#cv-action).

The chop modes find the hits and syllables in the audio, so they work on vocals
and chords as well as drums. The harmony modes, 20 to 24, cut the loop into
short grains and pitch every other grain. CC 115 sets their length.

| # | Live Flip mode | What it does |
| :-: | --------- | ------------ |
| 1 | Off | plays the loop as it is |
| 2 | Rotate | starts each bar from beat 2 |
| 3 | Tail Loop | repeats the last beat through the second half of the bar |
| 4 | Half Repeat | replays the middle of the bar in its second half |
| 5 | Two-Bar Repeat | replays the middle of bar 1, twice, as bar 2 |
| 6 | Threes | cuts the bar into a 3 + 3 + 2 pattern of 8ths |
| 7 | Riddim | keeps only the kick and snare, in a dancehall pattern |
| 8 | Figure Loop | loops a six-step figure against the bar |
| 9 | UKG | swings the bar and moves the kick, garage style |
| 10 | Subtle Chops | swaps one or two hits a bar for others |
| 11 | Lift Chops | adds short off-beat chops, some an octave up |
| 12 | Reverse Swell | plays each downbeat reversed into itself |
| 13 | Reverse Reverb | adds a reversed reverb tail into some downbeats |
| 14 | Mix Subtle | plays the loop as it is, with an occasional other mode |
| 15 | Mix Gentle | changes mode every two bars |
| 16 | Mix Wild | changes mode every bar |
| 17 | Pitch Roll | ends each phrase with a rising or falling snare roll |
| 18 | Call & Answer | answers bar 1 with bar 2, an octave up |
| 19 | Chop Stabs | chops the loop with gaps, some as chord stabs |
| 20 | Fifth Up | pitches alternate grains a fifth up |
| 21 | Fourth Up | pitches alternate grains a fourth up |
| 22 | Minor Third | pitches alternate grains a minor third up |
| 23 | Octave Down | pitches alternate grains an octave down |
| 24 | Octave + Fifth | rotates grains through the root, a fifth and an octave |
| 25 | Half-Time | plays the loop at half speed, at its own pitch |

## Hold Buttons

A hold button acts while held and stops when released. Send `64` or more to
hold and less than `64` to release. When you let go, the loop returns to where
it would have been. Hold buttons need Vinyl, Jungle or Key Lock. A gate can
trigger them too, with [Gate Action](GATES-AND-CV.md#gate-action-and-hold-fx).

| CC | Button | While held | On release |
| -- | ------ | ---------- | ---------- |
| **118** | Roll | repeats from the press point: 8ths, then 16ths, then 32nds | back in time |
| **119** | Pitch Roll | the same, a semitone further each repeat, up to an octave | back in time |
| **104** | Freeze | holds a short grain from the press point | back in time |
| **105** | Pull Up | flicks the record back and lets it slow to a stop | back in on the next 16th |
| **107** | Power Down | coasts to a stop, like a deck switched off | spins back up onto the beat |
| **102** | Brake | stops quickly, like a deck's Stop button | spins back up onto the beat |
| **78** | Baby Scratch | rocks the record back and forth in 16ths | back in on the next 16th |

Rolls last four beats. Each press of Pitch Roll reverses its direction, rising
first. Pitch Roll is also a Live Flip mode.

These are the Hold FX that [Gate Action](GATES-AND-CV.md#gate-action-and-hold-fx)
triggers. Baby Scratch is called Scratch there.

Pull Up, Power Down, Brake and Baby Scratch play one at a time. Pressing another
takes over. As the record slows, the sound fades, and a stopped record is
silent.

## Pop-Ups

CC 112 and 113 show the Clip mode above the tempo the pad follows.

| Clip mode | Top line | Bottom line |
| ------ | -------- | ----------- |
| Vinyl | `VINYL` | `Clock  134  0.0st`, with the pitch shift in semitones |
| Jungle | `JUNGLE` | `Clock  134  Modern`, with the flavour |
| Key Lock | `KEY LOCK` | `Clock  134` |
| Stock | `CLIP TEMPO` | `Stock` |

In Manual, the top line adds `MANUAL` and the bottom line starts `Manual`. Any
other pad type shows `Clip pads only`. While a CV input sets the Live Flip mode,
CC 117 shows the input and the mode playing, such as `CV 3: Rotate`.

## Things to Know

### Beat Count

A loop's tempo is worked out from its length and its Beat Count. On Auto the
module guesses. Loops of three or six beats, or loops far from the preset's
tempo, can then play at double or half speed. Set the pad's Beat Count to fix
this.

### After Power-On

An external clock takes over only after it sends a Start. Until then, pads
follow Preset BPM. So restart your sequencer once.

### Preset Loads

Each pad loads with the Clip mode saved in its Clip Tempo setting. Older presets load
Stock. The Live Flip mode and Manual start off, as your
other CCs do. [Keep CCs](SETTINGS.md#keep-ccs) carries what your controller
changed.

### Long Holds

The module keeps about two bars of each loop in memory. A Baby Scratch held
longer than that fades out.
