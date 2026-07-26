# Persistence protocol

The vertical slice stores one record in a fixed 246-byte slot:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | ASCII magic `BRL1` |
| 4 | 1 | format version (`1`) |
| 5 | 4 | payload length, little-endian |
| 9 | 8 | sequence number, little-endian |
| 17 | 0–224 | payload |
| variable | 4 | CRC-32/ISO-HDLC |
| padding | variable | erased `0xFF` |
| 245 | 1 | commit marker |

CRC uses reflected polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, and
final XOR `0xFFFFFFFF`. It covers the header and actual payload, but not CRC,
padding, or commit marker.

The body is programmed first. The fixed final marker is programmed from
`0xFF` to `0x00` in a separate last physical step. Recovery checks that fixed
marker before trusting the length field. Empty or uncommitted slots yield
`NoCommittedRecord`; malformed committed slots yield `DetectedCorruption`; only
a committed, structurally valid, CRC-valid slot yields `Recovered`.

This fixed-slot protocol is intentionally narrow. It does not yet implement the
`naive`, `double_buffer`, or `append_log` strategy guarantees.
