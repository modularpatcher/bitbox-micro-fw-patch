# Bitbox Micro, Unlocked — with MIDI Superpowers

The Bitbox Micro packs a great deal into a narrow module: eight pads that will
play, slice, loop, multi-sample or granulate, eight configurable inputs, eight
outputs, and delay and reverb on board.

This patch does two things to it.

**It frees the controls that were already there.** A long list of settings that
owners have asked about for years exist on the module today — behind a screen,
reached one at a time, with an encoder. They stop being settings you go and find,
and become parameters you play.

**And it opens the compressor, which nobody has ever been able to reach.**

That second one is the bigger claim, so it is worth being plain about. Your
Bitbox has a proper bus compressor sitting across Out 1 and 2. It has a
threshold, a ratio, an attack, a release and a makeup gain, all doing their job
every time you play a note. The module gives you exactly one control over it:
**On** or **Off**. Every other value is fixed, invisible and unreachable — not
buried in a menu, not hidden behind a mod matrix, simply not exposed to anyone by
any means.

Now all five compressor parameters are available over MIDI, and it arrives
configured by default as a glue compressor much like the classic **SSL G-Series
bus compressor** — which you can then adjust live, while you are performing.

### 🎛️ The Controls

Most of them on that pad's own channel, so eight pads answer independently — in
the moment, not buried in a preset:

- ✅ Delay send per pad — *Sample, Multi-Sample, Clip, Slicer, Granular*
- ✅ Reverb send per pad — *Sample, Multi-Sample, Clip, Slicer, Granular*
- ✅ Envelope Sustain per pad — *Sample, Multi-Sample, Clip, Slicer, Granular*
- ✅ LFO Wave per pad — *Sample, Multi-Sample, Clip, Slicer, Granular*
- ✅ LFO Beat Sync per pad — *Sample, Multi-Sample, Clip, Slicer, Granular*
- ✅ LFO Retrigger per pad — *Sample, Multi-Sample, Clip, Slicer, Granular*
- ✅ Stop long clips per pad — *every mode*
- ✅ Reverse per pad — *Sample, Multi-Sample, Granular*
- ✅ Loop Mode per pad — *Sample, Multi-Sample, Granular*
- ✅ Loop Crossfade per pad — *Sample, Multi-Sample, Granular*
- ✅ Granular Density per pad — *Granular*
- ✅ Granular Grain Size per pad — *Granular*
- ✅ Granular Window per pad — *Granular*
- ✅ Granular Scatter per pad — *Granular*
- ✅ Granular Pan Rnd per pad — *Granular*
- ✅ Play Through per pad — *Slicer*

Recorded-sample pads are the one exception, and that is the module rather than
this patch: they have no FX sends in stock firmware to begin with.

These control the one delay effect the whole preset shares:

- ✅ Delay beat-sync
- ✅ Delay ping-pong
- ✅ Delay filter on/off
- ✅ Delay filter width

And these are the new ones — the master bus compressor, never previously
adjustable by MIDI or even on the device, other than on/off:

- ✅ Compressor on / off
- ✅ Compressor Threshold
- ✅ Compressor Ratio
- ✅ Compressor Attack
- ✅ Compressor Release
- ✅ Compressor Makeup gain

---

## The Patch

**It costs you nothing to use.** Not one modulation slot. A pad has twelve, and
every MIDI Learn binding spends one — these spend none. Nothing you have already
built has to give way.

**And you set it up once.** Fixed numbers, baked into the firmware, identical on
every pad of every preset you will ever load. Nothing to map, nothing to learn,
nothing to redo tomorrow.

**You build it yourself.** You run a small script against **your own copy** of
the stock firmware, and it writes out a patched image you flash. Nothing of
1010music's is included, and it refuses to run on anything but a byte-exact stock
2.3.4.

## Why This Exists

Bitbox Micro firmware has been on **2.3.4 since May 2024**. In the same stretch
1010music have updated most of the rest of the range — Bluebox in August 2026,
Blackbox and Bento in July, the Nanobox line in January. The two Bitboxes, Micro
and MK2, are the only products still on a 2024 release. Those dates are all on
their own downloads page.

The module works and has been stable for two years. But if you want these
controls, waiting is not a plan. So somebody went and made them
happen.

---


## Read This First

Five things, briefly. Each links to the full version further down. None of it
should stop you — this is considerably less dangerous than it sounds — but you
should know it before you flash.

