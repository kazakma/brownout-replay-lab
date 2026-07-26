# ADR-001: behavioural step model

Status: accepted.

Use a deterministic sequential step runner. Cycle-accurate MCU and electrical
simulation are out of scope because the research target is software crash
consistency. Every failure is attached to a stable named step or integer
virtual time.
