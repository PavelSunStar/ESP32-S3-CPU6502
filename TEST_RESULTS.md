# ESP32-S3 CPU6502 — test results (2026-10-29)

Verified by the user on real ESP32-S3 hardware via Arduino Serial Monitor.

| Test | Result | Notes |
| --- | --- | --- |
| instr_test-v5 | PASS 16/16 | Re-run after adding IRQ/NMI and dummy bus access fixes |
| nestest | PASS 5000/5000 | Re-run after adding IRQ/NMI and dummy bus access fixes; reference CPU states |
| CPU Timing (CPU-only) | PASS 242/242 | Re-run after dummy bus fixes; not original timing ROM with PPU |
| Dummy Bus Access (CPU-only) | PASS 37/37 | 36 RMW write sequences and one page-cross dummy read; isolated test harness may use its own CPU copy |
| IRQ/NMI (CPU-only) | PASS 16/16 | Main NES_esp32 library, after IRQ/NMI implementation |
| Branch Timing (CPU-only) | PASS 64/64 | Verified before last changes; not rerun after IRQ/NMI |

## Test logs

```text
instr_test-v5: TOTAL: PASS=16 FAIL=0 STOP=0 TIMEOUT=0 OTHER=0 / 16
nestest: PASS: all 5000 reference states match nestest.log!
CPU timing: FINAL: PASS=242 FAIL=0 TOTAL=242
Dummy bus: FINAL: PASS=37 FAIL=0 TOTAL=37
IRQ/NMI: FINAL: PASS=16 FAIL=0 TOTAL=16
Branch timing: FINAL: PASS=64 FAIL=0 TOTAL=64
```

## Included test examples

- `examples/InstrTestV5_All16/`: test ROM suite and Arduino sketch.
- `examples/Nestest_5000_Check/`: reference-state comparison.
- `examples/IRQ_NMI_MainLibrary/`: CPU-only IRQ/NMI verification using the installed library.
- `examples/README_RU.txt`: Russian instructions.

## Accuracy scope

These results validate the stated scenarios, not the complete original NESdev PPU/APU-dependent ROM suites. Original `cpu_interrupts.nes` (Mapper 1) was **not** executed. IRQ/NMI are handled at instruction boundaries, without full cycle-by-cycle behavior or the precise CLI/SEI interrupt latency. The dummy bus test covers selected operations only. More hardware and integration testing is required.
