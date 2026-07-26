# ADR-003: deterministic randomness

Status: accepted.

Every random campaign receives an explicit 64-bit seed. Version 1.0 uses a
project-owned, specified PRNG transformation rather than implementation-defined
standard distributions. System time and unordered iteration never affect model
output.
