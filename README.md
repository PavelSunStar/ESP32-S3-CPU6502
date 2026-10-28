# ESP32-S3-CPU6502

Educational MOS 6502 / NES CPU emulator in C++ for **ESP32-S3** and Arduino.

## Validation: nestest (5000 states)

The current development version passed all 5000 reference states in the `nestest.log` trace:

```text
4900 OK PC:FA27 OP:70 A:AA X:55 Y:74 P:A4 SP:F9 CYC:14282
5000 OK PC:C6B2 OP:48 A:FF X:97 Y:4E P:A5 SP:F9 CYC:14567
PASS: all 5000 reference states match nestest.log!
```

Compared: **PC, opcode, A, X, Y, processor flags P, stack pointer SP and CPU cycle count**. This is validation against the tested trace, not a claim that every NES program is fully supported.

## Source files

- `src/cpu6502.h` / `src/cpu6502.cpp` — CPU state, addressing modes and instructions
- `src/cpu6502_opcodes.cpp` — 256-entry opcode dispatch table
- `src/bus.h` / `src/bus.cpp` — simple RAM and test memory bus
- `src/NES_esp32.h` / `src/NES_esp32.cpp` — wrapper for CPU and Bus

## Run the 5000-state test

All test files are provided in [examples/Nestest_5000_Check](examples/Nestest_5000_Check):

- `Nestest_5000_Check.ino` — Arduino serial test sketch
- `nestest_rom.h` — original `nestest.nes` test ROM represented as a C array
- `nestest_expected.h` — first 5000 states from `nestest.log`, converted into a C array

Install this project as an Arduino library (or put `src` files in your library folder), select the ESP32-S3 board, open `Nestest_5000_Check.ino`, and upload. Open the Serial Monitor at **115200 baud**.

The sketch prints checkpoints every 100 instructions and should end with `PASS: all 5000 reference states match nestest.log!` on the version tested during development.

## License and credits

Our emulator implementation and the Arduino verification sketch are released under the **MIT License** (see [LICENSE](LICENSE)).

The `nestest` test ROM and reference log are by **Kevin Horton (kevtris)** and are distributed here as third-party public-domain emulator test fixtures. They are not claimed as original project code.

Original fixtures: [christopherpow/nes-test-roms — other](https://github.com/christopherpow/nes-test-roms/tree/master/other)

## Current limitations

This is a CPU-focused educational implementation. Some unsupported opcodes intentionally halt execution. PPU, APU, interrupts, cartridge mappers, and full game compatibility are future work.
