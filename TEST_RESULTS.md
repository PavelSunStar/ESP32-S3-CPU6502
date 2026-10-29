# ESP32-S3 CPU6502 — instr_test-v5

Verified on ESP32-S3, 2026-10-29 (user-provided serial output).

```text
TOTAL: PASS=16 FAIL=0 STOP=0 TIMEOUT=0 OTHER=0 / 16
```

All 16 independent mapper-0 instr_test-v5 ROM tests passed, including BRK and special cases.

## Source code and reproducible test examples

- `src/` — CPU6502, opcode table, Bus, and library wrapper from the tested archive.
- `examples/InstrTestV5_All16/` — Arduino test sketch and embedded PRG data for all 16 test ROMs.
- `examples/Nestest_5000_Check/` — nestest sketch, reference log data, and ROM header.
- `examples/README_RU.txt` — instructions (Russian).

The auxiliary nested `Nestest_5000_Check.zip` present in the original archive is not duplicated here because the unpacked source files are included directly.

This demonstrates the test suite passing, but does not establish full cycle-by-cycle bus, IRQ or NMI timing accuracy.
