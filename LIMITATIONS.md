# Bitbox Micro Firmware: Limitations

What this does not do, and what it does to your firmware. Worth reading before
you flash, and it is short.

## What It Will Not Do

Three things the module does not do, whatever you send it. None is caused by
the patch, and each is listed with whatever there is to do about it.

### Use Your Ears

Send a CC and the value changes. The display does not move. Push CC 84
up. The delay send really is going up. The FX page carries on displaying
whatever number was last dialled in by hand, with total confidence.

Nothing is broken and the patch does not cause it. The module has always
behaved this way, including for the CCs stock firmware handles itself: automate
CC 7 and the volume changes without the screen ever admitting it. The
patched CCs inherit exactly the same behaviour.

Two exceptions. Reverse on CC 86 shows on the display, send it and watch
the pad flip. Loop Mode on CC 88 shows too: the setting steps visibly between
None, Forward and Bidirectional as the CC crosses each third. Make the most of
both. The rest will tell you absolutely nothing.

So test the rest with your ears. Send the CC, listen for the delay or the
reverb coming up underneath the pad. Ignore the display completely. Sit
watching the FX page waiting for a number to move and you will conclude the
patch is dead at the exact moment it is working perfectly.

### A Preset Change Wipes It

Load a new preset. Every value you set by CC is gone. The pad comes up with
whatever that preset was saved with, and nothing you did with your controller
is remembered.

So if you had a pad sitting in the delay, it will not be in the delay any more:
the new preset's saved send is what you get, which is usually nothing. Same for
grain size, sustain, loop mode, all of it.

Resend your CCs after every preset change. Most controllers can push all
their current values on demand: a snapshot, a "send all", or whatever yours
calls it. Fire that after loading and you are back where you were in a second.

This is the module, not the patch. A CC mapped through a preset behaves the same
way, and so does CC 7 on stock firmware.

The two devices are the exception. Neither the compressor's settings nor the
EQ's are stored in a preset at all, so loading one leaves both exactly as you
left them. The patch holds those values itself, and the module keeps the
compressor's on/off state with its own global settings. A power cycle is what
resets them, not a preset change. Everything else in this list is wiped.

### Can CV Control These?

No, and the patch does not change that.

CV reaches parameters through the **modulation system**, the same route a
preset-mapped CC uses. What this patch adds is a **MIDI Control Change** path
that deliberately bypasses that system: the value is written straight into the
pad, which is the only reason the granular five work at all. The module refuses
them as modulation destinations, and CV has no other way in.

So a CV input cannot drive any of these controls, however creatively you patch it.
I did check.

**What does work**: anything that converts CV to MIDI CC. From the sending end
these are entirely ordinary Control Changes with no secret handshake, so a
CV-to-MIDI converter puts your voltage straight back in the game. That is an
extra box in the chain rather than a limitation of the module, but it is a real
answer if CV is how you want to play them.

## Strict Build by Design

The patcher will not run unless two things hold: your input file is
byte-for-byte stock 2.3.4, checked by sha256, and every site it touches still
holds the exact instruction it expects to find there.

That looks fussy until you see what it prevents. Parameter numbers and
addresses move between firmware versions, and the same number can mean a
different parameter in each. Pointed at the wrong firmware, a patcher without
those checks would apply cleanly, report success, and quietly corrupt whatever
happened to live at the address it wrote to. You would find out by ear, later,
with no idea why.

Refusing is the safe answer. Yours refused? You have the wrong file, not a
broken script.

## Your Mileage May Vary

These controls work. Whether they work in your preset, at your polyphony, with
your sample lengths, is something only your module can tell you.

Some of this costs CPU, and the granular controls most of all. The reverb is
the other one the manual flags. 1010music know what the processor has left at
the end of an audio block, and some of what this opens up was probably left
closed for that reason.

I have never heard crackling or a dropout while testing this, not once. That
may say more about how I had the module set up than about the ceiling, so if
you do hear one, it is not a bug to report. Ease off the control, or simplify
the preset.

## Known Limitations

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
- **No support for recorded-sample pads.** They have no FX sends in stock
  firmware at all, so there is nothing for a send control to reach. See
  [which pad types work](CC-REFERENCE.md#which-pad-types-work).
- **Values are set, not smoothed.** A CC writes the value directly, exactly as
  turning the encoder does. Fast sweeps can sound stepped;
  [use 14-bit CC](CC-REFERENCE.md#use-14-bit-cc) is the fix for that.
- **Start, Length, Loop Start and Loop End are not included**, and would not be
  much use if they were. All four already work over MIDI without any patch, but
  placing a start point or a loop region is a thing you do **by eye** — you are
  looking at the waveform and putting a marker somewhere. Turning a knob with
  nothing to look at is not the same job, and the module's own screen will not
  help, because it does not redraw for MIDI. Use the encoders for these; that is
  what they are good at.


## How It Works, Briefly

Seven small blocks of code are bolted onto the end of the firmware image, and a
few existing instructions and one table entry are pointed at them. The blocks
chain into one another, so a single redirect serves several of them. Wherever possible, the patch
hands a value straight to the firmware's own code rather than reimplementing
what that code does: a Control Change is rescaled and branched into the same
handler the encoder already uses, so it does exactly what turning the encoder
does. Any CC the patch does not claim takes the original path completely
unchanged.

The change is small and additive. Nothing in the original image is removed,
relocated or overwritten.

The two devices are the exception. Neither has an existing handler to hand a
value to, so the patch keeps their values itself and applies them to the audio
path continuously, rather than writing them once. That is why they survive a
preset change when nothing else does, and why only a power cycle resets them.
The EQ is much the larger of the two, and designs its four bands afresh on
every audio block.