- **This is not from 1010music**, not endorsed by them, and unsupported by
  anyone. See [What this is not](#what-this-is-not) and [Support](#support).
- **Never pass the patched image on.** It is yours only because you supplied the
  firmware it was built from. Send people this page and let them make their own —
  it takes a minute. See [What this is not](#what-this-is-not).
- **It cannot brick your module.** The bootloader is a separate program this
  patch never touches, and it is what does the flashing — so a bad image can
  always be replaced. See [Can this brick my module?](#can-this-brick-my-module)
- **It may affect your warranty.** Assume it does and be comfortable with that.
  See [Warranty](#warranty).
- **Some of it costs CPU**, and the granular controls most of all. If something
  crackles, that is the reason these were not offered in the first place. See
  [Why these controls were not already there](#why-these-controls-were-not-already-there).

---


## Contents

**Getting Up and Running**

- [What You Need](#what-you-need) — a Bitbox, a card reader, and your own copy of the stock firmware
- [Patching](#patching) — building your own image
- [Installing](#installing) — getting it onto the module
- [Checking It Worked](#checking-it-worked) — the one thing to look at every time

**Setting Up Your Controller**

- [Which Pad Types Work](#which-pad-types-work) — what is possible where
- [MIDI CC Map](#midi-cc-map) — every number, in one table
- [Encoder Types](#encoder-types) — linear, logarithmic and stepped
- [Use 14-Bit CC](#use-14-bit-cc-on-your-controller) — for feel, not resolution

**The Controls**

- [Playback](#playback) — looping, reverse, and stopping a pad
- [Granular](#granular) — the five the module will not let you modulate
- [Envelope](#envelope) — sustain, and why decay depends on it
- [LFO](#lfo) — wave, sync, retrigger, and how to hear any of it
- [Delay and Reverb](#delay-and-reverb) — per-pad sends and the shared delay
- [The Compressor](#the-compressor) — the new one

**Help and Background**

- [Troubleshooting](#troubleshooting) — when something does not behave
- [Support](#support) — and how to ask for features
- [The Small Print](#the-small-print) — what this is, what it is not, and how it works

## What You Need

Four things, none of them exotic:

- A **Bitbox Micro** running firmware **2.3.4**.
- **Your own copy of the stock 2.3.4 image**, downloaded from 1010music:
  <https://1010music.com/downloads>. The script will not run without it and
  none of it is redistributed here.
- **Python 3.** Any version from the last several years — no compiler, no build
  step, nothing to configure. Where to get it:

  - **macOS** — usually there already.

    The first time you type `python3`, macOS may offer to install its command
    line tools. Say yes and let it finish. It is a big download, so give it a
    few minutes — it has not hung.

  - **Linux** — already there.

    If a minimal install has left it out, your package manager has it under the
    name `python3`.

  - **Windows** — you will need to install it.

    Download it from <https://www.python.org/downloads/> and run the installer.
    On the very first screen, **tick "Add python.exe to PATH"** before you click
    Install. Older installers word it "Add Python to PATH" — same box.

    That one tickbox is the difference between this working and not, and it is
    easy to click straight past. If you do miss it, nothing is broken — see
    *If Windows cannot find Python* below.

- A **microSD card** the module can read. Any working card is fine for this —
  the image is 656 KB and gets read once.

  Tested on a **SanDisk Extreme PRO 64 GB microSDXC** (`SDSQXCU-064G-GN6MA`) —
  Class 10, UHS-I, U3, V30, A2, 200 MB/s. If you are buying one anyway, **V30 is
  the rating worth having**: 30 MB/s sustained, where Class 10 only promises 10.
  That matters for streaming samples while you play, not for flashing firmware.

Two things worth doing first:

1. **Keep the stock `MICRO.BIN`** — and keep 1010music's `MICRO234.zip` while
   you are at it.
   It is how you go back, and the script wants it again every time you re-patch.
   Lose it and your only recourse is 1010music's website.
2. **Back up your SD card.** The patch does not go anywhere near your presets or
   samples, but back it up anyway.

### Presets Built to Make Use of It

Worth knowing before you start, because it shapes what you get out of this.

The controls need no setting up and work on every preset you own from the
moment you flash. But **an old preset will only give you back what it was built
to do.** Eight Sample pads all listening on one MIDI channel, with the delay
turned down and the LFO wired to nothing, will answer barely a third of them —
and none of it is the patch's fault.

The things that matter, in rough order:

- **Give every pad its own MIDI channel.** This is the big one. Per-pad controls
  are addressed by channel, so eight pads on eight channels is eight independent
  sets of controls. All on one channel and every pad answers at once.
- **Pick pad modes deliberately.** The five granular controls do nothing except
  on a Granular pad, and Play Through nothing except on a Slicer pad.
- **Decide which FX parameters you will drive and which you will fix.** The six
  worth mapping — delay time, feedback, cutoff, reverb decay, damping, predelay
  — want parking at their minimum so a CC has room to travel. Whatever you do
  not map is whatever you saved it as, so a reverb with no decay and nothing
  driving it is silence.
- **Point the LFO at something** — the filter is the obvious choice. Three of
  these controls shape an LFO that does nothing until you do.
- **Leave a modulation slot or two spare** for it. Fill all nine CC lanes on a
  pad and there is no room left to route anything, and the module does not warn
  you — the Source box simply stops responding.

Each of those gets a proper explanation further down. The short version is that
half an hour spent on a preset built for this is worth more than any single
control here on its own.

---


## Patching

**You supply the firmware. This project does not.** What you have here is
`patch_micro.py`, this guide and a licence file — three text files and not one
byte of anyone's firmware. The Bitbox Micro image is 1010music's, it is theirs
to distribute, and they do:

> **<https://1010music.com/downloads>** → bitbox micro → **`MICRO234.zip`**

That is a 1010music download, made by you, from them, under whatever terms they
put on it. Unzip it and you get **`MICRO.BIN`**. The version lives in the zip's
name and nowhere in the file's, which is worth knowing before you have four of
them in a folder and no idea which is which.

**Already running 2.3.5-mod?** Patch your **stock** `MICRO.BIN` again with the
new script — not the patched image you flashed last time. The script checks, and
will refuse a file that has already been through it. That is the safety net
working, not a fault. If you no longer have the stock file, download
`MICRO234.zip` from 1010music again.

You run the script from a terminal, in the folder that contains
`patch_micro.py` and `MICRO.BIN`.

**Getting the terminal into that folder**

- **macOS** — open Terminal, type `cd ` (with the space), then drag the folder
  from Finder onto the Terminal window and press Enter. It fills in the path
  for you.
- **Windows** — open the folder in File Explorer, click into the address bar at
  the top, type `cmd` over what is there, and press Enter. (PowerShell does all
  of this just as well if that is what you already use.)
- **Linux** — most file managers have "Open Terminal Here" on the right-click
  menu. The macOS drag trick works in most terminals too.

Then enter the commands one line at a time, pressing Enter after each. Pasting
both at once works, but going one at a time lets you see what each one did.

Do not double-click `patch_micro.py`. It has to be run from a terminal, and
double-clicking it will either open a text editor or flash a black window shut
before you can read it.

**On macOS and Linux**

```sh
mkdir patched
python3 patch_micro.py MICRO.BIN patched/MICRO.BIN
```

**On Windows**

The same thing, but the command is `python` rather than `python3`:

```sh
mkdir patched
python patch_micro.py MICRO.BIN patched/MICRO.BIN
```

The forward slash is right on Windows too — Python reads that part itself, so
the shell never sees it. You do not need to change it to a backslash.

**On all three**, if you have patched before, `mkdir patched` will say the
folder already exists. That is fine — the folder is what you wanted. Carry on
to the next line.

**If it says it cannot open `patch_micro.py`** — a long line ending in
`[Errno 2] No such file or directory` — the terminal is not in the folder those
two files are in. Nothing is missing and nothing is broken. The long path in
that message is where the terminal looked, which tells you where it currently
is. Go back to *Getting the terminal into that folder* above and try again.

The patched image **must also be called `MICRO.BIN`** — exact name, upper case —
for the module to find it. That is why it goes in its own folder: writing the
output over your input would destroy the only stock copy you have, and that copy
is your way back if anything goes wrong. The script refuses if you try.

**That file is yours alone — do not pass it on.** Not to a friend, not in a
forum thread, not to somebody who says they cannot run the script. Point them
here and let them make their own. All of this rests on nobody handing out
1010music's firmware, and a patched image is exactly that.

Copy `patched/MICRO.BIN` to the card, and keep the original where it is.

You should see something like:

```text
  Checking your firmware...
     ok   Genuine bitbox micro 2.3.4, nothing done to it yet
     ok   The code is where the patch expects to find it

  Embellishing your bitbox micro with community requests...
     ok   Compressor on/off, ch 1      CC 40
     ok   Compressor threshold, ch 1   CC 41
     ok   Compressor ratio, ch 1       CC 42
     ok   Compressor attack, ch 1      CC 43
     ok   Compressor release, ch 1     CC 44
     ok   Compressor makeup, ch 1      CC 45
     ok   Granular density, per pad    CC 79
     ok   Granular grain size, per pad CC 80
     ok   Granular window, per pad     CC 81
     ok   Granular scatter, per pad    CC 82
     ok   Granular pan random, per pad CC 83
     ok   Delay send, per pad          CC 84
     ok   Reverb send, per pad         CC 85
     ok   Reverse, per pad             CC 86
     ok   Envelope sustain, per pad    CC 87
     ok   Loop mode, per pad           CC 88
     ok   Loop crossfade, per pad      CC 89
     ok   Play through, slicer pads    CC 90
     ok   LFO wave, per pad            CC 91
     ok   LFO beat-sync, per pad       CC 92
     ok   LFO retrigger, per pad       CC 93
     ok   Delay beat-sync, global      CC 108
     ok   Delay ping-pong, global      CC 109
     ok   Delay filter, global         CC 110
     ok   Delay filter width, global   CC 111
     ok   Stop this pad                CC 120 / 123
     ok   Splash screen, so nobody blames 1010music for my work

  Done. MICRO.BIN is ready -- 672,724 bytes.

     Fingerprint  63c5ec887a9ab6e37f7748f6b0f6838b33d23ae59f97576f5e6c0140674dba68
```

It then tells you how to get the file onto the module, which is also written
out below.

**Check the fingerprint matches the one above.** If it does, you have built
exactly the same firmware as everybody else running this patch, down to the
byte.

Add `-v` if you want to watch it work — addresses, opcodes and byte counts for
every change it makes.


## Installing

1. Copy `MICRO.BIN` to the **root** of the microSD card — not inside a folder.
2. Power the module off and insert the card.
3. Hold the **white right-arrow** button while powering on.
4. Release when the display shows *Erasing* / *Programming*. It takes about
   15 seconds.

Do not interrupt the power while it is writing.


## Checking It Worked

The splash on the next boot should read:

```text
bitbox micro
community fw
2.3.6-mod
```

If it still says `by 1010music` and `2.3.4`, the module booted the old firmware
and nothing was flashed — check the file is named `MICRO.BIN` and is in the card
root, then try again.

**Check this every time you flash.** It is the only reliable confirmation of
which image is actually running, and it takes one second — as against the
twenty minutes you will otherwise spend proving that a CC does not work on
firmware that was never installed.

Then **test the CCs with your ears** — see *Use Your Ears, Not the Screen*
below. For all but two of them the display will not move, on this firmware or
the stock one, so listening is the only way to tell. This catches everybody
exactly once. The two that do show are Reverse and Loop Mode, which makes them
the quickest way to prove your controller is getting through at all.

---


## Setting Up Your Controller

Everything you need to get your controller talking to the module, in the order
you will want it: what works on which pad type, every CC number in one table,
which channel to send each one on, and how to set your encoders up so the
controls feel right under your hand.

You only do this once. After that it is the same on every preset you ever load.

### Which Pad Types Work

| Pad mode | Sends<br>84, 85 | Reverse<br>86 | Granular<br>79–83 | Loop<br>88, 89 | Play Thru<br>90 | Sustain + LFO<br>87, 91–93 | Stop<br>120/123 |
| -------- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| Sample | ✅ | ✅ | — | ✅ | — | ✅ | ✅ |
| Granular | ✅ | ✅ | ✅ | ✅ | — | ✅ | ✅ |
| Clip | ✅ | — | — | — | — | ✅ | ✅ |
| Slicer | ✅ | — | — | — | ✅ | ✅ | ✅ |
| Multi-Sample | ✅ | ✅ | — | ✅ | — | ✅ | ✅ |
| Recorded Samples | ❌ | ❌ | — | ❌ | ❌ | ❌ | ❌ |

Every ✅ above has been confirmed on hardware.

**Multi-Sample is Sample mode**, which is why it matches that row exactly. It is
not a separate pad mode at all — it is a Sample pad with a folder of WAVs loaded
into it, and the manual says as much twice: loading a directory into a pad
"will automatically be changed to Sample", and switching a multi-sample pad to
any other mode leaves it playing only the first file in the folder. So anything
proven on a Sample pad is proven there too.

**Two of the gaps are the module, not the patch.** The Bitbox runs pads through
two different bits of firmware — one for Sample, Multi-Sample and Granular,
another for Clip and Slicer — and they do not have identical features.

**Reverse** simply does not exist in the Clip/Slicer half. There is no setting
there for CC 86 to point at, so it can never work on those pads however it is
asked. **Play Through** is narrower still: it is a **Slicer** parameter, and only
a Slicer one — the manual lists it among the settings specific to Slicer mode,
and a Clip pad has no slices for playback to carry on past. So CC 90 does
nothing anywhere except a Slicer pad.

Loop Mode and Loop Crossfade are the same story — Clip and Slicer pads handle
looping their own way and have no equivalent setting.

Sustain and the three LFO controls, by contrast, **do** work on every pad type.
That took a second technique: the Clip/Slicer half keeps its parameters in a
separate place with two copies of everything, so those arms hand the value to
the firmware's own setter rather than writing it directly.

**Recorded-sample pads have no FX sends in stock firmware either.** The delay
and reverb knobs on the FX screen do precisely nothing on one, even with content
recorded — turn them all you like. That is stock behaviour, confirmed on
hardware by turning them all I liked. Only two routines in the whole
firmware send audio to the FX bus, and both are patched here, so **this covers
every pad type that can use the FX sends at all.** Giving recorded-sample pads
sends would mean building that capability from scratch, which this does not
attempt.

**The compressor is not in this table, because pad type is irrelevant to it.**
It sits on the Out 1/2 bus, after everything has been mixed together, so it acts
on every pad routed there whatever mode that pad is in — and on nothing routed to
Out 3–6. It is the one control here that is not per-pad at all.

---


### MIDI CC Map

| CC | Controls | Send it on | Values | Encoder |
| ---- | -------- | ---------- | ------ | ------- |
| **40** | Compressor on/off | **channel 1 only** | `≥64` on, `<64` off | button |
| **41** | Compressor Threshold | **channel 1 only** | −40 … 0 dB | linear |
| **42** | Compressor Ratio | **channel 1 only** | 1:1 … 20:1 | linear |
| **43** | Compressor Attack | **channel 1 only** | 0.5 … 100 ms | **logarithmic** |
| **44** | Compressor Release | **channel 1 only** | 10 ms … 1 s | **logarithmic** |
| **45** | Compressor Makeup gain | **channel 1 only** | −36 … +36 dB | linear |
| **79** | Granular Density | that pad's MIDI channel | `0`–`127` | linear |
| **80** | Granular Grain Size | that pad's MIDI channel | `0`–`127` | linear |
| **81** | Granular Window | that pad's MIDI channel | `0`–`127` | linear |
| **82** | Granular Scatter | that pad's MIDI channel | `0`–`127` | linear |
| **83** | Granular Pan Rnd | that pad's MIDI channel | `0`–`127` | linear |
| **84** | Delay send | that pad's MIDI channel | `0`–`127`, follows the same curve as the on-screen knob | linear |
| **85** | Reverb send | that pad's MIDI channel | `0`–`127`, same curve | linear |
| **86** | Reverse | that pad's MIDI channel | `≥64` on, `<64` off | button |
| **87** | Envelope Sustain | that pad's MIDI channel | `0`–`127` | linear |
| **88** | Loop Mode | that pad's MIDI channel | three even bands: None / Forward / Bidirectional | stepped, 3 |
| **89** | Loop Crossfade | that pad's MIDI channel | `0`–`127` | linear |
| **90** | Play Through | that pad's MIDI channel | `≥64` on, `<64` off | button |
| **91** | LFO Wave | that pad's MIDI channel | ten even bands across the waveform list | stepped, 10 |
| **92** | LFO Beat Sync | that pad's MIDI channel | `≥64` on, `<64` off | button |
| **93** | LFO Retrigger | that pad's MIDI channel | `≥64` on, `<64` off | button |
| **108** | Delay beat-sync | any channel (global) | `≥64` on, `<64` off | button |
| **109** | Delay ping-pong | any channel (global) | `≥64` on, `<64` off | button |
| **110** | Delay filter on/off | any channel (global) | `≥64` on, `<64` off | button |
| **111** | Delay filter width | any channel (global) | `0`–`127` | linear |
| **120** | Stop this pad — All Sound Off | that pad's MIDI channel | any value; releases the note | button |
| **123** | Stop this pad — All Notes Off | that pad's MIDI channel | any value; releases the note, identically to 120 | button |

Every control the patch adds, on twenty-seven numbers. Write them down somewhere.

**Every one of them has been heard on the module** — nothing in this table rests
on reading the code and hoping.

**There is nothing to set up.** Every one of these is fixed in the firmware and
live on every pad of every preset the moment you flash. You do not map them,
assign them, learn them or save them — send the CC and it works. That is the
opposite of every other CC on the module, which you *do* assign yourself, per
preset, at a cost of one modulation slot each.


### Which Channel to Send On

**CC 79 through 93 and 120/123 are per-pad.** Send them on the MIDI channel that
pad is already set to listen on — the same channel you use to trigger it. Eight
pads on eight channels answer independently.

**CC 108 through 111 are global** and answer on any channel, because they control
the one delay the whole preset shares.

**CC 40 through 45 are the compressor, and they answer on channel 1 only.**

That last one is the odd one out, and it is deliberate. **CC 40 to 45 sit in the
fine-adjust range that almost nothing actually uses.** Formally, CC 32 to 63 are
the LSB partners of CC 0 to 31 — so CC 40 is the low half of CC 8 — but hardly
any device sends those, which leaves the numbers effectively free and means gear
does use them for other things.

"Effectively free" is not the same as free, and this is the one block in the
patch sitting on numbers with a defined meaning. The other patched globals, CC
108 to 111, live in 102–119, which genuinely is undefined. So the compressor is
restricted to a single channel, to stop a stray message from elsewhere in your
rig quietly changing it while you are not looking.

**If the compressor does not respond to anything, this is why.** Put your
controller on MIDI channel 1.


### Encoder Types

The right-hand column is a suggestion for how to set up each control on your
controller, if it lets you choose. It changes the feel, not what the firmware
does — the module always maps a CC evenly across the parameter's range.

**Almost everything wants a plain linear encoder.** Two do not:

**Attack and Release should be logarithmic if your controller offers it.** Both
are times, and times are not heard evenly. Attack runs from 0.5 ms to 100 ms — a
two-hundred-fold range — and the difference between 0.5 ms and 5 ms is enormous
while the difference between 90 ms and 100 ms is nothing at all. On a linear
encoder every useful fast setting is crammed into the first few percent of
travel. Logarithmic spreads them out. Release has the same problem across 10 ms
to 1 second.

**Ratio is a borderline case and linear is fine.** Be aware that the gentle,
musical settings live in the bottom quarter of the throw — 2:1 sits at about 5%
and 4:1 at about 16% — so move slowly down there. If your controller does
logarithmic easily it will feel better, but this is not one to go out of your
way for.

**Loop Mode and LFO Wave are lists, not sweeps — use a stepped encoder.** The
firmware divides the CC range into even bands, one per entry, so what you want is
a detented or stepped encoder with exactly the right number of positions:

| Control | Steps | The list |
| ------- | ----- | -------- |
| **Loop Mode** | **3** | None / Forward / Bidirectional |
| **LFO Wave** | **10** | the ten waveforms, in the order the module lists them |

Set to ten steps, one click of the LFO Wave encoder moves you cleanly to the next
waveform and you never land between two. Set it to anything else and you will
skip shapes or hit the same one twice in a row, which reads as a broken control.

If your controller cannot do stepped encoders, a slow linear sweep still works —
you just have to find the bands by ear, and the edges are easy to sit on by
accident.

**The switches want buttons.** Anything marked `≥64 on` is a toggle. Set it up as
a button sending 0 and 127 rather than as an encoder you have to sweep.

---


### Use 14-Bit CC on Your Controller

Worth doing, and worth understanding why it helps, because it is not the reason
you would guess.

The module widens every incoming CC internally, but on the Control Change path
it reads only the value byte and shifts it — **the LSB of a 14-bit CC pair is
discarded**. Your lovely high-resolution controller sends 16,384 values and the
module cheerfully throws away 16,256 of them. It can land on 128 positions and
no more, however you send them, and there is no getting between them.

What 14-bit mode does change is the *feel*. In 7-bit, one detent is one value,
and on a sweep-heavy parameter that lurches. In 14-bit the controller only
advances the MSB once every 128 internal steps, so the same physical turn sends
far fewer messages and the parameter moves smoothly instead of leaping.

Recommended for anything you sweep by hand:

| | |
|---|---|
| **Filter cutoff, resonance** *(your own lanes, not this patch)* | The two that most obviously step in 7-bit |
| **Granular Density, Window, Scatter** | Slow sweeps are the whole point of these |
| **Delay and reverb sends** | Riding a send wants smooth |
| **Envelope Sustain, Loop Crossfade** | Both step audibly when nudged in 7-bit |
| **Delay filter Width** | Sweeping the filter's bandwidth wants the same smoothing |

Leave the switches on plain 7-bit — Reverse, Play Through, LFO Beat Sync, LFO
Retrigger, and the delay's Beat Sync, Ping-Pong and Filter. They only want to
know which side of halfway you are on, and giving them extra precision to ignore
helps nobody. The same goes for the two banded controls, Loop Mode and LFO
Wave: they divide the range into three and ten steps, so finer resolution buys
nothing.


---


## The Controls

What follows walks the signal path, from what a pad plays through to what leaves
the outputs. Read it in order the first time and the whole thing will make more
sense than a list of numbers ever could.

```
   sample playback   →   envelope   →   LFO        →   delay / reverb sends
   loop, reverse                        modulation
                                                              ↓
   Out 3-6  ←──────────────────────────────────────────  bus sum
                                                              ↓
   Out 1/2  ←──  master volume  ←──  safety limiter  ←──  COMPRESSOR
```

---


## Playback

These decide what the sample actually does when a pad is triggered, and they are
the first thing in the chain.

**Loop Mode** picks between None, Forward and Bidirectional. On a controller it
is three even bands across the throw. Bidirectional runs the sample forwards then
backwards, which is either lovely or seasick depending on the material.

**Loop Crossfade** softens the join. A loop that clicks usually just needs a
little of this. Long crossfades on short loops start to blur the sound, which is
sometimes what you want.

**Reverse** plays the sample backwards. It works on Sample, Multi-Sample and
Granular pads only — Clip and Slicer pads have no reverse setting in the module
at all, so there is nothing for the control to reach.

**Play Through** is a Slicer control and only a Slicer control. It decides
whether playback carries on past the end of a slice into the next one. On any
other pad type it does nothing, because there are no slices to carry on past.

---

### Stopping a Pad

**Stop this pad** answers to the two standard MIDI panic messages — All Sound Off
and All Notes Off — on that pad's own channel. Either one releases the note.

It matters most on long clips. A four-bar loop that you have triggered and now
want gone would otherwise play to its end, and this cuts it cleanly from wherever
your controller sits. Both messages behave identically here, so use whichever
your controller sends more conveniently.

---


## Granular

These five only do anything on a pad set to **Granular** mode, and they are the
part of this patch the module fights hardest. They are the Gran-page controls
1010music marks "Mod Target? No" — no three boxes on the screen, and MIDI Learn
looks straight through them. Until now the encoder was the only way in.

**Density** is how many grains play at once. **Grain Size** is how long each one
lasts. Between them they take you from a recognisable sample to a smeared pad to
a sparse cloud of fragments.

**Window** is the shape each grain fades in and out with. Softer windows sound
smoother; harder ones click and click is sometimes the point.

**Scatter** randomises grain timing, and **Pan Rnd** randomises their position in
the stereo field. Both turn a regular texture into a diffuse one.

**Scatter is the one that will fool you.** It does nothing whatsoever unless
Density is below half — the manual says so plainly, and it is the module's
behaviour rather than anything this patch introduces. Sweep Scatter with Density
up and you will hear nothing and blame the patch. Pull Density down to a quarter
first, where the grains are sparse enough to hear individually, and Scatter's
effect on their timing is obvious.

Speed is not here, and deliberately: the module already marks it "Mod Target?
Yes", so it works over MIDI without any of this.

These are also the ones most likely to cost you CPU — see *Why These Controls
Were Not Already There* in the small print. Density is the one the manual warns
about.

---


## Envelope

**Sustain is the control this patch adds, and Decay is why it matters.**

Decay is how long the sound takes to fall *from its initial peak down to the
sustain level*. So with Sustain at full, Decay has nowhere to travel and does
nothing you can hear — which is exactly why a decay knob so often feels dead.
Pull Sustain down and it comes alive: at zero, the pad falls from its peak to
silence over the decay time, and Decay becomes a **length** control.

### The Hi-Hat, Which Is the Case That Makes It Click

Load a hi-hat and you have two very different instruments in one sample,
depending entirely on where these two sit:

| | Sustain | Decay | You get |
| --- | --- | --- | --- |
| **Open, splashy** | high | long | the hat rings on, washes into the next bar, sits back in the mix |
| **Tight, crisp** | low | short | a short chip that punches and gets out of the way |

Same sample. Same pad. The difference between a hat that drives a track and one
that clutters it is those two controls, and until now Sustain was frozen at
whatever the preset happened to be saved with.

**Now you can move between the two while the pattern is playing.** Open the hats
through a build, tighten them for the drop, and do it from a knob rather than by
loading a different preset. That is the whole argument for putting Sustain on a
CC, and once you have heard it on a hat you will start using it on snares, claps
and anything else with a tail.

**Attack, Decay and Release were always modulation targets**, so you could
already automate those. Sustain never was — which meant you could drive Decay all
you liked while the one control that decides whether Decay does anything stayed
fixed. Both are yours now.

---


## LFO

Three controls here — **Wave**, **Beat Sync** and **Retrigger** — and they shape
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

**Wave** picks the shape — ten of them, as ten even bands across the throw.
**Beat Sync** locks the rate to the tempo instead of running free. **Retrigger**
restarts the LFO from the top on every note, which is the difference between a
wobble that lines up with your hits and one that drifts against them.

**If the bottom knob will not change a `None` slot**, the pad is full. Each pad
has twelve modulation slots, three of which the firmware claims for itself, and a
preset with lots of mapped CCs can easily use the rest. It reads exactly like a
dead encoder. Free a slot by removing a mapping you are not using and it will
spring back.

Worth knowing: the Source list on that screen is longer than the manual suggests.
The MIDI chapter lists the MIDI sources, the CV chapter lists EXT 1–8, and no page
lists the whole thing — LFO is in there, several clicks further round than you
might stop.

---


## Delay and Reverb

**Delay send** and **Reverb send** are per pad, so each of your eight can sit in
the effects by a different amount, changed live. This is the pair people reach for
first, and it is the fastest way to make a static loop move.

Recorded-sample pads are the exception: they have no FX sends in stock firmware
at all, so there is nothing for the control to reach. That is the module, not this
patch.

**The delay itself is shared by the whole preset**, and four of its controls are
here. These answer on any channel:

**Beat Sync** locks the delay time to tempo rather than milliseconds. **Ping-pong**
bounces repeats across the stereo field. **Filter on/off** and **Filter width**
shape the tone of the repeats — darkening each successive echo is the classic dub
move, and width is how aggressively it happens.

The delay's time and feedback are not here, because the module already lets you
modulate them — they work over MIDI without this patch.

---


## The Compressor

**This is the one that was never available to anyone.**

Your Bitbox has a real bus compressor across Out 1 and 2. It has always been
there, it has always been running, and it has always been shaping every note you
play through those outputs. The module gives you one control over it: **On** or
**Off**.

Threshold, ratio, attack, release, makeup gain — all fixed, all invisible, all
unreachable. Not buried three menus deep. Not hidden behind the modulation
system. Simply never exposed, to anyone, by any route. Which is why it is the
most underused feature in the module: not because it is bad, but because a
compressor you cannot adjust is a compressor you cannot use for anything except
the one thing its designer picked for you.

Everything else in this patch takes a control you could already reach and makes
it playable. **This one hands you five parameters that have never been on the
table at all**, and turns a fixed safety net into an instrument you can shape
while the music runs.

### Where It Sits in the Chain

```
   all pads routed to Out 1/2   →   delay and reverb returns
                                              ↓
                                        COMPRESSOR          ← yours now
                                              ↓
                                      safety limiter        ← untouched
                                              ↓
                                      master volume
                                              ↓
                                          Out 1/2
```

Three things follow from that, and they all matter in practice:

**It sees everything.** The compressor is on the sum of every pad routed to Out
1/2, including the delay and reverb returns. This is a mix bus compressor, not a
channel strip — it hears the whole thing at once.

**Pads on Out 3–6 bypass it completely.** That is your clean path, and it is
useful (see below).

**Master volume is after it.** So turning the module down does not change how
hard the compressor works — the setting on the front panel and the setting on
your controller are independent, which is what you want.

**And behind it sits a safety limiter that this patch does not touch.** It is the
last thing before the outputs, it is not adjustable, and it is what stops a
careless setting from reaching your speakers. You can push the compressor hard
without worrying that you will damage anything.

### You Will Want to Know How a Compressor Works

This guide is not the place to learn that, and there are far better resources
than anything that would fit here. But a one-line version, so the controls below
make sense:

> A compressor turns the loud parts down, then you turn the whole thing back up.
> The result is a smaller gap between loud and quiet — which sounds denser,
> steadier, and louder at the same peak level.

The five controls decide *how loud is loud enough to act on*, *how much to turn
it down*, *how fast to react*, *how fast to let go*, and *how much to put back*.

### What Each Control Does

**Threshold** — how loud a sound has to get before the compressor pays any
attention. Everything below it passes untouched. Bring it down and more of the
music gets caught.

**Ratio** — how hard it clamps once the threshold is crossed. At 2:1, a sound
going 10 dB over the threshold only comes out 5 dB over. At 10:1 it comes out 1 dB
over. Low ratios shape; high ratios flatten.

**Attack** — how quickly it reacts. Fast attacks catch the very front of a
transient and squash it. Slow attacks let the initial hit through and only then
clamp down, which is what keeps drums punchy.

**Release** — how quickly it lets go again. Short releases pump and breathe with
the track. Long releases are smoother and less obvious.

**Makeup gain** — how much level you put back afterwards, to compensate for what
the compression took away.

### It Arrives Set Up as a Bus Glue Compressor

You do not have to configure anything. Switch it on and you have gentle,
musical bus compression — the sort that pulls a mix together rather than
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

Three of those four are literally switch positions on an SSL: **2:1**, a slow
**30 ms** attack, and a **300 ms** release. The slow attack is the important one
— it is what lets the front of every kick and snare through before the compressor
clamps, and it is the difference between glue and mush.

**Why −12 dB and 2:1, specifically.** Threshold is the one setting that depends
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
quietly steps aside if you run quiet. It cannot crush you.

**On input level and makeup gain.** Aim to have your mix peaking somewhere
around **−6 dBFS** before the compressor. That is good practice anyway, and it is
the level these defaults are built around. Makeup gain starts at 0 dB, so
switching the compressor on can only ever make things slightly *quieter* — never
louder, never clipping. Once you have set a threshold you like, bring makeup up
until engaged and bypassed sound about the same loudness. That is the honest way
to judge whether it is helping: match the levels first, then compare.

If you run much hotter than −6 dBFS, the compressor works harder and you will
want more makeup. If you run cold, it will barely engage and you should bring the
threshold down rather than reaching for makeup.

### Put It on a Button

If your controller can send several CCs from one button press, **set one up with
the five values in the table above.**

That gives you a home to come back to. Experiment as violently as you like — pull
the threshold to the floor, crank the ratio, make it pump — and when you have had
enough, one press puts you back at a known good glue setting instantly. It turns
the whole thing from something you have to be careful with into something you can
throw around.

### Setting It by Ear

**The module's screen never shows any of this**, and there is no gain reduction
meter. That is true of everything this patch adds — the display does not redraw
when a CC arrives — and a compressor is invisible anyway. Your controller is the
display. If you turn an encoder on the module itself, the module wins.

**Finding the threshold is the only fiddly part**, and there is a technique:

> Bring the **threshold** down slowly while something busy is playing. At first
> nothing happens. Then the loud parts start to lean back a little. **That point
> is the threshold** — leave it just past there. If it starts sounding squashed
> or breathless, you have gone too far.

The others are quicker to hear:

- **Ratio** — sweep it top to bottom on a busy loop. Transparent, then obviously
  squashed.
- **Release** — short, on a kick loop. It should pump hard. The most audible of
  the four by a wide margin.
- **Attack** — long, on the same loop. The click of the kick comes back.
- **Makeup gain** — level, and nothing else.

### Two Things That Will Confuse You Once

**Nothing happens while the compressor is off.** The five settings are
remembered, but nothing applies them until it is running. So if you set
everything up with it switched off and hear no change, that is expected — switch
it on and it all arrives at once.

**Makeup gain at zero is −36 dB and will silence you.** If that encoder happens
to be parked at the bottom when you first touch it, the sound vanishes and it
looks like something has broken. It has not. Turn it up.

### What It Will Not Do

**There is no sidechaining.** You cannot duck the mix from a kick on another
channel, or key the compressor from anything but the signal passing through it.
The detector is wired to the audio it is compressing and there is no key input to
redirect it, so this is a limit of the module rather than something the patch
chose not to expose.

What gets you close: ordinary bus compression already ducks the mix when the kick
lands — that is what glue compression does, and with a fast attack and a loud kick
it pumps convincingly. The difference is that the kick is compressed too. And if
you sequence CCs, you can draw a threshold envelope in time with the kick, which
is automation rather than sidechaining but reaches a similar place.

**There is no wet/dry mix either.** The module cannot blend compressed and
uncompressed signal — there is no mix control and no way to add one.

What you can do instead: **pads routed to Out 3–6 skip the compressor entirely.**
Put the same sample on two pads with the same MIDI channel and Pad Note, route one
to Out 1/2 and the other to Out 3/4, and mix them outside the box. Squash the
compressed one hard and blend it underneath. That is parallel compression, using
stock routing.

It costs a pad and an output pair, so most people will not bother — but it is
there if you want it.

### Settings Last Until You Power Off

Your compressor settings live in the module's memory for as long as it is
switched on, and **they survive changing presets** — unlike everything else in
this patch. A power cycle puts them back to the glue values in the table above.

There is no way to save them into a preset, and no reset control. Power off is
the reset.

---

**Used well, this is the control that makes eight pads sound like one record.**
Everything else here shapes a sound; the compressor is what makes all of them sit
together, breathe together and hit as a single thing. That is what glue means,
and it is why this is the addition worth learning properly.

---


## Troubleshooting

Most things that look broken are one of a handful of well-worn gotchas. In rough
order of how often they catch people:


### Use Your Ears, Not the Screen

**Send a CC and the value changes, but the display does not move.** Push CC 84
up and the delay send really is going up. The FX page carries on displaying
whatever number was last dialled in by hand, with total confidence.

Nothing is broken and the patch does not cause it. The module has always
behaved this way, including for the CCs stock firmware handles itself —
automate CC 7 and the volume changes without the screen ever admitting it. The
patched CCs inherit exactly the same behaviour.

**Two exceptions.** Reverse on CC 86 shows on the display — send it and watch
the pad flip. Loop Mode on CC 88 shows too: the setting steps visibly between
None, Forward and Bidirectional as the CC crosses each third. Make the most of
both, because the rest will tell you absolutely nothing.

**So test the rest with your ears.** Send the CC, listen for the delay or the
reverb coming up underneath the pad, and ignore the display completely. Sit
watching the FX page waiting for a number to move and you will conclude the
patch is dead at the exact moment it is working perfectly.


### Getting the Full Range Out of a CC

This is about the module rather than the patch, but it will catch you out the
first time and it looks exactly like a broken mapping.

A CC mapped to a parameter **through a preset** moves that parameter *upward
from whatever value it was saved with*. It does not sweep the full range. Save a
parameter sitting halfway and your CC reaches the top half of it and stops,
looking for all the world like a broken cable.

So before you save, set the parameter to its **minimum** on the device:

| Parameter | Set it to |
| --- | --- |
| Delay time, Beat Sync on | `1/64` — first entry in the list |
| Delay time, Beat Sync off | `0%` |
| Delay feedback | `0` |
| Delay cutoff | `0` — see the note below, it is not what it looks like |
| Reverb decay, damping, pre-delay | `0` |

Now the CC has somewhere to go.

Two things worth knowing:

**Cutoff reads oddly.** The screen shows it running negative for low-pass and
positive for high-pass, but it is stored as a plain 0-1000 with the midpoint as
neutral. So its minimum really is zero — it just means fully low-passed, and
the delay will sound very dark until the CC brings it up.

**Your preset will sound different unmodulated.** Feedback at zero is a single
slap rather than a delay, so the preset is duller before any MIDI arrives. That
is the trade for full CC range, and it is only worth making on parameters you
really are going to drive.

**None of this applies to any CC in this patch.** Every one of them is
written directly rather than through the modulation system, so they all cover
their full range no matter what the pad was saved with. The rule above is about
the CCs you map yourself in a preset.


### Changing Preset Wipes Everything You Sent

Load a new preset and every value you set by CC is gone. The pad comes up with
whatever that preset was saved with, and nothing you did with your controller
is remembered.

So if you had a pad sitting in the delay, it will not be in the delay any more —
the new preset's saved send is what you get, which is usually nothing. Same for
grain size, sustain, loop mode, all of it.

**Resend your CCs after every preset change.** Most controllers can push all
their current values on demand — a snapshot, a "send all", or whatever yours
calls it. Fire that after loading and you are back where you were in a second.

This is the module, not the patch. A CC mapped through a preset behaves the same
way, and so does CC 7 on stock firmware.

**The compressor is the exception.** Its settings are not stored in a preset at
all, so loading one leaves them exactly as you left them — the five parameters
because the patch holds them, and the on/off state because the module keeps that
with its own global settings. Everything else in this list is wiped.


### Can CV Control These?

**No — and the patch does not change that.**

CV reaches parameters through the **modulation system**, the same route a
preset-mapped CC uses. What this patch adds is a **MIDI Control Change** path
that deliberately bypasses that system: the value is written straight into the
pad, which is the only reason the granular five work at all. The module refuses
them as modulation destinations, and CV has no other way in.

So a CV input cannot drive any of these controls, however creatively you patch it.
I did check.

**What does work:** anything that converts CV to MIDI CC. From the sending end
these are entirely ordinary Control Changes with no secret handshake, so a
CV-to-MIDI converter puts your voltage straight back in the game. That is an
extra box in the chain rather than a limitation of the module, but it is a real
answer if CV is how you want to play them.

---

### If Windows Cannot Find Python

Two different things can happen here, and neither means you have broken
anything.

**The Microsoft Store opens instead of running anything.**

Python is not installed yet, and Windows is offering to sort that out. Take the
Store's copy or install from python.org as above — both work.

**It says `'python' is not recognized`.**

Python is installed, but Windows does not know where to find it: the "Add
python.exe to PATH" tickbox was missed during the install. Before you reinstall
anything, try `py` instead of `python`:

```sh
py patch_micro.py MICRO.BIN patched/MICRO.BIN
```

`py` is a small launcher that comes with Python and gets added to PATH whether
or not you ticked that box, so it usually works when `python` does not.

If `py` does not work either, install Python again from
<https://www.python.org/downloads/> and tick **Add python.exe to PATH** this
time. Nothing is wrong with the script or your download.


### If It Stops with an Error

Good. That is the safety check earning its keep. It will tell you what it found
and what it expected, and it will not have written a thing.

- *"That is not the stock 2.3.4 firmware"* — the input is a different version,
  already patched, or a truncated download.

  **If you are upgrading from an earlier version of this patch, this is almost
  certainly why.** You have pointed the script at the patched image you flashed
  last time rather than at stock. Patch your original `MICRO.BIN` again — the
  one straight out of `MICRO234.zip`. There is no upgrade-in-place path, and
  there deliberately never will be: every safety check in the script is written
  against stock bytes, which is exactly why it caught this. Download
  `MICRO234.zip` from 1010music again if you no longer have it.
- *"The code at … is not what it should be"* — the file passes the checksum but
  the machine code underneath is wrong, which should be impossible. Something
  strange is happening and forcing it will not make it less strange.

The script checks every site before it writes anything, so there is no such
thing as a half-patched file. It is all or nothing, and it prefers nothing.

---

### Going Back to Stock

**The bootloader is untouched.** It lives in a separate region of flash that
this patch never writes to, and it is what performs the update. That means a
bad firmware image cannot stop you flashing a different one.

To go back to stock: rename your original 1010music download to `MICRO.BIN`, put
it in the card root, and follow the same install steps. The module overwrites the
patched image with it and forgets any of this ever happened. This is also the
answer if you simply decide you preferred it before.

## Support

There isn't any. This is one person's patch, given away as-is, and there is
nobody on the other end of it.

**What there is instead is everyone else.** None of this is theoretical — the
controls have been used on real hardware, and where something has not been
confirmed this guide says so rather than hoping. Anyone else running the patch
is running exactly the same controls on exactly the same numbers, so
their answer transfers straight to you, which is more than most support desks
manage.

Somewhere general is the place to ask — **ModWiggler**, or **r/modular** and
**r/eurorack** on Reddit. Please do not take it to 1010music's forum or their
Discord: this is not their work, the people answering there are trying to help
with the actual product, and a queue of questions about a stranger's patch is
exactly the sort of thing that makes companies wish modifications like this did
not exist.

And before you ask anywhere, "it doesn't work" is one of seven things roughly
every time, and you can rule out all seven in about two minutes.

**1. Did it actually flash?** The splash must read `community fw` / `2.3.6-mod`.
If it still says `by 1010music`, nothing was installed and nothing below will
work. Check the file is named `MICRO.BIN` and sits in the card root.

**2. Right channel?** These are per-pad. Send on the channel that pad is set to
— the one you trigger it with, not a global one. This is the second most common
answer, and the second most annoying.

**3. Right pad mode?** CC 79–83 do nothing except on a **Granular** pad. CC 86
(Reverse) and CC 88/89 (the loop pair) do nothing on Clip or Slicer. CC 90
(Play Through) does nothing on anything *except* a Slicer pad. Nothing works
at all on a recorded-sample pad. The table under *Which pad types work* has the
lot.

**4. Listening or looking?** The screen does not redraw for MIDI, on this
firmware or the stock one. It is not sulking. Judge it by ear — except for
Reverse on CC 86 and Loop Mode on CC 88, which both show on the display and are
therefore the fastest way to prove your controller is reaching that pad at all.
If either of those responds and nothing else does, your MIDI is fine and the
answer is somewhere in the other six checks.

**5. Just changed preset?** Everything you sent by CC is gone — see *Changing
preset wipes everything you sent* above. Resend and try again.

**6. Scatter, or an LFO control, doing nothing?** Two different traps.

*Scatter* has no effect at all unless **Density is under 50% as shown on the
device** — which is **CC 63 or lower**, since CC 64 already lands a shade above
half. That is the module's behaviour, documented in the manual.

*The LFO controls* change how the LFO behaves, and the LFO only makes a sound
once you have pointed it at something. On a fresh pad it is wired to nothing, so
Wave, Beat Sync and Retrigger will all appear dead. Route the LFO to the filter
first — see the *LFO* section above — and they come alive together.

**7. Does the encoder do it?** This is the one that settles it. Turn the same
parameter by hand on the device. **If the encoder doesn't change it either, the
patch is not your problem** — you are asking for something the module does not
do, and no quantity of MIDI will talk it round.

*The one exception is the stop pair, CC 120 and 123.* There is no encoder for
stopping a single pad — it is the only thing here the module cannot do by hand.

Survive all seven and you have found something real. Write down what you did and
what happened, in that order.

### Feature Requests

**The low-hanging fruit is already picked.** These came from working
through the manual's parameter tables, taking everything owners had asked for
that could actually be reached, and stopping where the reaching got hard.

If something you want is not here, it is worth knowing why before you ask —
because in a good many cases you can have it today, without a patch at all.

**Almost none of it is new.** All but one were already adjustable on the module,
already worked, and already did exactly what they do now — you just had to reach
over and turn them by hand. All this does is teach the firmware to listen for a
MIDI message it was already ignoring, then hand the value to the module's own
code, untouched. The Bitbox does the work. The patch does the introductions.

Stopping a pad barely counts as an exception either: it uses the module's own
stop, which was already there. What is new is being able to aim it at one pad
instead of all of them.

**The compressor is the one real exception**, and even then the DSP is
1010music's. It has always existed and always worked — it has been shaping your
output every time you played a note. What it never had was any way to adjust it,
which is a different thing from not being there. The patch does not add a
compressor; it adds the controls the compressor never had.

That is the whole design, and it is why it is as safe as it is: nothing is
reimplemented, so there is nothing new to get wrong.

**So anything that does not already exist is out of reach.** A control that is
not on the screen is not a CC away from working — it is a feature somebody would
have to build, from scratch, inside a firmware nobody has the source to. That is
a different undertaking entirely, and no amount of MIDI plumbing gets you there.
If the module cannot already do the thing by hand, this patch cannot make it.

**And the ones that do exist are still not free.** Every parameter is its own
reverse-engineering job: find where the value lands, work out what the setter
actually does, and establish whether anything has to be recomputed afterwards.
Some turn out not to be reachable at all, and you only find that out after doing
the work. And the whole thing is pinned to firmware 2.3.4, so every release
1010music ship invalidates it.

Before asking, **check the manual's parameter tables.** Every one has a
"Mod Target?" column, and anything marked **Yes** already works over MIDI
through an ordinary preset mapping — no patch required, and no waiting for
anyone. That column is also how this patch decided what was worth doing: only
the **No** rows need firmware at all. One control was patched in an early build
before anyone thought to look, and dropped again once they did.

This exists because one person wanted these controls badly enough to
spend their evenings reading ARM assembly. That is not a scalable support
model.

**But the tools are in your hands.** `patch_micro.py` is short and heavily
commented: every hook address, every struct offset, the exact case entry each
granular arm branches to, and the reasoning behind each choice — including the
two approaches that were tried and rejected, and why. Run it with `-v` and it
shows you every address as it works.

If you want another parameter, that file tells you how the existing ones were
done, and adding one is a far better outcome than asking for it.

### When a New Firmware Comes Out

The patcher will refuse it, and it is being stubborn on your behalf. The
addresses it writes to are specific to 2.3.4; on a different build they point
somewhere else entirely, and somewhere else is not a good place to write. Do
not try to work around the version check — someone has to redo the reverse
engineering first.

---


## The Small Print

Everything below is detail rather than instruction. Worth reading once, ideally
while something is rendering; definitely not worth reading before you flash.

### What This Is Not

**It is not from 1010music, and it is not endorsed by them.** It is an
unofficial modification made by a customer with too much time. The patched image
says `community fw` on the splash screen precisely so it can never be mistaken
for one of their releases.

**Do not ask 1010music for support with it.** If something misbehaves while
you are running this, flash the stock firmware back before reporting anything
to them, and confirm the problem still happens. They cannot be expected to
debug someone else's changes to their product.

**It is not a firmware distribution.** No part of 1010music's firmware is
included here. The script contains addresses and offsets — facts about where
things sit in the binary — and machine code written from scratch. It cannot
produce anything unless you supply the stock image yourself, and it refuses to
run on anything except the exact 2.3.4 build.

**And it must not become one.** The image you produce is yours only because you
supplied the firmware it was built from. Send it to somebody else and that stops
being true: you are handing out 1010music's code with thirty-one bytes
changed. Send them this page instead. The script takes seconds to run, and they
will have their own copy inside a minute.

**It is not a general feature framework.** It adds the controls listed above and
nothing else. It does not add sends to recorded-sample pads (see below), does
not touch the audio engine, and takes no interest whatsoever in anything the
modulation system can already reach.

**It is not tested to a commercial standard.** It is one person's patch,
verified on one unit. It comes with no warranty of any kind. You
are modifying your own hardware at your own risk.

### Was This Built with AI?

Yes, and a good deal of it — reading ARM assembly, testing where a value lands,
working out why a setter did something unexpected. It would have taken far longer
without.

What it did not shorten was the testing. That was many hours at the module
itself, by hand and by ear, one control at a time, on every pad type it was
supposed to work on — and a fair few it turned out not to.

The fair question is whether that should put you off, and the answer is in how
you check it rather than in how it was written. **Every control in the table was
confirmed by ear on real hardware**, and where something has not been confirmed
this guide says so. A patch either does what it claims or it does not, and
listening settles it.

Nor do you have to take any of it on trust. The script is short and heavily
commented, it refuses to run on anything but a byte-exact stock 2.3.4, it changes
31 bytes, and it prints a fingerprint so you can confirm you built the same file
as everybody else. The bootloader is untouched, so the worst case is a reflash.
What should worry anyone is unreadable code shipped as a binary you cannot check.
This is the opposite of that.

In short — it does not matter whether AI helped build it, so long as it works
for you. Flash it, listen, and judge it on that.

### Can This Brick My Module?

**No.** Not because the script is careful, though it is, but because of how the
module is put together.

The thing that flashes firmware is the **bootloader**, and it is a separate
program in a separate region of flash. It is not in the file you are patching,
and this patch never writes anywhere near it.

You can prove that from the firmware image itself. The words **`Erasing`,
`Programming`, `Verifying` and `Bootloader`** — the ones on screen during an
update — appear **nowhere in the firmware file**. They belong to the bootloader,
which has its own display code and its own SD card handling and never calls into
the firmware at all. The firmware cannot even write to flash: the unlock
sequence needed to do it is not in the image.

And the update is triggered by **holding a physical button at power-on**, before
any firmware runs at all. The recovery path does not depend on the firmware
working, which is a good property for a recovery path to have. A completely
broken image still cannot stop you flashing a good one.

**Worst realistic case:** the module boots oddly or not at all, you hold the
white right-arrow, flash the stock image back, and you are where you started
about two minutes later.

Some other things that make this narrower than it sounds:

- **31 bytes of the original image change**, and 1,348 are added at the end.
  Nothing is deleted, moved or overwritten.
- **Size is not a problem.** Official releases already differ by about 4 KB
  (2.2.9 is 667,324 bytes, 2.3.4 is 671,376), so the bootloader has no fixed-size
  assumption to violate.
- **The patcher refuses more than it accepts.** Wrong firmware version, a file
  that has already been patched, any patch site not holding the exact
  instruction it expects, or an output path that would overwrite your stock
  copy — each stops it before a single byte is written.

**The one genuine risk is losing power during the write**, which takes about
15 seconds. That is true of official 1010music updates too, and even then the
bootloader survives, so you just do it again. Do not flash from a laptop that is
about to sleep or a supply you do not trust.

See *If something goes wrong* above for the exact steps back to stock.

### Warranty

**Assume that running modified firmware can affect your warranty, and be
comfortable with that before you flash anything.** That is the safe position,
and nothing below changes it.

Check 1010music's own warranty terms yourself rather than trusting a stranger's
readme on the subject, and bear in mind they are perfectly entitled to take a
dim view of a modified unit.

Practical position:

- **Flash the stock firmware back before sending a unit in for service.** It
  takes a couple of minutes, restores the module to the state it shipped in,
  and removes the question from the conversation.
- **Be straight if you are asked.** If the fault could plausibly be connected
  to running modified firmware, say so. Restoring stock to tidy up an unrelated
  hardware fault is reasonable; using it to hide a relevant detail is not, and
  it is how goodwill for projects like this gets destroyed for everyone.
- **This patch cannot physically damage the module.** It changes 31 bytes of a
  firmware image, and the worst case is a reflash — see *Can This Brick My
  module?* above.

### Why These Controls Were Not Already There

Worth saying plainly, because it is the thing most likely to bite you:

**A missing feature is not always an oversight.** 1010music build the firmware,
they know what the processor has left at the end of a block, and they decided
what to expose. Some of what this patch opens up was very likely left closed on
purpose — and CPU load is the obvious candidate. The manual is explicit that
more grains cost more processing, so sweeping a granular parameter hard while
eight pads are running is exactly the kind of thing a manufacturer would
choose not to hand you.

What that means in practice: **your mileage may vary.** These controls work.
Whether they work in *your* preset, at *your* polyphony, with *your* sample
lengths, is something only your module can tell you. If you hear crackling,
dropouts or a pad choking, that is not a bug to report — that is the reason
the control was not offered, arriving on schedule. Ease off the control or
simplify the preset.

None of this is a reason not to use it. It is a reason to find out where the
edge is at home, rather than in front of people.

### Known Limitations

The honest list.

- **2.3.4 only.** The patch works by writing to specific addresses in that exact
  build. It refuses every other version, including future ones — which is the
  correct behaviour, since those addresses would point somewhere else entirely.
  A new 1010music release means this has to be redone, not just re-run.
- **This adds no modulation targets.** These are all driven from outside, by
  MIDI Control Change, and that is the only route in. The module's own modulation
  system is untouched: its LFOs and envelopes still cannot be pointed at the
  granular controls, the sends, or any of the rest, and the manual's
  "Mod Target? No" is as true after patching as it was before. External
  sequencer or controller, yes. Internal modulation, no.
- **No support for recorded-sample pads**, for the reasons above.
- **Values are set, not smoothed.** A CC writes the value directly, exactly as
  turning the encoder does. Fast sweeps can sound stepped — see the 14-bit note
  above, which is the fix for that.
- **Start, Length, Loop Start and Loop End are not included**, and would not be
  much use if they were. All four already work over MIDI without any patch, but
  placing a start point or a loop region is a thing you do **by eye** — you are
  looking at the waveform and putting a marker somewhere. Turning a knob with
  nothing to look at is not the same job, and the module's own screen will not
  help, because it does not redraw for MIDI. Use the encoders for these; that is
  what they are good at.

#### Delay time and beat-sync

The delay keeps **two** separate time values and beat-sync decides which one the
engine actually reads. So a CC mapped to delay time works beautifully until you
switch beat-sync off, at which point it goes completely dead — because your CC
is still faithfully driving a parameter nobody is listening to any more.

Map both destinations to the same CC and the problem evaporates. This is a
preset-level quirk and nothing to do with the patch; it was here first.

**LFO Rate has exactly the same shape**, for exactly the same reason — one
control on screen, two parameters underneath, and BeatSync picking between
them. If your LFO Rate knob dies the moment you flip that switch, this is why,
and the same fix applies.


### How It Works, Briefly

Five small blocks of code are bolted onto the end of the firmware image, and four
existing instructions are pointed at them:

- Two hooks extend the per-pad Control Change handlers — there are two, because
  Sample-family and Clip-family pads use different voice classes with different
  memory layouts. **Any CC the patch does not claim takes the original path
  completely unchanged.**
- One hook extends the MIDI dispatcher to translate CC 108 to 111 into the same
  internal messages the FX screen knobs send. The delay has no CC handler of its
  own, and the modulation system cannot drive an on/off switch, so this is the
  route that works.
- Two more handle the compressor, and they work differently from everything else
  here — see below.

Four different techniques, depending on what the parameter needs:

**Written straight into the pad** — the two sends, Reverse, Play Through. Their
setters do nothing the patch needs, so there is nothing to gain by going the
long way round.

**Jumped into the firmware's own code** — the granular five, Sustain, the loop
pair and the LFO three, on Sample-family pads. Their setters do real work: one
indexes a lookup table, one clamps, one calls a converter, Density triggers a
recompute. Rather than reimplement any of that, those arms rescale the value and
branch into the firmware's handler for that parameter, which then does exactly
what it does when you turn the encoder. This is possible because on those pads
the Control Change handler and the parameter setter are the *same function*,
sharing one stack frame.

**Handed to the firmware's setter as a call** — the LFO three on Clip and Slicer
pads. There the Control Change handler and the setter are *separate* functions,
so there is no case to jump into; the arm builds the small message the setter
expects and calls it.

**Written into both copies at once** — Sustain on Clip and Slicer. That half of
the firmware keeps every parameter twice, in two places, and its own setter
writes both every time. So this arm writes both as well, rather than setting one
and leaving the two halves disagreeing about how loud the pad is.

In total **31 bytes of the original image change** — 14 of them executable code,
the other 17 being splash text — plus 1,348 bytes appended. Nothing is removed,
relocated, or overwritten.

**The compressor is the exception to all of the above.** Every other control in
this patch hands its value to 1010music's own code and gets out of the way — that
is the whole safety argument for the rest of it. The compressor has no such
handler to hand anything to, because none of its parameters were ever meant to be
set from outside.

So the patch keeps those five values itself and applies them to the audio path
continuously, rather than writing them once and hoping. That is why they survive
a preset change when nothing else does, and why a power cycle is what resets
them. It is a bigger step than the rest of the patch takes, and it seemed worth
saying plainly rather than letting it pass as more of the same.

### Why Fixed Numbers, and Not MIDI Learn

The obvious question. The module already has MIDI Learn — why not make these
learnable and let everyone pick their own?

Because MIDI Learn works through the modulation system, and the modulation
system is exactly what cannot reach these controls. That is the entire reason
they needed patching in the first place. Making them learnable would mean first
making them modulation destinations, and then teaching the modulation path to do
the clamping, the lookup and the recompute their own setters already do
perfectly well. That is a much bigger project and a far better way to break
something.

Two consolations, and they are real ones rather than excuses.

**Fixed CCs are free.** A learned binding occupies a modulation slot, and a pad
only has twelve — three of which the firmware claims before you start. These
controls use none at all. The value goes straight into the pad and the
modulation system is never troubled. That matters more than it sounds: those
slots are also what you need to point the LFO at anything, so every one this
patch does *not* spend is one you can.

**They work everywhere.** A learned binding belongs to one parameter, on one pad,
in one preset. CC 84 is the delay send on every pad in every preset you will ever
load. Set your controller up once and never think about it again.

The cost is that changing them means re-patching. If they clash with something in
your rig, see below.

### Choice of Numbers

Stock firmware handles four CCs per pad on its own — **CC 1** (modwheel),
**CC 7** (volume), **CC 10** (pan) and **CC 64** — and nothing above 64. These
all clear those comfortably.

They also avoid the numbers the MIDI spec reserves: 96/97 (Data Increment and
Decrement), 98/99 (NRPN) and 100/101 (RPN), any of which a controller or DAW may
treat specially. That reserved run is the gap you can see in the map between 93
and 108.

The two exceptions are deliberate. **CC 120 and 123 are Channel Mode messages** —
All Sound Off and All Notes Off — and stopping a pad is exactly what they are
for, so the patch answers them rather than inventing a number of its own. Your
DAW almost certainly already sends them on stop, which is a feature.

The run starts at 79 to leave the low numbers clear for the per-pad lanes you
assign yourself, and pauses again at **102–107**, left free for global FX ones
you assign the same way. Past that the
choice is arbitrary — a free block that suited the author's rig, not the result
of surveying everything a module might conceivably be sent. If they clash with
something in your setup, they are constants at the top of the script and can be
changed before you patch. You will then be running a build nobody else on earth
has, so write down what you did somewhere you will find it.

---


## Licence and Attribution

**This is free.** Free to download, free to use, free to pass on, and there is
nothing to buy at any point. No trial, no unlock code, no email address to hand
over, no tip jar.

If you ever meet this behind a paywall, or bundled into something you paid for,
or attached to a wallet address of any description — somebody has put it there
who did not write it, and you should treat everything else they are offering
you with the same suspicion.

MIT — see the `LICENSE` file. It covers the patcher and this guide, and nothing
else: the Bitbox Micro firmware is 1010music's, is not included here in any
form, and no permission over it is granted or implied — not over the stock image
you supply, and not over the patched image the script makes from it.

The patch script is the author's own work and contains no 1010music code.

Bitbox Micro is a product of 1010music LLC. The name is used here only to say
what hardware this runs on. This project is not affiliated with, endorsed by, or
supported by 1010music.

No warranty, expressed, implied or wished for. You are modifying your own
device, and the consequences are entirely yours to enjoy.

---

## If You're from 1010music

Hello. Genuinely — thank you for the Bitbox Micro.

People keep this module in their racks for years and are still finding new
corners of it, which is a rare thing and the only reason any of this exists.
Nobody spends their evenings reading ARM assembly for a device they feel
lukewarm about.

**I have tried to do this properly.** No firmware of yours is included here,
in any form — not the stock image, not a patched one, not a fragment. Everyone
downloads `MICRO234.zip` from your site themselves, under your terms, and the
script refuses to run on anything else. What ships is one Python file, a guide
and an MIT licence covering only those. The patched image says `community fw` on
the splash for one reason: so it can never be mistaken for your work, and so
nobody turns up at your support desk with my changes on their module. The
guide tells them to flash your firmware back before they contact you, and to be
straight with you if they do.

**And nothing here is clever — it stands entirely on your shoulders.** All but
one are yours already: your parameters, your setters, your clamping and lookups
and recomputes, doing exactly what they do when somebody turns the encoder. The
last uses your own stop, only aimed at a single pad. The patch teaches the
firmware to listen for a MIDI message it was already ignoring, then gets out of
the way — all of thirty-one bytes changed.

I reimplemented nothing, and not out of restraint — there was no need. You had
already written every bit of it properly, and the most useful thing anyone
outside the company could do was leave it alone and knock.

**What I would genuinely love is for this to become unnecessary.** If an
official update brought these controls to the micro — or a better set, chosen
by people who know the codebase rather than inferred from the outside — every
owner would get them. No terminal, no checksum, no trusting a stranger's Python
script, no version pinning that breaks the moment you ship anything. That is a
far better outcome than this one, and I would retire this cheerfully, take the
page down, and point everyone at you.

Until then, this is your customers' wish list, written down in one place by
somebody who loves this thing.

---


## Now Go and Make Some Music — and Twiddle Some Knobs

Every one of them, on eight pads, all under your fingers at once — or
under your sequencer's. These are ordinary Control Changes, so anything that
sends MIDI can drive them: a hardware sequencer running CC lanes, a controller
with faders you can ride, a modulation source going out through a CV-to-MIDI
box. The module cannot automate them itself, and that is the point — you bring
the movement to it.

Some places to start. A few of these combine the patched controls with CCs you map
yourself — delay time, feedback and damping are preset mappings, not part of the
patch:

- **Throw the vocal into the delay and wind the repeats in.** Send it in a bar
  out, then step the beat-synced time down as you approach — quarter, eighth,
  sixteenth — so the echoes stack faster and faster and the tension screws up
  with them. Then haul the send out dry as the drop lands, before anyone has
  worked out what you did.
- **Sequence the sends into a wash as the break comes up** — put CC lanes on
  your sequencer and let it run: reverb climbing on the vocal and the synths
  over sixteen bars, delay opening underneath them, while the drums stay bone
  dry and keep the floor. Then choke it in the last bar rather than yanking it:
  feedback down so the repeats stop regenerating, damping up so the tail goes
  dark and dies early. The wash collapses in on itself and the drop lands in
  clean air, which is a very different thing from someone switching the reverb
  off. That is the per-pad part earning its keep — drench what should swim,
  leave the kicks and percs alone.
- **Grind a vocal into a cloud and back** — Grain Size down and the phrase
  shatters into fragments. Window to zero and it stops travelling through the
  file, stuttering on a single syllable. Density up for a solid wash, or low
  with Scatter wide to throw what is left off the grid, and Pan Rnd to spread
  it around you. Walk all five back over a bar and the vocal steps out intact,
  as if nothing happened.
- **Flip a pad backwards on the fly** — a reverse cymbal swelling up into the
  transition, a reverse snare pulling you into the bar, or a hat turned round
  halfway through a phrase for a bit of IDM. Forward again on the beat, one pad
  at a time, while everything else runs straight.
- **Filter the echoes down to nothing** while the dry signal stays bright, then
  open them up again as everything comes back in.
- **Knock the delay off beat-sync and wind the time down to milliseconds** —
  out of tempo and into comb-filter country, where the repeats stop being
  echoes and start being resonance. Sweep it from there and the whole thing
  rings and detunes with you.
- **Point an LFO at the filter on a bassline and reshape it live** — take the
  wobble from a triangle to a square mid-bar, snap it to the clock, have it
  retrigger on every note so it starts clean each time. Same bass, different
  animal, and none of it stopping to touch the module.
- **Kill a clip that has outstayed its welcome** the moment you decide, and let
  the other seven run.
- **Ride the sustain on a held pad** so it breathes with the arrangement instead
  of sitting still.
- **Squeeze the whole mix and let it go.** Pull the compressor's threshold down
  through a build and everything tightens and leans back together; take it off as
  the drop lands and the mix opens up like a held breath let out. Shorten the
  release and the whole track starts pumping with the kick. This is the one
  control here that acts on all eight pads at once, and it is the difference
  between eight things playing and one record moving.

All of it per pad — apart from the compressor, which takes the lot — and all from
wherever your hands already are. No stopping, no leaning over, no hunting for a
page.

Start with the delay send. It is still the most fun of the lot: throw one pad
into it on the drop and see what happens.

Then spend an evening with the compressor. It is the one thing here nobody has
been able to play with before, it is already set up as a glue compressor, and it
is the control that makes the other seven sound like they belong together.

Enjoy your Bitbox Micro, enhanced with MIDI superpowers. It was a wonderful
little thing already, and it is a better one now.
