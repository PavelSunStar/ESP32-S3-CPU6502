#include "cpu6502.h"
#include "bus.h"

uint8_t CPU6502::read(uint16_t address) {
    return _bus->cpuRead(address);
}
void CPU6502::write(uint16_t address, uint8_t value) {
    _bus->cpuWrite(address, value);
}

void CPU6502::setZN(uint8_t value) {
    _reg.SR.Z = (value == 0);
    _reg.SR.N = (value & 0x80) != 0;
}

void CPU6502::reset() {
    if (!_bus) { _halted = true; return; }
    _reg.A = _reg.X = _reg.Y = 0;
    _reg.SP = 0xFD;
    _reg.SR.reg = 0x24;
    uint8_t lo = read(0xFFFC);
    uint8_t hi = read(0xFFFD);
    _reg.PC = (uint16_t(hi) << 8) | lo;
    _addr_abs = 0;
    _addr_rel = 0;
    _fetched = 0;
    _opcode = 0;
    _cycles = 8;
    _totalCycles = 0;
    _halted = false;
}

void CPU6502::clock() {
    if (_halted || !_bus) return;
    if (_cycles == 0) {
        _opcode = read(_reg.PC++);
        const Instruction& instr = instructions[_opcode];
        _cycles = instr.cycles;
        uint8_t extra1 = (this->*instr.addressMode)();
        uint8_t extra2 = (this->*instr.execute)();
        _cycles += extra1 & extra2;
        _reg.SR.U = 1;
    }
    --_cycles;
    ++_totalCycles;
}

// ============================================================
// Addressing Modes - all 12 modes of the 6502
// ============================================================

uint8_t CPU6502::IMP() {
    _fetched = _reg.A;
    return 0;
}

uint8_t CPU6502::IMM() {
    _addr_abs = _reg.PC++;
    return 0;
}

uint8_t CPU6502::ZPG() {
    _addr_abs = read(_reg.PC++);
    return 0;
}

uint8_t CPU6502::ZPX() {
    _addr_abs = uint8_t(read(_reg.PC++) + _reg.X);
    return 0;
}

uint8_t CPU6502::ZPY() {
    _addr_abs = uint8_t(read(_reg.PC++) + _reg.Y);
    return 0;
}

uint8_t CPU6502::ABS() {
    uint16_t lo = read(_reg.PC++);
    uint16_t hi = read(_reg.PC++);
    _addr_abs = (hi << 8) | lo;
    return 0;
}

uint8_t CPU6502::ABX() {
    uint16_t lo = read(_reg.PC++);
    uint16_t hi = read(_reg.PC++);
    uint16_t base = (hi << 8) | lo;
    _addr_abs = uint16_t(base + _reg.X);
    return (base & 0xFF00) != (_addr_abs & 0xFF00);
}

uint8_t CPU6502::ABY() {
    uint16_t lo = read(_reg.PC++);
    uint16_t hi = read(_reg.PC++);
    uint16_t base = (hi << 8) | lo;
    _addr_abs = uint16_t(base + _reg.Y);
    return (base & 0xFF00) != (_addr_abs & 0xFF00);
}

uint8_t CPU6502::REL() {
    _addr_rel = read(_reg.PC++);
    if (_addr_rel & 0x80) _addr_rel |= 0xFF00;
    return 0;
}

uint8_t CPU6502::IND() {
    uint16_t lo = read(_reg.PC++);
    uint16_t hi = read(_reg.PC++);
    uint16_t pointer = (hi << 8) | lo;

    // Original NMOS 6502 wraps the high-byte read within the same page.
    uint16_t next = (pointer & 0xFF00) | uint8_t(pointer + 1);
    _addr_abs = uint16_t(read(pointer)) | (uint16_t(read(next)) << 8);
    return 0;
}

uint8_t CPU6502::IDX() {
    uint8_t pointer = uint8_t(read(_reg.PC++) + _reg.X);
    uint16_t lo = read(pointer);
    uint16_t hi = read(uint8_t(pointer + 1));
    _addr_abs = (hi << 8) | lo;
    return 0;
}

uint8_t CPU6502::IDY() {
    uint8_t pointer = read(_reg.PC++);
    uint16_t lo = read(pointer);
    uint16_t hi = read(uint8_t(pointer + 1));
    uint16_t base = (hi << 8) | lo;
    _addr_abs = uint16_t(base + _reg.Y);
    return (base & 0xFF00) != (_addr_abs & 0xFF00);
}

