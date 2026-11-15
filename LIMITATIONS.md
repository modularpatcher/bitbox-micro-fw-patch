# Bitbox Micro Firmware: Limitations

This page covers what the patch does not do, and what it does to your firmware.
Read it before you flash.

## What It Will Not Do

Three things catch people out. None of them is caused by the patch.

### The Module's Pages Don't Follow CCs

A CC changes the sound, but the module's own pages keep showing the old value.
The module has always worked this way. Watch the
[pop-up](SETTINGS.md#the-pop-up) instead: it shows every patch CC as it lands.
With the OSD on OFF or CPU there's no pop-up, so trust your ears.

### A Preset Load Resets Your CCs

Loading a preset brings its pads back as saved, so anything you changed on the
pads by CC is gone. Turn on [Keep CCs](SETTINGS.md#keep-ccs) to carry your changes over, or
resend them from your controller.

### Can CV Control Them?

Partly. Use CV Action to sequence a Clip pad's Live Flip mode or choose its Hold
FX from a CV input, as [Gates and CV](GATES-AND-CV.md) describes. Every other
control needs a CV-to-MIDI converter, whose output the module reads as ordinary
Control Change.

## Strict Build by Design

The patcher runs only on byte-for-byte stock 2.3.4, checked by sha256. It also
checks that every place it changes holds exactly what it expects. Without those
checks, wrong firmware could be quietly corrupted. If yours refused, you have
the wrong file, not a broken script.

## Your Mileage May Vary

A busy preset can run out of processor. The granular controls cost CPU, and so
does the Character chain with every slot running. The reverb is the other one
the manual flags.

Watch the [CPU bar](SETTINGS.md#the-cpu-bar). Testing never produced a crackle
or a dropout. If you hear one, it is not a bug to report. Ease off the control,
or simplify the preset.

## Known Limitations

- **It runs on 2.3.4 only.** It refuses every other version, including future
  ones. A new 1010music release means the patch has to be redone, not just
  re-run.
- **This adds no modulation targets.** The module's own LFOs and envelopes still
  cannot reach the granular controls, the sends or the rest. The manual's
  "Mod Target? No" is as true as before. Drive them from an external sequencer
  or controller.
- **Recorded-sample pads get no sends.** They have none in stock firmware. See
  [which pad types work](CC-REFERENCE.md#which-pad-types-work).
- **The Character chain is not saved in a preset.** It survives preset changes
  and power-off instead.
- **Pad values are set, not smoothed.** Fast sweeps can sound stepped, and
  14-bit is no cure, because
  [the patch's CCs stay 7-bit](CC-REFERENCE.md#use-14-bit-cc).
- **Start, Length, Loop Start and Loop End are not included.** They already work
  over MIDI without the patch, but you place them by eye on the waveform, and
  the screen does not redraw for MIDI. Use the encoders.

## How It Works, Briefly

The patch adds its code to the end of the firmware image and points a few
existing instructions at it. Nothing in the original is removed or moved. Most
of the data it rewrites is in the lists of settings on the Settings page and on
each pad's Main Parameters page.

A patch CC does exactly what turning the encoder does. Any CC the patch does
not use behaves as before. The three devices are different: the patch holds
their values itself, so a preset change leaves them alone.
