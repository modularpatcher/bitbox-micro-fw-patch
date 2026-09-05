# Bitbox Micro Firmware: The Master EQ and Filter

A multi-algorithm master EQ and filter across the whole mix. Ten algorithms
on one encoder. See [devices](DEVICES.md) for where it sits, and
[the compressor](COMPRESSOR.md) for the other one.

Where the compressor controls dynamics, this one controls tone and colour. Ten
modes sit on one encoder: filters first, a DJ sweep and a dual cut and a band
pass, then shelving and bell EQs, a notch, a phaser and a vowel morpher. One
knob picks the character. Three more play it.

It runs on the sum of everything routed to Out 1 and 2, after the delay and
reverb returns. So it takes the tails with it, which you hear and which is the
point. Pads on Out 3–6 stay clean.

The module offers no EQ of any kind. No page, no menu, nothing in the manual,
and nothing you can assign. Where the rest of this patch reaches controls the
module already had, this one adds brand new features that have never been
available on the hardware.

## Centre Is Always Neutral

One rule holds the whole design together, and it is worth knowing before you
touch anything. **Every control is neutral at its centre position, in every
mode.** Three knobs centred is an EQ doing nothing at all, whichever mode you
are in.

That is what makes ten modes playable from one panel. You can sweep the mode
dial with the controls centred and hear nothing from the EQ the whole way, then
pick a mode and move one knob. It also means a control's own centre is a real
bypass for that control, so you can back out of anything by returning it.

On a controller with endless encoders there are no detents to feel for, so give
each of the three a centre-line LED style and a reset-to-middle gesture. You
will use both constantly.

## The Ten Modes

Every mode has been heard and tested on hardware.

| # | Mode | Encoder 2 | Encoder 3 | Encoder 4 |
| - | ---- | --------- | --------- | --------- |
| **1** | **DJ Filter** | filter, dull to thin | resonance | knee |
| **2** | **Dual Cut** | high cut | low cut | resonance |
| **3** | **Band Pass** | centre frequency | width | gain |
| **4** | **Mixer EQ** | high | mid | low |
| **5** | **Tone + Filter** | high | low | filter, dull to thin |
| **6** | **Tilt** | tilt | hinge frequency | — |
| **7** | **Notch** | frequency | Q | depth |
| **8** | **Peak** | frequency | Q | gain |
| **9** | **Phaser** | sweep | depth | spread |
| **10** | **Formant** | vowel, I to U | intensity | shift |

The first five, briefly. **DJ Filter** is the classic one-knob sweep: anticlockwise closes to a dull
thump, clockwise thins to hats. **Dual Cut** gives you both ends separately.
**Band Pass** isolates a band you can move, narrow and lift. **Mixer EQ** is a
three-band desk EQ. **Tone + Filter** puts two shelves and a filter on one
panel.

Then the rest. **Tilt** is a see-saw, lifting the treble by exactly what it drops the bass.
**Notch** and **Peak** are one surgical cut and one sweepable bell. **Phaser**
sweeps four notches together as one body. **Formant** parks three bells on the
resonances of a human vocal tract and morphs between vowels.

Ten algorithms across three encoders is an enormous amount of sound design to
go at, and this guide stops at what each control does. What any of it does to
your own material is the interesting part, and there is far more of it than
could be written down. That is yours to explore.

## Working the Four Encoders

Nine CCs, all on **MIDI channel 1**, like the compressor.

| Control | Turn | Push |
| ------- | ---- | ---- |
| **Encoder 1** | CC **50** — mode | CC **57** — master bypass |
| **Encoder 2** | CC **51** | CC **54** |
| **Encoder 3** | CC **52** | CC **55** |
| **Encoder 4** | CC **53** | CC **56** |
| **A button** | — | CC **59** — filter slope, 12 or 24 dB |

Encoders 2, 3 and 4 want a plain linear encoder across the full range. The mode
encoder is the fussy one. Set it to **ten steps** running from **6 to 122**
rather than the full sweep. Ten windows across 128 values are about thirteen
wide, and a full sweep lands a step within one value of a boundary, which makes
dialling a mode unreliable. Starting at 6 and stopping at 122 centres every
step with six values of margin.

Send CC 50 as plain 7-bit. A 14-bit pair would put its second half on CC 82,
which is inside the per-pad range, and you would be moving a pad's granular
window every time you changed mode.

## Kill, Open and Slam

The pushes are the performance controls, and they are all momentary. Hold to
act. Release to return. What comes back is the encoder's own position,
untouched, so nothing jumps.

Three different things, deliberately not sharing a word:

| Gesture | What it does | Where |
| ------- | ------------ | ----- |
| **Kill** | that range disappears from the mix | Mixer EQ's three, Tone + Filter's two |
| **Open** | that band stops acting, letting more through | Dual Cut, Band Pass width, Tone + Filter's filter |
| **Slam** | forces a control to maximum effect | Notch, Tilt, Peak, Phaser, Formant |

Sixteen pushes do something across the ten modes. The rest are unassigned, and
on those you are better off spending the push on a reset-to-centre gesture
instead. Most controllers let you flip a push between sending its CC and
centring the encoder locally, one setting, no re-entry. Expect to flip it.

## Bypass and Slope

CC 57 takes the whole EQ out. Press once and it is gone, press again and
it is back. It is latching rather than momentary, and it is the one push on
encoder 1 worth spending a CC on, because a mode dial has no meaningful middle
to reset to.

CC 59 switches the filter slope between 12 and 24 dB per octave, on the modes
that have one. Set it up as an on-off button. Band Pass is the exception:
there its gain control needs the same bands, and gain wins.

## How the EQ Arrives

It powers on in **DJ Filter**, all three controls centred, bypass off and slope
at 12 dB. So a freshly flashed module sounds exactly as it did before. Centred controls
pass everything through.

Its settings behave like the compressor's. They survive a preset change, and a
power cycle puts them back to the boot state above. See
[what they have in common](DEVICES.md#what-they-have-in-common).
