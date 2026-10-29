#include "NES_esp32.h"

NES_esp32::NES_esp32() {
    _cpu.connectBus(&_bus);
}

void NES_esp32::reset() {
    _cpu.reset();
}

void NES_esp32::clock() {
    _cpu.clock();
}