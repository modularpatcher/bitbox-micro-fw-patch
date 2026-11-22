# Bitbox Micro Firmware: Settings

Seventeen of the patch's additions live on the module's own Settings page, not
on a CC. Set them once and they stay set, through preset changes and power cycles.
Preset BPM is the exception: it belongs to each preset, and saves with it.

Press TOOLS once to open Settings. Turn the top knob to pick a row and the
bottom knob to change it. Scroll past MIDI Keys and you reach these rows, in
this order:

```text
MIDI Rec:    1010music's, unchanged
Gate Rec:    OFF / RECORD 1 to 8 / REPLACE 1 to 8
ProgChange:  OFF / ON / NEW ONLY
Clk Loss:    INTERNAL / HOLD
Preset BPM:  1 to 255
SD Mode:     1010music's, unchanged
Flip UI:     OFF / ON
OSD:         OFF / POP-UP / CPU / BOTH
Keep CCs:    OFF / ON
Theme:       STOCK / AMBER / STAGE / CONTRAST / PHOSPHOR / SYNTHWAVE / CUSTOM
Delay Mode:  EFFECT / AUX / BOTH
Delay Out:   Out3/4 / Out5/6 / Out7/8 / Out3 to Out8
Reverb Mode: EFFECT / AUX / BOTH
Reverb Out:  Out3/4 / Out5/6 / Out7/8 / Out3 to Out8
Aux HP:      OFF / 60 Hz to 1.2 kHz
Aux Mono:    L+R / LEFT / RIGHT
Return:      OFF / MIX / DELAY / REVERB
Return HP:   OFF / 60 Hz to 1.2 kHz
Return Lvl:  -60.0 dB to +6.0 dB
```

## OSD

Use OSD to turn on the pop-up, the CPU bar, or both. They appear on top of
whichever screen you are on.

- OFF: neither.
- POP-UP: the pop-up only. This is the default.
- CPU: the bar only.
- BOTH: the pop-up and the bar.

Both turn upside down with Flip UI.

### The Pop-Up

Send any of the patch's CCs, except the pad stop, and a box opens in the middle
of the screen. The top line says where: the Character bank and its effect, `EQ`
and its mode, `COMPRESSOR`, `DELAY`, or a pad's channel. The bottom line says
what changed and its new value, in real units: hertz, decibels, milliseconds,
percent, or a name. It closes about a second and a half after you stop turning.

This is where to look. The module's own pages do not move for a CC.

