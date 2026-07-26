# Domain model

All simulated time is an unsigned integer count of microseconds. `SimTime`,
`Address`, and `ByteCount` are distinct value types; implicit conversion
between them is forbidden.

Addition of time and address-plus-size arithmetic is checked. Overflow is a
simulation error, never a modelled power-loss outcome. Addresses and sizes use
`uint64_t` at the public boundary and are checked before conversion to host
`size_t`.

A logical record is acknowledged only after its final physical commit step has
completed. Loss of an acknowledged record is therefore a protocol failure.
Loss of an operation that did not finish its commit step is acceptable. Recovery
must never consult the oracle's ground-truth history.

Sequence numbers are unsigned 64-bit integers. Version 1.0 treats wraparound as
an explicit storage exhaustion/error condition; ordering never relies on
modular comparison.
