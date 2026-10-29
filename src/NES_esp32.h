#pragma once

#include <stdint.h>
#include "cpu6502.h"
#include "bus.h"

class NES_esp32 {
public:
    NES_esp32();

    void reset();
    void clock();

    CPU6502& cpu() { return _cpu; }
    Bus& bus() { return _bus; }

private:
    Bus _bus;
    CPU6502 _cpu;
};