uint8_t CPU6502::fetch() { _fetched = read(_addr_abs); return _fetched; }
uint8_t CPU6502::Instr_LDA() { _reg.A = fetch(); setZN(_reg.A); return 1; }
uint8_t CPU6502::Instr_LDX() { _reg.X = fetch(); setZN(_reg.X); return 1; }
uint8_t CPU6502::Instr_LDY() { _reg.Y = fetch(); setZN(_reg.Y); return 1; }
uint8_t CPU6502::Instr_STA() { write(_addr_abs, _reg.A); return 0; }
uint8_t CPU6502::Instr_TAX() { _reg.X = _reg.A; setZN(_reg.X); return 0; }
uint8_t CPU6502::Instr_INX() { ++_reg.X; setZN(_reg.X); return 0; }
uint8_t CPU6502::Instr_ADC() {
    uint8_t value = fetch();
    uint16_t result = uint16_t(_reg.A) + value + _reg.SR.C;
    _reg.SR.C = result > 0xFF;
    _reg.SR.V = ((~(_reg.A ^ value) & (_reg.A ^ result)) & 0x80) != 0;
    _reg.A = uint8_t(result);
    setZN(_reg.A);
    return 1;
}
uint8_t CPU6502::Instr_NOP() { return 0; }
uint8_t CPU6502::Instr_XXX() { _halted = true; return 0; }


// ============================================================
// Temporary stubs: declared interfaces, not implemented yet.
// An unsupported opcode is handled by Instr_XXX().
// ============================================================

uint8_t CPU6502::Instr_CLI() {
    _halted = true;
    return 0;
}

uint8_t CPU6502::Instr_SED() {
    _reg.SR.D = 1;
    return 0;
}

uint8_t CPU6502::Instr_AND() {
    _reg.A &= fetch();
    setZN(_reg.A);
    return 1;
}

uint8_t CPU6502::Instr_CLV() {
    _reg.SR.V = 0;
    return 0;
}

uint8_t CPU6502::Instr_SEI() {
    _reg.SR.I = 1;
    return 0;
}

uint8_t CPU6502::Instr_ASL() {
    bool accumulator = (_opcode == 0x0A);

    uint8_t value = accumulator ? _reg.A : fetch();

    _reg.SR.C = (value & 0x80) != 0;

    uint8_t result = uint8_t(value << 1);
    setZN(result);

    if (accumulator)
        _reg.A = result;
    else
        write(_addr_abs, result);

    return 0;
}

uint8_t CPU6502::Instr_CMP() {
    uint8_t value = fetch();

    uint16_t result = (uint16_t)_reg.A - value;

    _reg.SR.C = _reg.A >= value;
    _reg.SR.Z = (uint8_t)result == 0;
    _reg.SR.N = (result & 0x80) != 0;

    return 1;
}

uint8_t CPU6502::Instr_LSR() {
    bool accumulator = (_opcode == 0x4A);

    uint8_t value = accumulator ? _reg.A : fetch();

    _reg.SR.C = value & 1;

    uint8_t result = value >> 1;
    setZN(result);

    if (accumulator)
        _reg.A = result;
    else
        write(_addr_abs, result);

    return 0;
}

uint8_t CPU6502::Instr_BCC() {
    if (!_reg.SR.C) {
        ++_cycles;
        uint16_t target = uint16_t(_reg.PC + _addr_rel);
        if ((_reg.PC & 0xFF00) != (target & 0xFF00)) ++_cycles;
        _reg.PC = target;
    }
    return 0;
}

uint8_t CPU6502::Instr_CPX() {
    uint8_t value = fetch();

    uint16_t result = (uint16_t)_reg.X - value;

    _reg.SR.C = _reg.X >= value;
    _reg.SR.Z = (uint8_t)result == 0;
    _reg.SR.N = (result & 0x80) != 0;

    return 0;
}

uint8_t CPU6502::Instr_STX() {
    write(_addr_abs, _reg.X);
    return 0;
}

uint8_t CPU6502::Instr_BCS() {
    if (_reg.SR.C) {
        ++_cycles;  // Taken branch: +1 cycle
        uint16_t target = uint16_t(_reg.PC + _addr_rel);
        if ((_reg.PC & 0xFF00) != (target & 0xFF00)) ++_cycles;
        _reg.PC = target;
    }
    return 0;
}

