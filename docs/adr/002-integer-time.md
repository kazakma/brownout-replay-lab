# ADR-002: integer virtual time

Status: accepted.

Represent monotonic time as checked unsigned integer microseconds. This avoids
floating-point drift and makes traces byte-for-byte reproducible. Overflow is
reported as a simulation error.
