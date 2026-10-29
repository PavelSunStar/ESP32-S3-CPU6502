#include <Arduino.h>
#include <NES_esp32.h> 
#include "instr_test_roms.h"
     
// ESP32-S3: run all 16 blargg instr_test-v5 rom_singles in order.
// Serial Monitor: 115200.  No PPU/APU. Each CPU test has a fresh RAM image. 
// IMPORTANT: CPU6502::Instr_CLI() must clear I instead of halting.   
// Tests containing unofficial opcodes may halt; this is reported as STOP.    
  
static NES_esp32 nes;

enum Result : uint8_t { NOT_RUN, PASS, TEST_FAIL, STOP, TIMEOUT, LOAD_ERROR, RESET_REQUEST };   
static Result outcomes[kRomCount] = {};
static uint8_t statusCodes[kRomCount] = {};
static size_t current = 0;
static uint32_t testStarted = 0;
static uint64_t steps = 0;
static uint32_t lastReport = 0;
static uint8_t resetCount = 0;
static bool done = false;
static uint16_t lastInstructionPC = 0;
static uint8_t lastInstructionOp = 0;

static constexpr uint32_t MAX_SECONDS_PER_TEST = 120;
static constexpr uint64_t MAX_CLOCKS_PER_TEST = 100000000ULL;
static constexpr uint8_t MAX_RESETS_PER_TEST = 3;

static const char* resultName(Result r) {
  switch (r) {
    case PASS: return "PASS";
    case TEST_FAIL: return "FAIL";
    case STOP: return "STOP";
    case TIMEOUT: return "TIMEOUT";
    case LOAD_ERROR: return "LOAD ERROR";
    case RESET_REQUEST: return "RESET NEEDED";
    default: return "NOT RUN";
  }
}

static bool hasSignature() {
  return nes.bus().cpuRead(0x6001) == 0xDE &&
         nes.bus().cpuRead(0x6002) == 0xB0 &&
         nes.bus().cpuRead(0x6003) == 0x61;
}

static void printOutput() {
  if (!hasSignature()) {
    Serial.println("  No result signature yet ($6001-$6003)");
    return;
  }
  Serial.println("  ---- ROM text output ($6004+) ----");
  for (uint16_t i = 0; i < 512; ++i) { 
    uint8_t c = nes.bus().cpuRead(uint16_t(0x6004 + i));
    if (!c) break;
    Serial.write((c == '\n' || c == '\r' || (c >= 32 && c <= 126)) ? c : '.');
  }
  Serial.println("\n  ---------------------------------");
}

static void summary() {
  int passed = 0, failed = 0, stopped = 0, timedOut = 0, other = 0;
  Serial.println("\n============== FINAL SUMMARY ==============");
  for (size_t i = 0; i < kRomCount; ++i) {
    Serial.printf("%02u %-16s %-14s", (unsigned)(i + 1), kRoms[i].name, resultName(outcomes[i]));
    if (outcomes[i] == PASS || outcomes[i] == TEST_FAIL)
      Serial.printf(" ($%02X)", statusCodes[i]);
    Serial.println();
    switch (outcomes[i]) {
      case PASS: ++passed; break;
      case TEST_FAIL: ++failed; break;
      case STOP: ++stopped; break;
      case TIMEOUT: ++timedOut; break;
      default: ++other; break;
    }
  }
  Serial.printf("TOTAL: PASS=%d FAIL=%d STOP=%d TIMEOUT=%d OTHER=%d / %u\n",
                passed, failed, stopped, timedOut, other, (unsigned)kRomCount);
  Serial.println("===========================================");
  done = true;
}

static bool loadCurrent() {
  // Completely clear writable bus memory so tests don't contaminate each other.
  // Do NOT clear after a test requests a soft RESET, which intentionally preserves RAM.
  for (uint32_t addr = 0; addr < 0x8000; ++addr)
    nes.bus().cpuWrite(uint16_t(addr), 0); 

  const uint8_t* prg = kRoms[current].prg;
  nes.bus().load(0x8000, prg, 16384);
  nes.bus().load(0xC000, prg + 16384, 16384);
  nes.reset();
  while (!nes.cpu().complete() && !nes.cpu().halted()) nes.clock();
  return !nes.cpu().halted();
}