uint8_t CPU6502::Instr_CPY() {
    uint8_t value = fetch();

    uint16_t result = (uint16_t)_reg.Y - value;

    _reg.SR.C = _reg.Y >= value;
    _reg.SR.Z = (uint8_t)result == 0;
    _reg.SR.N = (result & 0x80) != 0;

    return 0;
}

uint8_t CPU6502::Instr_ORA() {
    _reg.A |= fetch();
    setZN(_reg.A);
    return 1;
}

uint8_t CPU6502::Instr_STY() {
    write(_addr_abs, _reg.Y);
    return 0;
}

uint8_t CPU6502::Instr_BEQ() {
    if (_reg.SR.Z) {
        ++_cycles; // Branch taken
        uint16_t target = uint16_t(_reg.PC + _addr_rel);
        if ((_reg.PC & 0xFF00) != (target & 0xFF00)) ++_cycles;
        _reg.PC = target;
    }
    return 0;
}

uint8_t CPU6502::Instr_DEC() {
    uint8_t value = fetch();

    --value;

    write(_addr_abs, value);
    setZN(value);

    return 0;
}

uint8_t CPU6502::Instr_PHA() {
    write(0x0100 | _reg.SP, _reg.A);
    _reg.SP--;
    return 0;
}

uint8_t CPU6502::Instr_BIT() {
    uint8_t value = fetch();
    _reg.SR.Z = ((_reg.A & value) == 0);
    _reg.SR.V = (value & 0x40) != 0;
    _reg.SR.N = (value & 0x80) != 0;
    return 0;
}

uint8_t CPU6502::Instr_DEX() {
    _reg.X--;
    setZN(_reg.X);
    return 0;
}

uint8_t CPU6502::Instr_PHP() {
    uint8_t status = _reg.SR.reg | 0x30;

    write(0x0100 | _reg.SP, status);
    _reg.SP--;

    return 0;
}

uint8_t CPU6502::Instr_TAY() {
    _reg.Y = _reg.A;
    setZN(_reg.Y);
    return 0;
}

uint8_t CPU6502::Instr_BMI() {
    if (_reg.SR.N) {
        ++_cycles;

        uint16_t target = uint16_t(_reg.PC + _addr_rel);

        if ((_reg.PC & 0xFF00) != (target & 0xFF00))
            ++_cycles;

        _reg.PC = target;
    }

    return 0;
}

uint8_t CPU6502::Instr_DEY() {
    _reg.Y--;
    setZN(_reg.Y);
    return 0;
}

uint8_t CPU6502::Instr_PLA() {
    _reg.SP++;

    _reg.A = read(0x0100 | _reg.SP);

    setZN(_reg.A);

    return 0;
}

uint8_t CPU6502::Instr_TSX() {
    _reg.X = _reg.SP;
    setZN(_reg.X);
    return 0;
}

uint8_t CPU6502::Instr_BNE() {
    if (!_reg.SR.Z) {
        ++_cycles;
        uint16_t target = uint16_t(_reg.PC + _addr_rel);
        if ((_reg.PC & 0xFF00) != (target & 0xFF00)) ++_cycles;
        _reg.PC = target;
    }
    return 0;
}

uint8_t CPU6502::Instr_EOR() {
    _reg.A ^= fetch();
    setZN(_reg.A);
    return 1;
}

uint8_t CPU6502::Instr_PLP() {
    _reg.SP++;

    uint8_t status = read(0x0100 | _reg.SP);

    _reg.SR.reg = (status & 0xEF) | 0x20;

    return 0;
}

uint8_t CPU6502::Instr_TXA() {
    _reg.A = _reg.X;
    setZN(_reg.A);
    return 0;
}

uint8_t CPU6502::Instr_BPL() {
    if (!_reg.SR.N) {
        ++_cycles;

        uint16_t target = uint16_t(_reg.PC + _addr_rel);

        if ((_reg.PC & 0xFF00) != (target & 0xFF00))
            ++_cycles;

        _reg.PC = target;
    }
    return 0;
}

uint8_t CPU6502::Instr_INC() {
    uint8_t value = fetch();

    ++value;

    write(_addr_abs, value);
    setZN(value);

    return 0;
}

uint8_t CPU6502::Instr_ROL() {
    bool accumulator = (_opcode == 0x2A);

    uint8_t value = accumulator ? _reg.A : fetch();

    uint8_t oldCarry = _reg.SR.C;

    _reg.SR.C = (value & 0x80) != 0;

    uint8_t result = uint8_t((value << 1) | oldCarry);

    setZN(result);

    if (accumulator)
        _reg.A = result;
    else
        write(_addr_abs, result);

    return 0;
}

