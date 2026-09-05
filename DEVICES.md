# Bitbox Micro Firmware: Devices

A device acts on the whole mix rather than on one pad. It sits on the master
bus, after everything has been summed and after the delay and reverb returns,
so it hears the finished sound and not the parts. That is what separates these
from the controls in [pad controls](PAD-CONTROLS.md), which are addressed per
pad on that pad's own channel.

There are two.

- **[The compressor](COMPRESSOR.md)**: dynamics. It arrives set up as a glue
  compressor, and all five of its parameters are on MIDI.
- **[The master EQ and filter](EQ-FILTER.md)**: tone and colour. Ten
  algorithms on one encoder, and the larger of the two.

Both answer on **MIDI channel 1**, unlike the per-pad controls, and both act on
everything routed to Out 1 and Out 2. Pads on Out 3–6 skip them completely,
which is your clean path when you want one.

The module gives you no screen for any of it. Nothing on the display moves when
a CC arrives, for either device, so your controller is the display and your
ears are the test.

## Where They Sit

![Signal chain: pads routed to Out 1 and 2 join the delay and reverb returns,
then pass through the compressor, the master EQ and filter, a safety limiter
and master volume before reaching the outputs.](signal-chain.svg)

1010music do not publish the order of the master bus, so this is reasonably
deduced rather than documented. It is consistent with what both devices do to
a mix.

Three things follow from that, and they apply to both devices.

They see everything. Each acts on the sum of every pad routed to Out 1/2,
including the delay and reverb returns. These are mix processors, not channel
strips, and they hear the whole thing at once.

Master volume is after both of them. So turning the module down does not change
how hard either one works, which is what you want.

Behind them sits a safety limiter this patch does not touch. It is the last
thing before the outputs, it is not adjustable, and it is what stops a careless
setting from reaching your speakers. You can push either device hard without
worrying that you will damage anything.

## What They Have in Common

Neither is stored in a preset. Both keep their settings while the module is
switched on, both **survive a preset change** when nothing else in this patch
does, and a power cycle returns both to their starting values. There is no way
to save either into a preset and no reset control for either.

Every control on both devices has been heard and tested on hardware.
