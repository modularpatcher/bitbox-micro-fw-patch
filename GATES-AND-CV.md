# Bitbox Micro Firmware: Gates and CV

Sequence or modulate a Clip pad's Live Flip mode by CV, and trigger its Hold FX
by gate. Reassign gate inputs so one gate fires several pads, and let pads that
share an input or a MIDI channel take turns. Record and re-record loops by gate,
into one pad or whichever pad you select.

The pad settings on this page save with the preset. Gate Rec is a module setting,
and it is kept through a power-off.

## Before You Start

Set each input's job on the module's Input screen: push TOOLS twice, select the
input, and choose Gate for triggers or CV for control voltages. Inputs 1 and 2
can also be set to Audio for recording. The Input screen saves with the preset.

To reach the new settings, touch the pad, press the right arrow twice to reach
its Pad Parameters, and scroll down past Excl Group. Gate In, Round Robin and Rec
In appear on every pad. Clip pads add CV Action, Gate Action and Hold FX, because
Live Flip modes and Hold FX exist only there. Those three also need the pad's [Clip mode](CLIP-MODES.md) set to Vinyl,
Jungle or Key Lock.

## On Clip Pads

### CV Action

Use CV Action to sequence or modulate the pad's Live Flip mode, or to choose the
Hold FX that its gates trigger, from a CV input. The voltage is read one semitone
per step from 0 V, so the notes of a sequencer track make the choice. With CV
Action set to Off, the pad's settings and CCs work as usual.

Flip In 1 to 8 sets the Live Flip mode from that input. C at 0 V is Off, C# is
Rotate, and each semitone up selects the next mode in the
[list](CLIP-MODES.md#live-flip-modes), up to Half-Time at 2 V. It works whether
the pad is played from MIDI or from a gate. While Flip In is set, CC 117 leaves
the Live Flip mode alone, and its pop-up shows the input and the mode playing,
for example `CV 3: Rotate`.

Hold FX In 1 to 8 chooses the Hold FX that each gate triggers, in place of the
pad's Hold FX setting. It needs [Gate Action](#gate-action-and-hold-fx) set to
Hold FX.

| Note | Volts | Hold FX In chooses |
| ---- | ----- | ------------------ |
| C | 0 | Off |
| C# | 0.08 | Roll |
| D | 0.17 | Pitch Roll |
| D# | 0.25 | Freeze |
| E | 0.33 | Pull Up |
| F | 0.42 | Power Down |
| F# | 0.5 | Brake |
| G and above | 0.58 | Scratch |

For example, send the gates of one sequencer track to input 8 and the pitch CV
of another to input 3. The note on each step then chooses the Hold FX that its
gate triggers.

A pop-up shows the first choice after you change CV Action. After that it stays
quiet, so a running sequence does not keep covering the screen.

### Gate Action and Hold FX

Use Gate Action to trigger Hold FX by gate. On Play, the gate plays the pad, as
in stock firmware. On Hold FX, the gate triggers the Hold FX chosen below for as
long as it is high, and stops it when it falls. The gate no longer starts the
pad, so play the pad from MIDI or from another input. These are the same effects
as the [hold buttons](CLIP-MODES.md#hold-buttons) you can play by MIDI CC.

| Hold FX | While the gate is high |
| ------- | ---------------------- |
| Off | nothing happens |
| Roll, Pitch Roll, Freeze | the loop repeats or freezes from where the gate arrived |
| Pull Up, Power Down, Brake, Scratch | the loop is pulled back, slowed, stopped or scratched like a record |
| Random | a different Hold FX on every gate |

To sequence a random Hold FX on the last beat of every bar, set Hold FX to
Random and put a gate on that step. Off stops the gate doing anything without
changing Gate Action, which is useful while you build a pattern.

## Playing Pads from Gates

### Gate In

Use Gate In to reassign the pad's gate input.

| Gate In | The pad plays from |
| ------- | ------------------ |
| Own | the input in the pad's own position, as in stock firmware |
| 1 to 8 | that input |
| None | no input, only MIDI and the screen |

Several pads can share an input, so one gate can play a kick and a layered sub
together. A pad set to another input no longer plays from its own.

### Round Robin

Use Round Robin to alternate between pads that share an input or a MIDI channel,
or to pick one at random on each hit.

| Round Robin | What happens |
| ----------- | ------------ |
| Off | the pad plays on every gate or note |
| On | the pads take turns, in screen order |
| Random | a random pad plays, never the same one twice in a row |

For a snare roll that doesn't sound like a machine gun, load three different
snares on three pads, set each to Gate In 4 and Round Robin On, and send the
roll to input 4. On a MIDI channel, each note-off stops the pad that its note-on
played, so held notes ring out as you would expect.

## Recording from a Gate

### Rec In

Use Rec In to record into a pad by gate. It is a per-pad setting, on the pad's
Main Parameters page. Set input 1, or inputs 1 and 2, to Audio first.

| Rec In | What a gate does |
| ------ | ---------------- |
| Off | nothing |
| Record 1 to 8 | starts recording if the pad is empty, and the next gate stops it |
| Replace 1 to 8 | clears the pad and records again, every time |

Rec Quant, Length and RecToPlay work as they do when you record from the screen.
Replace keeps the old WAV on the card, so nothing is lost.

Pads that share an input take turns in screen order. With three empty pads on
Record 5, each pair of gates fills the next pad, so you can record takes across
several pads like a looper. One input can record one pad and play another at the
same time.

### Gate Rec

Use Gate Rec to record into whichever pad you select, by gate. It is on the
Settings page, not on a pad. Press TOOLS once and it sits below MIDI Rec, with
the same choices as Rec In. It works as MIDI Rec does for a note, so select a
pad and then send the gate. It takes priority over Rec In on the same input, and
it is kept through a power-off.

A pad is always selected after power-on. With Gate Rec on REPLACE, a sequence
that is already running re-records that pad straight away.

## With MIDI

Gates, CV and MIDI work together on the same pad. This is what happens when they
meet.

| You do this | What happens |
| ----------- | ------------ |
| Play a pad from MIDI and from a gate | both play it, and the gate plays at full velocity |
| Play a Clip pad from MIDI with Gate Action on Hold FX | MIDI plays the pad, and the gate triggers the Hold FX |
| Play a Clip pad from MIDI with CV Action on Flip In | MIDI plays the pad, and the CV sets its Live Flip mode |
| Send CC 117 while Flip In is set | the CV keeps the Live Flip mode until you set CV Action to Off |
| Use Round Robin on pads played by both | MIDI and gates keep separate turns, so neither upsets the other |
| Mute or solo a pad from your controller | it stays silent or alone, whether MIDI or a gate plays it |

Round Robin takes turns on the pad's MIDI channel. Notes that reach a pad
through MIDI Pads play it as usual, without turns.

Hold FX In only chooses the Hold FX for gates. From MIDI, use the
[hold buttons](CLIP-MODES.md#hold-buttons) on CC instead.

## Things to Know

### Empty Inputs

An input with nothing plugged in reads 0 V, so Flip In on it switches the Live
Flip mode off.

### Inputs 1 and 2

When inputs 1 and 2 carry audio, don't choose them for Flip In or Hold FX In.
The audio reads as a fast-changing voltage.

### One Input, Several Jobs

One input can record, play pads and duck the mix through the
[compressor's sidechain](COMPRESSOR.md#sidechain) at the same time. It records
first, then plays.

### Other Pad Types

A gate always plays a pad that is not a Clip pad, whatever Gate Action it had as
a Clip pad. Set it back to Clip and its settings return.
