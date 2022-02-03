# uxnseq

An experimental music sequencer using Uxn, designed
to run inside of sndkit.

The core motivation of this project to test how easily the
Uxn VM could be embedded in a project and used to build up
control systems for controlling pitch.

## Compilation

This needs [sndkit](https://git.sr.ht/~pbatch/sndkit) to be
installed.

After that, this project can be built by running `make`.
The build will produce two binaries: `uxnseq`, a sndkit LIL
interpretter with uxnseq functions added to it, and `uxnasm`,
a modified version of the uxn assembler to support symbol
table export (which is used in some of the examples).

## Examples

Examples are located in the examples folder. Going into the
folder and running `./render.sh`. Each example was constructed
to guage how well Uxn could be used in a generative music
system.

Each example has a sndkit patch written in LIL (.lil) and a
uxn program written in TAL (.tal).

`sing` tests sequences running in parallel. (I also had
got a bit carried away with the sound patch and turned the
instruments into classically trained singing chipmunks.)

`seq` tries to build up some more efficient
abstractions of sequences in uxn.

`rng` tests out some randomization using a PRNG implemented
in Uxn. This sequence is composed by randomly selecting
chunks of pre-composed sequences.

`coord` tests out coordination between sequences. One
sequence is performed with some randomization. The other
sequence has an awareness of the other sequences, and makes
choices based on the state of the other sequence.

## Technical Notes

Uxnseq does not use Vavara, which is what most Uxn programs
seem to target. Instead, the core Uxn VM is created inside
of sndkit, and then used to manipulate the state of nodes
in the audio graph that sndkit generates.

Uxn does not synthesize the sound itself. Audio synthesis
happens using Sndkit. Uxn acts as a controller for a signal
generator in sndkit (kind of analogous to "virtual CV").
This signal can be used to modulate
synthesis parameters in the patch created by sndkit.

Uxn communicates to sndkit using the device abstraction. I
created a custom device on port 2. In a way, you can think
of sndkit as like a modular synthesizer that plugs into Uxn.

Uxn devices don't have a generic pointer for external
data, I'm doing some sneaky/clever things to avoid using
global data. Devices *do* however, have access to the
instance of the Uxn VM. I've placed the Uxn VM as the
first item inside the `uxnseq` struct. I take advantage
of casting to turn the Uxn instance into an uxnseq
instance. C programmers will recognize this hack as the
classic way to get OOP-like mechanisms in C.

Timing is done using an audio-rate clock signal generated
by sndkit, which is very similar to `metro~` in PD or the
metro opcode in Csound. Every time the the clock ticks,
it makes a call to `uxn_eval` (unless the duration counter
is non-zero, but don't worry about that).

Concurrency in Uxn is possible to do because sndkit is
single-threaded, and very explicit with execution order.
There is only one instance of Uxn instantiated, but each
sequencer node their own sequencer state (including
a programmer pointer). When it is
their time to execute, they set their state to be the
current state. Uxn is blissfully unaware of these state
changes, as it only knows to send bytes to some device.

It is convenient to reference memory addresses in ROMs
by their human-readable labels, rather than their
absolute address. I have modified the
uxn assembler program `uxnasm` to generate a
simple [symbol table](https://en.wikipedia.org/wiki/Symbol_table)
at the beginning of the file.
I use a custom loader that knows to check for, and skip
this symbol table. Running uxnasm with the
`-g` flag like `uxnasm -g foo.tal foo.rom` will
generate a ROM file with a symbol table.

The struct of the symbol table is a linear list.
To indicate that it is indeed a symbol table, it
starts with the letters 'S', 'Y', and 'M', followed
by the table size in bytes (short). Following that
are the entries. An entry consists of the string size
(1-byte), the string itself (N bytes), and then the
address (short). Shorts are encoded as little-endian values.
Since the structure is a linear list, table-lookup is an
O(n) operation.