static void finishCurrent(Result r, uint8_t code = 0xFF) {
  outcomes[current] = r;
  statusCodes[current] = code;
  Serial.printf("  RESULT %s", resultName(r));
  if (hasSignature()) Serial.printf(" status=$%02X", nes.bus().cpuRead(0x6000));
  Serial.printf(" clocks=%llu\n", (unsigned long long)steps);
  printOutput();
  ++current;
  if (current >= kRomCount) {
    summary();
    return;
  }
  Serial.printf("\n[%u/%u] %s\n", (unsigned)(current + 1), (unsigned)kRomCount,
                kRoms[current].name);
  resetCount = 0;
  steps = 0;
  lastReport = millis();
  testStarted = millis();
  if (!loadCurrent()) finishCurrent(LOAD_ERROR);
  else Serial.printf("  Starting at PC=$%04X\n", nes.cpu().registers().PC);
}

void setup() {
  Serial.begin(115200);
  delay(700);
  Serial.println("\n=== ESP32-S3 CPU6502 : instr_test-v5, ALL 16 TESTS ===");
  Serial.println("Independent mapper-0 test ROMs, no display or user intervention.");
  Serial.println("STOP means unsupported opcode or emulation halt, not a test FAIL.");
  Serial.printf("Per-test timeout: %lu sec / %llu CPU clocks\n",
                (unsigned long)MAX_SECONDS_PER_TEST,
                (unsigned long long)MAX_CLOCKS_PER_TEST);
  current = 0;
  steps = 0;
  testStarted = lastReport = millis();
  Serial.printf("\n[1/%u] %s\n", (unsigned)kRomCount, kRoms[0].name);
  if (!loadCurrent()) finishCurrent(LOAD_ERROR);
  else Serial.printf("  Starting at PC=$%04X\n", nes.cpu().registers().PC);
}

void loop() {
  if (done) { delay(50); return; }

  for (uint32_t tick = 0; tick < 50000 && !done; ++tick) {
    // At every instruction boundary, remember address/opcode for STOP diagnostics.
    if (nes.cpu().complete()) {
      lastInstructionPC = nes.cpu().registers().PC;
      lastInstructionOp = nes.bus().cpuRead(lastInstructionPC);
    }
    nes.clock();
    ++steps;
    if (nes.cpu().halted()) {
      Serial.printf("  HALT PC=$%04X last instruction PC=$%04X OP=$%02X\n",
                    nes.cpu().registers().PC, lastInstructionPC, lastInstructionOp);
      finishCurrent(STOP);
      break;
    }
    // Status updates take effect when an instruction completes, so check at boundary.
    if (nes.cpu().complete() && hasSignature()) {
      uint8_t status = nes.bus().cpuRead(0x6000);
      if (status < 0x80) {
        finishCurrent(status == 0 ? PASS : TEST_FAIL, status);
        break;
      }
      if (status == 0x81) {
        if (++resetCount > MAX_RESETS_PER_TEST) {
          finishCurrent(RESET_REQUEST, status);
        } else {
          Serial.println("  ROM requested RESET ($81): preserving memory, reset in 110 ms");
          delay(110);
          nes.reset();
          while (!nes.cpu().complete() && !nes.cpu().halted()) nes.clock();
        }
        break;
      }
    }
  }
  if (done) return;
  uint32_t now = millis();
  if (steps >= MAX_CLOCKS_PER_TEST || now - testStarted >= MAX_SECONDS_PER_TEST * 1000UL) {
    Serial.printf("  LIMIT reached at PC=$%04X sig=%s\n",
                  nes.cpu().registers().PC, hasSignature() ? "yes" : "no");
    finishCurrent(TIMEOUT);
  } else if (now - lastReport >= 5000) {
    lastReport = now;
    Serial.printf("  RUNNING PC=$%04X clock=%llu sig=%s status=$%02X\n",
                  nes.cpu().registers().PC, (unsigned long long)steps,
                  hasSignature() ? "yes" : "no", nes.bus().cpuRead(0x6000));
  }
}