Pad pop-ups name the MIDI channel, as in `CH 3`, not the pad, because every pad
on that channel takes the CC. Give each pad its own channel and the channel
names the pad. A pad pop-up appears only when a pad on that channel took the CC.
No pop-up means no pad there answers it. See
[when a control seems dead](CC-REFERENCE.md#when-a-control-seems-dead).

The clock has pop-ups too: `CLOCK OFF` when an external clock stops, with the
tempo the module is holding or falling back to, `Synced` when one takes over,
and `Tempo` when it settles on a new tempo.

No pop-up opens for the value already showing, a preset load, or the values
Keep CCs puts back.

### The CPU Bar

A bar along the bottom of the screen shows how hard the processor is working. It
is green, then amber from about 70%, then red from about 90%. Use it to see what
the Character chain costs before you hear it.

![The CPU bar at three loads, stacked: green only at a light load, green into
amber at about 78%, and on into red at about 95%. A white tick marks the
recent peak.](cpu-bar.png)

From the top, the bars show a light load, a load into the amber and one into
the red. The white tick is the recent peak.

## Preset BPM

Set a preset's tempo on the module and save it with the preset. Before, the only
way was to edit the preset file on a computer.

### Using It

Pick a tempo from 1 to 255 BPM, then save the preset. It loads at that tempo
from then on. Each preset can have its own.

This is the tempo the module runs at when no clock is driving it. Synced delays
and LFOs follow it. Most presets sit at 120 only because nothing on the module
could change it.

### With a Clock

A running clock always sets the tempo. Change Preset BPM while one is running
and you hear no change yet. Your tempo takes over when the clock stops, and it
saves with the preset as normal.

### Capturing a Tempo

With Clk Loss on HOLD, stop or unplug the clock and the tempo it was running at
is copied into Preset BPM. Save the preset to keep it. Changing Preset BPM by
hand ends a hold.

## Theme

Theme changes the screen's colours. Choose one of five built-in themes, or
load your own.

### The Five Themes

- AMBER: warm vintage-sampler colours.
- STAGE: dim red, for dark rooms.
- CONTRAST: the most readable. Orange and blue replace red and green, for
  colour-blind users.
- PHOSPHOR: a green-screen terminal.
- SYNTHWAVE: magenta and cyan on violet.

STOCK is 1010music's original. In every theme the pop-ups stay white on black,
and the CPU bar keeps its green, amber and red.

### Your Own Theme

Build a theme in [the theme maker](theme-maker.html), which shows how readable
each colour is. Save it as `theme.tml` at the top of the card, and set Theme to
CUSTOM. In CUSTOM, the CPU bar uses your theme's green, yellow and red.

After changing the file, set Theme to another theme and back to CUSTOM to
reload it. This works even if you swapped the card while the module was on:
your colours appear a moment later. If the bar at the top of TOOLS keeps the
old colours, set Theme away and back once more.

If the card has no `theme.tml`, choosing CUSTOM creates one from the current
theme. Missing or invalid colours fall back to Stock's.

## Keep CCs

Loading a preset puts every pad back the way it was saved. Turn on Keep CCs to
carry the changes you made from your controller into the next preset.

Say your vocal pad is on channel 6 and you have pushed its delay and reverb
sends up for the last chorus. Load the next preset and both sends drop back to
their saved values. With Keep CCs ON, the new preset's channel 6 pad comes up
with the sends where you left them.

It covers the per-pad controls this patch adds: the sends, Reverse, Sustain,
the loop, granular and LFO controls, mute and solo, and the
[Clip modes](CLIP-MODES.md). It also covers the delay's Beat Sync, Ping-pong
and filter settings. Only the ones you have moved from your controller since
power-on carry over. The rest load as saved.

Changes made on the module's screen stay with their preset. Set a Clip pad to
Jungle on the screen and only that preset has it. Send CC 112 and Jungle
follows you into the next preset.

OFF is the default: presets load exactly as saved.

## Gate Rec

Use Gate Rec to record into whichever pad you select, by gate, as MIDI Rec does
for a note. See [Gates and CV](GATES-AND-CV.md#gate-rec).

## External FX

Use these rows to send the delay and reverb out to your own effects and bring
them back on In 1/2. [External FX](EXTERNAL-FX.md#the-settings) explains each
one.

- Delay Mode and Reverb Mode choose where each effect's sends go: the module's
  own effect, an output, or both.
- Delay Out and Reverb Out choose the output for each effect's sends.
- Aux HP cuts the low end from the sends before they leave.
- Aux Mono sets how a stereo send folds into a single output.
- Return sets where In 1/2 come back: the mix, or the mix and the module's own
  delay or reverb.
- Return HP cuts the low end from the return.
- Return Lvl sets the return's level. CC 63 sweeps it too.

## Clk Loss

Clk Loss keeps the tempo when the clock goes away.

### What It Fixes

Unplug an external clock, or let it stop sending, and after about five seconds
the module falls back to the preset's own tempo. Every beat-synced delay and LFO
jumps with it.

HOLD stops the jump. The module carries on at the last tempo the clock gave it.

- INTERNAL: what the module has always done. This is the default.
- HOLD: keep the last external tempo until a clock returns.

Set the fallback tempo with Preset BPM, above.

### What HOLD Keeps Through

A held tempo survives loading another preset, by Program Change or from the
panel. It survives Stop and Start too.

A returning clock always takes over, at whatever tempo it now runs. So does a
second clock source that is still running.

Power off and the held tempo is gone, unless you saved the preset while it was
holding. HOLD itself stays set. The module starts at the preset's tempo until a
clock arrives.

Switching to INTERNAL while holding takes effect at the next Start, locate or
preset load, not at once.

## Flip UI

Flip UI is for a module mounted upside down. It turns the screen, touch, buttons
and knobs the other way up, all together.

### What Turns

- Screen: rotated half a turn.
- Touch: follows the screen, so a tap lands on what is under your finger.
- Buttons: swap ends, so each sits where it should.
- Knobs: swap jobs. Turning direction is unchanged.

The knob swap happens the moment you change the setting. The knob you just used
starts doing the other job. Expect it.

### What Stays Put

The faceplate printing stays the right way up for a normal mount. So does the
bootloader's screen while flashing. One owner turned the buttons round inside
the module so their labels read correctly upside down. That is hardware work.
Do it at your own risk.

At power-on the splash and first screen appear the normal way up for about a
second, then turn. Nothing is wrong. The module draws them before it has read
the setting from the card.

## ProgChange

Stock firmware offers OFF and ON. The patch adds a third choice.

### NEW ONLY

- OFF: ignore Program Change.
- ON: always load, even the preset already playing, as stock firmware does.
- NEW ONLY: skip a request for the preset already loaded.

Some sequencers resend the same Program Change every time a pattern starts. With
ON, each repeat reloads the preset and swallows the first notes. NEW ONLY lets
the repeats go by. A different preset still loads.

Going from A to B and back to A works. So does loading B from the panel and
then sending A. After you rename the current preset or use Save As, the first
request reloads it once. Choose ON when you want a resend to reload a preset
you have just edited.

Program Change still answers on the MIDI Pads and MIDI Keys channels, as before.
