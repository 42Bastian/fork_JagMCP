# 0004: References are recorded traces, generated programs among them

**Status:** accepted

## Context

The core has to match the FPGA simulation cycle for cycle. Running the simulation in the test
suite would need Verilator and the Jaguar_MiSTer checkout on every machine and minutes of time;
hand-written tests cover only the cases someone thought of.

## Decision

- A run of the simulation is recorded once in `tests/reference/` (binary, arguments, trace, final
  state) by `scripts/make-references.py`, and the suite replays the core against the record.
- Besides the lyxass programs, the script generates random instruction sequences of one class at a
  time (`alu`: register to register instructions), straight to machine code, from a seed. A class
  is added for each area the core takes on: jumps, local memory, external memory, interrupts.
- A long program is recorded up to a cycle limit, so that references stay small.

## Consequences

- The suite runs anywhere in seconds, without Verilator.
- A change to the simulation means re-recording every reference, never editing one.
- Generated programs find interactions no one wrote a test for: the stall of a two operand
  instruction in a cycle with a register write, which of two writes of one register survives, and
  what NORMI's carry is made of were all found by them.
