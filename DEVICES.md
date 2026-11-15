# Bitbox Micro Firmware: Devices

Three devices act on the whole mix rather than on one pad. They sit on the
master bus, after the delay and reverb returns, so they hear the finished sound.

- **[The Character chain](CHARACTER.md)**: four Airwindows effect slots, from
  bit crushing to tape, with sixteen effects to choose from.
- **[The master EQ and filter](EQ-FILTER.md)**: tone and colour, with ten
  algorithms on one encoder.
- **[The compressor](COMPRESSOR.md)**: dynamics, set up as a glue compressor
  when it arrives, with three Airwindows compressors to switch to and a
  sidechain. Every setting is on MIDI, with auto makeup.

All three answer on **MIDI channel 1**, not on a pad's channel. They act on
everything routed to Out 1 and Out 2. Pads on Out 3–6 skip them completely,
which is your clean path when you want one.

Every change shows in the [pop-up](SETTINGS.md#the-pop-up).

## Where They Sit

![Signal chain: pads routed to Out 1 and 2 join the delay and reverb returns,
then pass through the Character chain, the master EQ and filter, the
compressor, a safety limiter and master volume before reaching the
outputs.](signal-chain.svg)

They hear everything. Every pad on Out 1/2 is in there, delay and reverb tails
included. These are mix processors, not channel strips.

Each one feeds the next. A filter sweep after heavy drive is a filter on a
distorted mix. An EQ boost makes the compressor work harder.

Master volume comes after all three. Turning the module down does not change
how hard any of them works.

Behind them sits a safety limiter. You cannot adjust it. The patch does not
touch it. It stops a careless setting from reaching your speakers, so push any
of the three as hard as you like.

## What They Have in Common

None is stored in a preset. Their settings **survive a preset change**. The
Character chain survives power-off too. So do the compressor's type, the
Airwindows types' settings and auto makeup.

The EQ and the stock compressor's five settings return to their starting values
at power-on. There is no reset control. The compressor's on/off switch is kept
with the module's own settings, as it always has been.
