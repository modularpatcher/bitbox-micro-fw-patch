# Bitbox Micro, Unlocked

The Bitbox Micro hides a long list of useful controls behind a screen, one
knob at a time, and a compressor you can only switch on and off, with no
adjustment at all. This patch puts every one of them on MIDI: delay and
reverb sends, envelope, LFO, loop and granular controls per pad, and all
five compressor parameters. It adds one thing the module never had at all, a
multi-algorithm master EQ and filter across the whole mix. All live, while
you play.

It is an unofficial modification. Nobody at 1010music wrote or endorsed it.
The splash reads `community fw`, so a patched unit is never mistaken for one
of theirs, and nobody should ask 1010music for support with it. Assume running
it affects your warranty, though it [cannot brick the module](INSTALLING.md#can-this-brick-my-module).

## A Patcher, Not Firmware

What you download is a script, not a build of the firmware. It edits a copy you
already own, on your own machine, and that copy never leaves it. Nobody is
distributing 1010music's code, because nobody here has it to distribute.

One rule holds the whole thing up: **never pass a patched image on**. You run
this against firmware you downloaded yourself, under 1010music's terms.

`patch_micro.py` reads your own copy of the stock 2.3.4 image and writes out a
patched one. It refuses anything that is not exactly stock 2.3.4. The image is
yours only because you supplied the firmware it came from. It stops with you.
Send someone this page instead, and let them build their own.

## Requirements

Five things, nothing exotic.

- A Bitbox Micro running firmware 2.3.4.
- [`patch_micro.py`](patch_micro.py), the patcher itself.
- Your own copy of the stock 2.3.4 image, from 1010music's downloads page.
- Python 3, any version from the last few years.
- A microSD card the module can read.

Full detail on each of these, including where to get them, is in
[installing](INSTALLING.md).

## Quick Start

Get [`patch_micro.py`](patch_micro.py) and `MICRO234.zip` from 1010music, run
the one against the other, and copy the result to your card. Check the splash afterwards. It should read
`community fw`. Full steps, and everything that can go wrong, are in
[installing](INSTALLING.md).

It cannot brick your module. The bootloader is a separate program this patch
never touches, and it is what does the flashing. See
[installing](INSTALLING.md#can-this-brick-my-module) for the full answer.

## Read Next

- [Installing](INSTALLING.md): stock image to patched module, step by step.
- [CC reference](CC-REFERENCE.md): every number, its channel, which pads answer
  it, and what to check when one seems dead.
- [Pad controls](PAD-CONTROLS.md): what each pad parameter does. The module had
  these already; the patch reaches them.
- [Devices](DEVICES.md): the two that act on the whole mix, and where they sit.
- [The compressor](COMPRESSOR.md): bus glue, adjustable from there.
- [The EQ and filter](EQ-FILTER.md): ten algorithms on one encoder.
- [Limitations](LIMITATIONS.md): what it will not do, and why it refuses.
- [Changelog](CHANGELOG.md): what changed. Newest first.

## Licence

This project is MIT-licensed, covering the patcher and this guide only. The
firmware is 1010music's, and no permission over it is granted. Bitbox Micro is
a product of 1010music LLC, named only to say what hardware this runs on. Meet
this behind a paywall and somebody sold you something they did not write.

Flash stock before sending a unit for service. See [LICENSE](LICENSE) for the
rest.

## AI Assistance

Yes, for the patch and this guide. It helped read the assembly and draft
documentation that would otherwise have taken weeks. Hours not spent writing go
into designing controls and testing them on hardware.

It could not decide whether any of it works. Every control was confirmed by
ear, over many hours of building controller mappings and moving parameters.
Judge it by whether it works and what it opens up for you, not by how it was
made.

## If You're from 1010music

Thank you for the Bitbox Micro. It is a wonderful little module, and this
exists because it is worth spending time inside. No firmware of yours is
included here, in any form. Everybody downloads the firmware from your site,
under your terms, and the script refuses to run on anything else.
What ships is one Python file, a guide, and an MIT licence covering only
those. The patched image reads `community fw` on the splash, so it is never
mistaken for your work. Anyone with a problem is told to flash your firmware
back before contacting you.

## Getting Help

There is no support for this: it is one person's patch, given away as is. Ask
in [the Mod Wiggler thread](https://www.modwiggler.com/forum/viewtopic.php?t=302078),
r/modular or r/eurorack. Other users run the same controls on the same numbers,
so their answers transfer to you. Please do not take questions to 1010music's
Discord. That is not their work, and the people there are helping with the
actual product.
