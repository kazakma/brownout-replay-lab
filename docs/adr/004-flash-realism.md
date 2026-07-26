# ADR-004: flash-model boundary

Status: accepted.

Model only software-visible NOR constraints: erased bytes are `0xFF`, program
permits `1 -> 0`, erase works by sector, operations take fixed virtual time, and
power loss applies a deterministic completed prefix. This is an artificial
fault model, not a claim about a particular chip's analogue behaviour.
