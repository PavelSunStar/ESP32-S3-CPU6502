#include <Arduino.h>
#include <NES_esp32.h>
#include "nestest_rom.h"
#include "nestest_expected.h"

NES_esp32 nes; 
    
bool loadNestest() {   
  if (nestest_rom_size < 16 + 16384 + 8192 || 
      nestest_rom[0] != 'N' || nestest_rom[1] != 'E' ||  
      nestest_rom[2] != 'S' || nestest_rom[3] != 0x1A ||
      nestest_rom[4] != 1 || nestest_rom[5] != 1 ||        
      (nestest_rom[6] & 0xF0) != 0 || (nestest_rom[7] & 0xF0) != 0) {      
    Serial.println("ERROR: unexpected iNES ROM");      
    return false;      
  } 
  const uint8_t* prg = nestest_rom + 16;
  nes.bus().load(0x8000, prg, 16384);
  nes.bus().load(0xC000, prg, 16384);
  nes.bus().cpuWrite(0xFFFC, 0x00); 
  nes.bus().cpuWrite(0xFFFD, 0xC0);
  nes.reset();
  while (!nes.cpu().complete() && !nes.cpu().halted()) nes.clock();
  return !nes.cpu().halted();
} 
  
void setup() { 
  Serial.begin(115200); 
  delay(700); 
  Serial.println("\n=== nestest: 1000 instructions, full comparison ==="); 
  if (!loadNestest()) return; 
   
  for (size_t i = 0; i < expectedTraceCount; ++i) {   
    ExpectedState e; 
    memcpy_P(&e, &expectedTrace[i], sizeof(e));  
    const Registers& r = nes.cpu().registers();
    uint16_t pc = r.PC;  
    uint8_t op = nes.bus().cpuRead(pc);  
    // Existing CPU reset counts 8 cycles, nestest starts at 7.
    uint32_t cyc = (uint32_t)(nes.cpu().totalCycles() - 1);   
    bool ok = pc == e.pc && op == e.op && r.A == e.a &&             
              r.X == e.x && r.Y == e.y && r.SR.reg == e.p && 
              r.SP == e.sp && cyc == e.cyc;  
     
    if (!ok) {      
      Serial.printf("FAIL at instruction %u\n", (unsigned)(i + 1));
      Serial.printf("EXPECTED PC:%04X OP:%02X A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%lu\n",
                    e.pc,e.op,e.a,e.x,e.y,e.p,e.sp,(unsigned long)e.cyc);
      Serial.printf("ACTUAL   PC:%04X OP:%02X A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%lu\n",
                    pc,op,r.A,r.X,r.Y,r.SR.reg,r.SP,(unsigned long)cyc);
      Serial.println("STOP: state mismatch");
      return;
    }

    // Print every instruction for first 100; then each 100th.
    if (i < 100 || (i + 1) % 100 == 0)
      Serial.printf("%4u OK PC:%04X OP:%02X A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%lu\n",
                    (unsigned)(i+1),pc,op,r.A,r.X,r.Y,r.SR.reg,r.SP,(unsigned long)cyc);

    do { nes.clock(); } while (!nes.cpu().complete() && !nes.cpu().halted());
    if (nes.cpu().halted()) {
      Serial.printf("STOP: unimplemented instruction %u at PC:%04X OP:%02X\n",
                    (unsigned)(i+1),pc,op);
      return;
    }
  }

  Serial.printf(
    "PASS: all %u reference states match nestest.log!\n",
    (unsigned)expectedTraceCount
);
}

void loop() {}