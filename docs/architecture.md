# Architecture

The version 1 vertical slice has three library layers:

1. `brlab_core` owns strong units, checked virtual-time arithmetic, named steps,
   cut decisions, and deterministic traces.
2. `brlab_flash` owns the byte image and software-visible NOR constraints. It
   does not know record formats.
3. `brlab_persistence` owns the explicit on-flash record codec and fixed-slot
   recovery. It does not know ground truth.

The `brlab` executable composes these layers. A body program and the final
commit-byte program are separate named physical steps. A cut stops the runner;
a newly constructed boot path calls recovery before attempting another write.

The current vertical slice deliberately does not contain the future power
schedule, lifecycle state machine, storage-strategy interface, campaign engine,
or oracle. Their CMake target names are reserved, but no stub behaviour is
presented as implemented.
