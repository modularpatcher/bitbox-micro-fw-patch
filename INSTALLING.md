# Bitbox Micro Firmware: Installing

The one-time path from a stock 2.3.4 download to a module running the patch:
what you need, how to patch it, how to install it, and what to do if it goes
wrong. See [the overview](README.md) for what the patch does, and
[the CC reference](CC-REFERENCE.md) for every number once you are set up.

## Prerequisites

Five things, none of them exotic.

- A **Bitbox Micro** running firmware **2.3.4**.
- [`patch_micro.py`](patch_micro.py), the patcher itself.
- Your own copy of the stock 2.3.4 image, from
  <https://1010music.com/downloads> → bitbox micro → **`MICRO234.zip`**. The
  script will not run without it, and none of it ships here.
- **Python 3.** Any version from the last several years works, with no
  compiler, no build step, nothing to configure. See
  [Getting Python](#getting-python).
- A microSD card the module can read. Any working card is fine for flashing:
  the image is small and gets read once. If you are buying one anyway, **V30
  is the rating worth having**. It promises 30 MB/s sustained where Class 10
  promises 10, which matters for streaming samples while you play rather than
  for this.

Two things worth doing first:

1. **Keep the stock `MICRO.BIN`**, and keep 1010music's `MICRO234.zip` too.
   It is how you go back, and the script wants it again every re-patch.
2. **Back up your SD card.** The patch never touches your presets or
   samples. Back it up anyway.

### Presets That Use It

The controls need no setup and work on every preset the moment you flash. But
an old preset only gives back what it was built to do. See
[Building a Preset for This](PAD-CONTROLS.md#building-a-preset-for-this) for what
that takes.

## Preparing

The firmware is 1010music's, and you download it from them yourself, under
whatever terms they put on it. Nothing here redistributes it. Unzip it. You
get `MICRO.BIN`. The version lives in the zip's name and nowhere in the
file's, worth knowing before your folder fills with unlabelled copies.

If you already have a patched build installed, patch your stock `MICRO.BIN`
again with the new script, not the patched image you flashed last time. The
script checks. It refuses a file that has already been through it. That is
the safety net working, not a fault. If you no longer have the stock file,
download `MICRO234.zip` from 1010music again.

The patcher refuses more than it accepts. A wrong firmware version, a file
already patched, a patch site not holding the exact instruction it expects,
or an output path that would overwrite your stock copy: each one stops it
before a single byte is written.

### Getting Python

> **Windows**: you will need to install it. Download it from
> <https://www.python.org/downloads/> and run the installer. On the very first
> screen, tick "Add python.exe to PATH" before you click Install. Older
> installers word it "Add Python to PATH," the same box. That one tickbox is
> the difference between this working and not, and it is easy to click
> straight past. Missing it breaks nothing; see *If Windows Cannot Find
> Python* below.

> **macOS**: usually there already. The first time you type `python3`, macOS
> may offer to install its command line tools. Say yes and let it finish. It
> is a big download. Give it a few minutes. It has not hung.

> **Linux**: already there. If a minimal install left it out, your package
> manager has it under the name `python3`.

You run the script from a terminal, in the folder that contains
`patch_micro.py` and `MICRO.BIN`.

Getting the terminal into that folder:

- Windows: open the folder in File Explorer, click into the address bar at
  the top, type `cmd` over what is there, and press Enter. PowerShell works
  the same way.
- macOS: open Terminal, type `cd ` (with the space), then drag the folder
  from Finder onto the Terminal window and press Enter. It fills in the path
  for you.
- Linux: most file managers have "Open Terminal Here" on the right-click
  menu. The macOS drag trick works in most terminals too.

## Building

Enter the commands one line at a time, pressing Enter after each.

Do not double-click `patch_micro.py`. It has to be run from a terminal;
double-clicking it will either open a text editor or flash a black window
shut before you can read it.

On Windows:

```sh
mkdir patched
python patch_micro.py MICRO.BIN patched/MICRO.BIN
```

The forward slash is right on Windows too. Python reads it, not the shell.
No backslash needed.

On macOS and Linux, the same thing, but the command is `python3`:

```sh
mkdir patched
python3 patch_micro.py MICRO.BIN patched/MICRO.BIN
```

On all three, if you have patched before, `mkdir patched` will say the folder
already exists. That is fine: it is the folder you wanted. Carry on.

If it says it cannot open `patch_micro.py`, a long line ending in
`[Errno 2] No such file or directory`, the terminal is not in the folder
those two files are in. Nothing is missing and nothing is broken. The long
path in that message is where the terminal looked. Go back to *Getting the
terminal into that folder* above and try again.

The patched image must also be called `MICRO.BIN`, exact name, upper case,
for the module to find it. That is why it goes in its own folder: writing the
output over your input would destroy the only stock copy you have, and that
copy is your way back if anything goes wrong. It refuses if you try.

That file is yours alone: do not pass it on. See
[A Patcher, Not Firmware](README.md#a-patcher-not-firmware) for why.

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
     ok   EQ mode select, ch 1         CC 50
     ok   EQ control A, ch 1           CC 51
     ok   EQ control B, ch 1           CC 52
     ok   EQ control C, ch 1           CC 53
     ok   EQ button A, ch 1            CC 54
     ok   EQ button B, ch 1            CC 55
     ok   EQ button C, ch 1            CC 56
     ok   EQ master bypass, ch 1       CC 57
     ok   EQ filter slope, ch 1        CC 59
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

  Done. MICRO.BIN is ready -- 676,164 bytes.

     Fingerprint  46affcde81318a4c9c4cdaa0467a6958156079d4a683bdf56c48aa573c5d70a2
```

It then tells you how to get the file onto the module. That is written out
below.

Check the fingerprint matches the one above. If it does, you have built
exactly the same firmware as everybody else running this patch, down to the
byte.

Add `-v` if you want to watch it work: addresses, opcodes and byte counts for
every change it makes.

## Flashing

1. Copy `patched/MICRO.BIN` to the **root** of the microSD card, not inside a
   folder. Leave your stock copy where it is: it is how you go back.
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
2.3.7-mod
```

If it still says `by 1010music` and `2.3.4`, the module booted the old
firmware and nothing was flashed. Check the file is named `MICRO.BIN` and
sits in the card root. Then try again.

Check this every flash. It is the only reliable confirmation of which image
is actually running, and it takes one second, against the twenty minutes you
would otherwise waste proving a CC does not work on firmware that was never
installed.

Then test the CCs by listening, not by watching the screen. For all but two
of them the display never moves, on this firmware or the stock one, so your
ears are the only way to tell. This catches everybody exactly once. The two
that do show, Reverse and Loop Mode, are the fastest way to prove your
controller is getting through. See [the CC reference](CC-REFERENCE.md) for the full
control list once you start checking them off.

## If Something Goes Wrong

### Windows Cannot Find Python

Two things can happen here. Neither means you broke anything.

#### The Microsoft Store Opens Instead of Running Anything

Python is not installed yet, and Windows is offering to sort that out. Take
the Store's copy, or install from python.org as above; either works.

#### It Says 'python' Is Not Recognized

Python is installed, but Windows does not know where to find it: the
"Add python.exe to PATH" tickbox was missed during the install. Before you
reinstall anything, try `py` instead of `python`:

```sh
py patch_micro.py MICRO.BIN patched/MICRO.BIN
```

`py` is a small launcher that comes with Python and gets added to PATH
whether or not you ticked that box, so it usually works when `python` does
not.

If `py` does not work either, install Python again from
<https://www.python.org/downloads/> and tick "Add python.exe to PATH" this
time. Nothing is wrong with the script or your download.

### If It Stops with an Error

Good. That is the safety check earning its keep. It tells you what it found
and what it expected, and it will not have written a thing.

- *"That is not the stock 2.3.4 firmware"*: the input is a different
  version, already patched, or a truncated download. If you are upgrading
  from an earlier version of this patch, this is almost certainly why: you
  have pointed the script at the patched image you flashed last time, not at
  stock. Patch your original `MICRO.BIN` again, the one straight out of
  `MICRO234.zip`. There is no upgrade-in-place path, and there never will
  be. Every safety check here is written against stock bytes, which is why
  it caught this. Download `MICRO234.zip` from 1010music again if you no
  longer have it.
- *"The code at … is not what it should be"*: the file passes the checksum,
  but the machine code underneath is wrong, which should be impossible.
  Something strange is happening. Forcing it will not make it less strange.

The script checks every site before it writes anything, so there is no such
thing as a half-patched file. It is all or nothing. It prefers nothing.

### Going Back to Stock

The bootloader stays untouched throughout. It lives in a separate region of
flash that this patch never writes to. It is what performs the update, so a
bad firmware image cannot stop you flashing a different one.

To go back to stock, rename your original 1010music download to `MICRO.BIN`,
put it in the card root, and follow the same install steps. The module
overwrites the patched image with it and forgets any of this happened. This
is also the answer if you simply preferred it before.

## Can This Brick My Module?

No. Not because the script is careful, though it is, but because of how the
module itself is put together.

The part that actually flashes firmware is the bootloader, a separate
program in its own region of flash. It is not in the file you are patching.
This patch never writes near it.

You can prove that from the firmware image itself. The words `Erasing`,
`Programming`, `Verifying` and `Bootloader`, the ones on screen during an
update, appear nowhere in the firmware file. They belong to the bootloader,
which has its own display code and its own SD card handling, and never calls
into the firmware. The firmware cannot even write to flash: the unlock
sequence needed to do that is not in the image.

The update is also triggered by holding a physical button at power-on,
before any firmware runs. Recovery does not depend on the firmware working,
which is exactly what you want from a recovery path. A completely broken
image still cannot stop you flashing a good one.

Worst case: the module boots oddly, or not at all. You hold the white
right-arrow, flash the stock image back, and you are where you started about
two minutes later.

Two things make this narrower than it sounds. The change to the original image
is tiny and everything else is appended, so nothing is deleted, moved or
overwritten. The one genuine risk, losing power mid-write, takes about 15
seconds and is no worse than an official 1010music update. Even then the
bootloader survives, so you just try again. Do not flash from a laptop about
to sleep, or a supply you do not trust.

See [If Something Goes Wrong](#if-something-goes-wrong) above for the exact
steps back to stock.

## When New Firmware Lands

The patcher will refuse it, and it is being stubborn on your behalf. The
addresses it writes to are specific to 2.3.4; on a different build they
point somewhere else entirely, and somewhere else is not a good place to
write. Do not try to work around the version check; someone has to redo the
reverse engineering first.
