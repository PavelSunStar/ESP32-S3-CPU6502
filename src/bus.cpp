#include "bus.h"

uint8_t Bus::cpuRead(uint16_t address) const {
    if (address < 0x2000) return _ram[address & 0x07FF];
    return _testMemory[address - 0x2000];
}

void Bus::cpuWrite(uint16_t address, uint8_t value) {
    if (address < 0x2000) _ram[address & 0x07FF] = value;
    else _testMemory[address - 0x2000] = value;
}

void Bus::load(uint16_t address, const uint8_t* data, uint16_t size) {
    for (uint32_t i = 0; i < size && uint32_t(address) + i < 65536; ++i)
        cpuWrite(uint16_t(address + i), data[i]);
}
