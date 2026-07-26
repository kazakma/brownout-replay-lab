# Flash model

`ByteFlash` is a deterministic byte-array NOR model. Geometry contains capacity,
page size, sector size, and fixed program/erase durations. Sizes and durations
must be nonzero; capacity is divisible by page and sector size, and a sector is
divisible by page size.

Fresh and erased-state bytes are `0xFF`. Page program must remain within one
page and can only clear bits (`1 -> 0`). The complete request is validated
before mutation, so a driver/model error cannot leave a partial side effect.

The v1 fault rule used by the vertical slice is `prefix_programmed`: if a step
is interrupted after `elapsed / total` of its duration, exactly
`floor(byte_count * elapsed / total)` leading bytes are programmed. The rest
remain unchanged. This is an artificial, reproducible behavioural rule, not a
claim about a particular chip.

Sector erase, busy-state arbitration, wear counters, operation history, and
hexdump diagnostics belong to the next foundation wave and are not implemented
in the vertical slice.

The checked-in demo profile is 512 bytes total, 256-byte pages and sectors,
100 microseconds per program, and 1,000 microseconds per erase. It is chosen to
keep the first experiment small; it is not presented as a specific commercial
device.
