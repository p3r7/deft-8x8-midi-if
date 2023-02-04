
fork of [Deftaudio/Midi-boards/MIDITeensy4.1](https://github.com/Deftaudio/Midi-boards/tree/master/MIDITeensy4.1).


| port #       | device               | comment           |
|--------------|----------------------|-------------------|
| USB (master) | norns                | omni              |
| 1 / X        | master keyboard      | in only, omni     |
| 2 / 10       | MPC                  | omni except sysex |
| 3 / 11       | Waldorf uQ           |                   |
| 4 / 12       | Waldorf Pulse        |                   |
| 5 / 13       | microKorg            |                   |
| 6 / X        | MIDI -> CV (keystep) | out only          |
| X / X        |                      |                   |
| 8            | computer (via MOTU)  | omni              |

NB: midi INs relay info to all the over midi OUTs except themselves. one exception of this is port #1 (IN #1) that does relay info to port #9 (OUT #1)

also, port #14 (IN #6) is dead. issue seems to be at the level of the teensy board (damage while soldering pins?). that's why MIDI->CV got set here (asymmetric).

if i ever fix this port #14, could be good to move MIDI->CV on port #9 so that a whole column would be free.


# next steps

 - [ ] allow switching w/ sysex between several pre-configured profiles. notably omni (current) & segmented
 - [ ] for segmented mode, make each device use its own channel set at BUS level (eg IN/OUT #2 is channel 2...)
 - [ ] norns mod to interract w/ the sysex config
 - [ ] remap MPC note values instead of relying on custom config at MPC level (PGM file)
