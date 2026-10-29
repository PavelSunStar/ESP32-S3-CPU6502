#pragma once
#include <stdint.h>

// Рабочая шина для тестирования CPU: 2 КБ NES RAM с зеркалами.
// Остальные адреса хранятся в тестовой памяти до подключения Cartridge/PPU/APU.
class Bus {
    public:
        uint8_t cpuRead(uint16_t address) const;
        void cpuWrite(uint16_t address, uint8_t value);
        void load(uint16_t address, const uint8_t* data, uint16_t size);
    private:
        uint8_t _ram[2048] = {};
        uint8_t _testMemory[65536 - 0x2000] = {};
};