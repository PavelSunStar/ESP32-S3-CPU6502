#include <Arduino.h>
#include <NES_esp32.h>

// Tests the installed NES_esp32 library, NOT an isolated CPU copy.
// NOTE: this checks instruction-boundary IRQ/NMI, not PPU/APU or CLI/SEI latency.
static NES_esp32 nes; // Large Bus is global, NOT on the task stack.
static Bus& bus = nes.bus();
static CPU6502& cpu = nes.cpu();
static int passed=0, failed=0;

void test(const char* name, bool ok) {
  Serial.printf("%-27s %s\n", name, ok ? "PASS" : "FAIL");
  if(ok) ++passed; else ++failed;
}
void step() { do { cpu.clock(); } while (!cpu.complete() && !cpu.halted()); }
void ready(const uint8_t* code, uint16_t n) {
  bus.load(0x8000, code, n);
  bus.cpuWrite(0xFFFC, 0x00); bus.cpuWrite(0xFFFD, 0x80);
  bus.cpuWrite(0xFFFA, 0x00); bus.cpuWrite(0xFFFB, 0x90);
  bus.cpuWrite(0xFFFE, 0x00); bus.cpuWrite(0xFFFF, 0xA0);
  bus.cpuWrite(0x9000, 0x40); // RTI at NMI vector
  bus.cpuWrite(0xA000, 0x40); // RTI at IRQ vector
  nes.reset();
  while(!cpu.complete()) cpu.clock();
}
void setup() {
  Serial.begin(115200); delay(300);
  Serial.println("=== CPU6502 IRQ / NMI TEST (CPU-only) ===");
  const uint8_t code[] = {0x58, 0xEA, 0x78, 0xEA}; // CLI,NOP,SEI,NOP
  ready(code,sizeof(code));
  cpu.setIRQLine(true);
  step(); // CLI: IRQ masked at start, should clear I
  test("CLI clears I", cpu.registers().PC==0x8001 && !cpu.registers().SR.I);
  uint64_t c=cpu.totalCycles(); step(); // IRQ at next boundary in simplified model
  test("IRQ vector $FFFE",cpu.registers().PC==0xA000);
  test("IRQ entry costs 7 cycles",cpu.totalCycles()-c==7);
  test("IRQ pushes PC",bus.cpuRead(0x01FD)==0x80 && bus.cpuRead(0x01FC)==0x01);
  test("IRQ pushed B=0 U=1",(bus.cpuRead(0x01FB)&0x30)==0x20);
  test("IRQ sets I and SP",cpu.registers().SR.I && cpu.registers().SP==0xFA);
  cpu.setIRQLine(false);
  step(); // RTI at $A000
  test("RTI restores PC and I",cpu.registers().PC==0x8001 && !cpu.registers().SR.I && cpu.registers().SP==0xFD);
  step(); // NOP
  test("CPU resumes after RTI",cpu.registers().PC==0x8002);
  step(); // SEI
  cpu.setIRQLine(true);
  step(); // next NOP should execute because I=1
  test("IRQ blocked by SEI",cpu.registers().PC==0x8004);
  cpu.setIRQLine(false);

  ready(code,sizeof(code)); // reset: I=1
  cpu.requestNMI();
  c=cpu.totalCycles(); step();
  test("NMI vector $FFFA",cpu.registers().PC==0x9000);
  test("NMI entry costs 7 cycles",cpu.totalCycles()-c==7);
  test("NMI ignores I mask",cpu.registers().SR.I && cpu.registers().SP==0xFA);
  test("NMI pushed B=0 U=1",(bus.cpuRead(0x01FB)&0x30)==0x20);
  step();
  test("NMI RTI restores PC",cpu.registers().PC==0x8000 && cpu.registers().SP==0xFD);
  step();
  test("NMI fires only once",cpu.registers().PC==0x8001);

  ready(code,sizeof(code));
  cpu.setIRQLine(true); cpu.requestNMI(); step();
  test("NMI priority over IRQ",cpu.registers().PC==0x9000);

  Serial.printf("FINAL: PASS=%d FAIL=%d TOTAL=%d\n",passed,failed,passed+failed);
  Serial.println("Original mapper-1 cpu_interrupts.nes is NOT executed here.");
}
void loop() { delay(1000); }