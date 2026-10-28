# ESP32-S3-CPU6502

Educational MOS 6502 / NES CPU emulation project for ESP32-S3, written in C++.

## Validation

The development version passed a comparison of **5,000 consecutive CPU reference states** against `nestest.log`:

```text
4900 OK PC:FA27 OP:70 A:AA X:55 Y:74 P:A4 SP:F9 CYC:14282
5000 OK PC:C6B2 OP:48 A:FF X:97 Y:4E P:A5 SP:F9 CYC:14567
PASS: all 5000 reference states match nestest.log!
```

The comparison included PC, opcode, A, X, Y, processor status P, stack pointer SP, and CPU cycle count. This demonstrates correctness for the tested instruction sequence; it does not yet establish full NES compatibility.

## Source files

- `cpu6502.h` / `cpu6502.cpp` — CPU registers, addressing modes and instructions
- `cpu6502_opcodes.cpp` — opcode dispatch table
- `bus.h` / `bus.cpp` — simple memory bus used during CPU development
- `NES_esp32.h` / `NES_esp32.cpp` — integration wrapper

## Status

CPU-focused learning project. Full PPU, cartridge mapper support, video output, and full game compatibility are separate future milestones. No commercial NES ROMs are included.

## Build

The `src` directory is intended to be compiled as part of an ESP32-S3 Arduino/C++ project. A complete `nestest` verification sketch and test fixtures are not included in this initial source snapshot.