uint8_t CPU6502::Instr_TXS() {
    _reg.SP = _reg.X;
    return 0;
}

uint8_t CPU6502::Instr_BRK() {
    _halted = true;
    return 0;
}

uint8_t CPU6502::Instr_ROR() {
    bool accumulator = (_opcode == 0x6A);

    uint8_t value = accumulator ? _reg.A : fetch();

    uint8_t oldCarry = _reg.SR.C;

    _reg.SR.C = value & 1;

    uint8_t result = (value >> 1) | (oldCarry << 7);

    setZN(result);

    if (accumulator)
        _reg.A = result;
    else
        write(_addr_abs, result);

    return 0;
}

uint8_t CPU6502::Instr_TYA() {
    _reg.A = _reg.Y;
    setZN(_reg.A);
    return 0;
}

uint8_t CPU6502::Instr_BVC() {
    if (!_reg.SR.V) {
        ++_cycles; // Taken branch: one extra CPU cycle.
        uint16_t target = uint16_t(_reg.PC + _addr_rel);
        if ((_reg.PC & 0xFF00) != (target & 0xFF00)) ++_cycles;
        _reg.PC = target;
    }
    return 0;
}

uint8_t CPU6502::Instr_INY() {
    _reg.Y++;
    setZN(_reg.Y);
    return 0;
}

uint8_t CPU6502::Instr_RTI() {
    // Восстанавливаем регистр состояния
    _reg.SP++;
    _reg.SR.reg = read(0x0100 | _reg.SP);

    // B очищаем, U устанавливаем
    _reg.SR.B = 0;
    _reg.SR.U = 1;

    // Восстанавливаем младший байт PC
    _reg.SP++;
    uint16_t lo = read(0x0100 | _reg.SP);

    // Восстанавливаем старший байт PC
    _reg.SP++;
    uint16_t hi = read(0x0100 | _reg.SP);

    _reg.PC = (hi << 8) | lo;

    return 0;
}

uint8_t CPU6502::Instr_CLD() {
    _reg.SR.D = 0;
    return 0;
}

uint8_t CPU6502::Instr_BVS() {
    if (_reg.SR.V) {
        ++_cycles; // Taken branch: one extra CPU cycle.
        uint16_t target = uint16_t(_reg.PC + _addr_rel);
        if ((_reg.PC & 0xFF00) != (target & 0xFF00)) ++_cycles;
        _reg.PC = target;
    }
    return 0;
}

uint8_t CPU6502::Instr_JMP() {
    // ABS/IND have already calculated the target address.
    _reg.PC = _addr_abs;
    return 0;
}

uint8_t CPU6502::Instr_RTS() {
    // Reverse of JSR: pull low, then high, and resume at saved address + 1.
    ++_reg.SP;
    const uint16_t lo = read(uint16_t(0x0100 | _reg.SP));
    ++_reg.SP;
    const uint16_t hi = read(uint16_t(0x0100 | _reg.SP));
    _reg.PC = uint16_t(((hi << 8) | lo) + 1);
    return 0;
}

uint8_t CPU6502::Instr_CLC() {
    _reg.SR.C = 0;
    return 0;
}

uint8_t CPU6502::Instr_JSR() {
    // ABS() has already read both operand bytes; PC points to next instruction.
    // 6502 pushes PC-1: high byte first, then low byte.
    const uint16_t ret = uint16_t(_reg.PC - 1);
    write(uint16_t(0x0100 | _reg.SP), uint8_t(ret >> 8));
    --_reg.SP;
    write(uint16_t(0x0100 | _reg.SP), uint8_t(ret));
    --_reg.SP;
    _reg.PC = _addr_abs;
    return 0;
}

uint8_t CPU6502::Instr_SBC() {
    uint8_t value = fetch();

    uint16_t result = (uint16_t)_reg.A - value - (1 - _reg.SR.C);

    _reg.SR.C = result < 0x100;

    uint8_t r = (uint8_t)result;

    _reg.SR.V = ((_reg.A ^ r) & (_reg.A ^ value) & 0x80) != 0;

    _reg.A = r;
    setZN(_reg.A);

    return 1;
}

uint8_t CPU6502::Instr_SEC() {
    _reg.SR.C = 1;
    return 0;
}
