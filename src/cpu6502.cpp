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
    _dummyReadPending = false;
    _dummyReadAddress = 0;
    _irqLine = false;
    _nmiPending = false;
}

void CPU6502::enterInterrupt(uint16_t vector) {
    // Instruction-boundary approximation, not individual bus-cycle accuracy.
    read(_reg.PC);  // discarded prefetch (simplified)
    write(0x0100 | _reg.SP--, uint8_t(_reg.PC >> 8));
    write(0x0100 | _reg.SP--, uint8_t(_reg.PC));
    write(0x0100 | _reg.SP--, uint8_t((_reg.SR.reg | 0x20) & ~0x10));
    _reg.SR.I = 1;
    _reg.SR.B = 0;
    _reg.SR.U = 1;
    uint8_t lo = read(vector);
    uint8_t hi = read(uint16_t(vector + 1));
    _reg.PC = uint16_t(lo) | (uint16_t(hi) << 8);
    _cycles = 7;
}

void CPU6502::clock() {
    if (_halted || !_bus) return;
    if (_cycles == 0) {
        if (_nmiPending) {
            _nmiPending = false;
            enterInterrupt(0xFFFA);
        } else if (_irqLine && !_reg.SR.I) {
            enterInterrupt(0xFFFE);
        } else {
            _opcode = read(_reg.PC++);
            const Instruction& instr = instructions[_opcode];
            _cycles = instr.cycles;
            uint8_t extra1 = (this->*instr.addressMode)();
            uint8_t extra2 = (this->*instr.execute)();
            _dummyReadPending = false;
            _cycles += extra1 & extra2;
            _reg.SR.U = 1;
        }
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
    // A page crossing produces a read from the uncorrected address
    // before the final read (for memory-reading instructions).
    _dummyReadPending = (base & 0xFF00) != (_addr_abs & 0xFF00);
    _dummyReadAddress = (base & 0xFF00) | (_addr_abs & 0x00FF);
    return _dummyReadPending;
}

uint8_t CPU6502::ABY() {
    uint16_t lo = read(_reg.PC++);
    uint16_t hi = read(_reg.PC++);
    uint16_t base = (hi << 8) | lo;
    _addr_abs = uint16_t(base + _reg.Y);
    // A page crossing produces a read from the uncorrected address
    // before the final read (for memory-reading instructions).
    _dummyReadPending = (base & 0xFF00) != (_addr_abs & 0xFF00);
    _dummyReadAddress = (base & 0xFF00) | (_addr_abs & 0x00FF);
    return _dummyReadPending;
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
    // A page crossing produces a read from the uncorrected address
    // before the final read (for memory-reading instructions).
    _dummyReadPending = (base & 0xFF00) != (_addr_abs & 0xFF00);
    _dummyReadAddress = (base & 0xFF00) | (_addr_abs & 0x00FF);
    return _dummyReadPending;
}

uint8_t CPU6502::fetch() {
    if (_dummyReadPending) {
        read(_dummyReadAddress);
        _dummyReadPending = false;
    }
    _fetched = read(_addr_abs);
    return _fetched;
}
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

uint8_t CPU6502::Instr_NOP() {

    // NOP с адресацией ABX:
    // дополнительный такт при пересечении страницы.
    if (_opcode == 0x1C || _opcode == 0x3C ||
        _opcode == 0x5C || _opcode == 0x7C ||
        _opcode == 0xDC || _opcode == 0xFC) {
        fetch();
        return 1;
    }

    // NOP с операндом в памяти.
    switch (_opcode) {
        case 0x04: case 0x44: case 0x64:
        case 0x14: case 0x34: case 0x54:
        case 0x74: case 0xD4: case 0xF4:
        case 0x0C:
            fetch();
            break;
    }

    return 0;
}

uint8_t CPU6502::Instr_SLO() {

    uint8_t value = fetch();

    // NMOS 6502 RMW: dummy write of the original byte.
    write(_addr_abs, value);

    // Старший бит уходит в Carry
    _reg.SR.C = (value & 0x80) != 0;

    // Сдвиг влево
    value <<= 1;

    // Записываем результат обратно в память
    write(_addr_abs, value);

    // Логическое ИЛИ с аккумулятором
    _reg.A |= value;

    // Обновляем флаги Zero и Negative
    setZN(_reg.A);

    return 0;
}

uint8_t CPU6502::Instr_RLA() {

    uint8_t value = fetch();

    // NMOS 6502 RMW: dummy write of the original byte.
    write(_addr_abs, value);

    // Запоминаем старый Carry
    uint8_t oldCarry = _reg.SR.C;

    // Старший бит переходит в Carry
    _reg.SR.C = (value & 0x80) != 0;

    // Вращение влево через Carry
    value = uint8_t((value << 1) | oldCarry);

    // Записываем результат в память
    write(_addr_abs, value);

    // AND с аккумулятором
    _reg.A &= value;

    // Обновляем Zero и Negative
    setZN(_reg.A);

    return 0;
}

uint8_t CPU6502::Instr_SRE() {

    uint8_t value = fetch();

    // NMOS 6502 RMW: dummy write of the original byte.
    write(_addr_abs, value);

    // Младший бит переходит в Carry
    _reg.SR.C = value & 0x01;

    // Сдвигаем значение вправо
    value >>= 1;

    // Записываем результат в память
    write(_addr_abs, value);

    // XOR с аккумулятором
    _reg.A ^= value;

    // Обновляем Zero и Negative
    setZN(_reg.A);

    return 0;
}

uint8_t CPU6502::Instr_RRA() {

    uint8_t value = fetch();

    // NMOS 6502 RMW: dummy write of the original byte.
    write(_addr_abs, value);

    // Запоминаем старый Carry
    uint8_t oldCarry = _reg.SR.C;

    // ROR: младший бит переходит в Carry
    _reg.SR.C = value & 0x01;

    value = (value >> 1) | (oldCarry << 7);

    // Записываем результат ROR в память
    write(_addr_abs, value);

    // ADC с новым Carry
    uint16_t sum = uint16_t(_reg.A) + value + _reg.SR.C;

    // Overflow вычисляем до изменения A
    _reg.SR.V =
        ((~(_reg.A ^ value) & (_reg.A ^ sum)) & 0x80) != 0;

    _reg.SR.C = sum > 0xFF;

    _reg.A = uint8_t(sum);

    setZN(_reg.A);

    return 0;
}

uint8_t CPU6502::Instr_SAX() {
    uint8_t value = _reg.A & _reg.X;

    write(_addr_abs, value);

    return 0;
}

uint8_t CPU6502::Instr_LAX() {
    uint8_t value = fetch();

    _reg.A = value;
    _reg.X = value;

    setZN(value);

    return 1;
}

uint8_t CPU6502::Instr_ANC() {

    // AND аккумулятора с непосредственным значением
    _reg.A &= fetch();

    // Обновляем Zero и Negative
    setZN(_reg.A);

    // Carry равен старшему биту результата
    _reg.SR.C = _reg.SR.N;

    return 0;
}

uint8_t CPU6502::Instr_DCP() {

    // Читаем значение памяти
    uint8_t value = fetch();

    // NMOS 6502 RMW: dummy write of the original byte.
    write(_addr_abs, value);

    // Уменьшаем на единицу
    value--;

    // Записываем результат обратно
    write(_addr_abs, value);

    // Сравниваем A с новым значением
    uint16_t result = uint16_t(_reg.A) - value;

    // Флаги как у CMP
    _reg.SR.C = _reg.A >= value;
    _reg.SR.Z = uint8_t(result) == 0;
    _reg.SR.N = (result & 0x80) != 0;

    return 0;
}

uint8_t CPU6502::Instr_ISC() {

    // Читаем значение памяти
    uint8_t value = fetch();

    // NMOS 6502 RMW: dummy write of the original byte.
    write(_addr_abs, value);

    // INC: увеличиваем на единицу
    value++;

    // Записываем новое значение
    write(_addr_abs, value);

    // SBC: A = A - value - (1 - Carry)
    uint16_t result =
        uint16_t(_reg.A) - value - (1 - _reg.SR.C);

    // Вычисляем флаги
    _reg.SR.C = result < 0x100;

    uint8_t r = uint8_t(result);

    _reg.SR.V =
        ((_reg.A ^ r) & (_reg.A ^ value) & 0x80) != 0;

    // Сохраняем результат в аккумуляторе
    _reg.A = r;

    // Обновляем Z и N
    setZN(_reg.A);

    return 0;
}

uint8_t CPU6502::Instr_ALR() {

    // AND с непосредственным значением
    _reg.A &= fetch();

    // Младший бит результата переходит в Carry
    _reg.SR.C = _reg.A & 0x01;

    // Сдвигаем аккумулятор вправо
    _reg.A >>= 1;

    // Обновляем флаги Zero и Negative
    setZN(_reg.A);

    return 0;
}

uint8_t CPU6502::Instr_ARR() {

    // Сначала AND с непосредственным значением
    uint8_t value = _reg.A & fetch();

    // Старый Carry используется при вращении
    uint8_t oldCarry = _reg.SR.C;

    // Вращаем вправо через Carry
    _reg.A = (value >> 1) | (oldCarry << 7);

    // Обновляем Zero и Negative
    setZN(_reg.A);

    // Carry получает бит 6 результата
    _reg.SR.C = (_reg.A & 0x40) != 0;

    // Overflow = бит 6 XOR бит 5
    _reg.SR.V =
        ((_reg.A >> 6) ^ (_reg.A >> 5)) & 0x01;

    return 0;
}

uint8_t CPU6502::Instr_SHY() {

    // ABX() уже вычислила эффективный адрес.
    uint16_t address = _addr_abs;

    // Восстанавливаем исходный адрес без индекса X.
    uint16_t base = uint16_t(address - _reg.X);

    uint8_t high = uint8_t(base >> 8);

    // Значение, которое SHY записывает в память.
    uint8_t value = _reg.Y & uint8_t(high + 1);

    // Проверяем пересечение страницы.
    if ((base & 0xFF00) != (address & 0xFF00)) {

        // Особенность неофициальной инструкции:
        // старший байт адреса заменяется значением.
        address = (uint16_t(value) << 8) |
                  (address & 0x00FF);
    }

    write(address, value);

    return 0;
}

uint8_t CPU6502::Instr_LXA() {
    uint8_t value = fetch();

    _reg.A = value;
    _reg.X = value;

    setZN(value);

    return 0;
}

uint8_t CPU6502::Instr_BRK() {

    // PC уже указывает на байт после opcode.
    // BRK пропускает ещё один байт.
    ++_reg.PC;

    // Сохраняем PC в стек: сначала старший байт.
    write(0x0100 | _reg.SP, uint8_t(_reg.PC >> 8));
    --_reg.SP;

    // Затем младший.
    write(0x0100 | _reg.SP, uint8_t(_reg.PC));
    --_reg.SP;

    // Сохраняем статус с установленными B и U.
    write(0x0100 | _reg.SP, _reg.SR.reg | 0x30);
    --_reg.SP;

    // Запрещаем маскируемые IRQ.
    _reg.SR.I = 1;

    // Переход на обработчик IRQ/BRK.
    uint16_t lo = read(0xFFFE);
    uint16_t hi = read(0xFFFF);

    _reg.PC = (hi << 8) | lo;

    return 0;
}

uint8_t CPU6502::Instr_AXS() {

    uint8_t value = fetch();

    // Побитовое AND регистров A и X
    uint8_t ax = _reg.A & _reg.X;

    // Вычитание без учёта старого Carry
    uint16_t result = uint16_t(ax) - value;

    // Carry = 1, если заёма не было
    _reg.SR.C = ax >= value;

    // Записываем результат только в X
    _reg.X = uint8_t(result);

    // Обновляем Z и N
    setZN(_reg.X);

    return 0;
}

uint8_t CPU6502::Instr_XXX() { _halted = true; return 0; }


// ============================================================
// Temporary stubs: declared interfaces, not implemented yet.
// An unsupported opcode is handled by Instr_XXX().
// ============================================================

uint8_t CPU6502::Instr_CLI() {
    _reg.SR.I = 0;
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

    // Only memory variants perform the original-value dummy write.
    if (!accumulator) write(_addr_abs, value);

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

    // Only memory variants perform the original-value dummy write.
    if (!accumulator) write(_addr_abs, value);

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

    // NMOS 6502 RMW: dummy write of the original byte.
    write(_addr_abs, value);

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

    // NMOS 6502 RMW: dummy write of the original byte.
    write(_addr_abs, value);

    ++value;

    write(_addr_abs, value);
    setZN(value);

    return 0;
}

uint8_t CPU6502::Instr_ROL() {
    bool accumulator = (_opcode == 0x2A);

    uint8_t value = accumulator ? _reg.A : fetch();

    // Only memory variants perform the original-value dummy write.
    if (!accumulator) write(_addr_abs, value);

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

uint8_t CPU6502::Instr_SHX() {

    // ABY() уже вычислила эффективный адрес
    uint16_t address = _addr_abs;

    // Восстанавливаем исходный адрес без Y
    uint16_t base = uint16_t(address - _reg.Y);

    // Старший байт исходного адреса + 1
    uint8_t high = uint8_t((base >> 8) + 1);

    // Значение для записи
    uint8_t value = _reg.X & high;

    // Особенность SHX при пересечении страницы
    if ((base & 0xFF00) != (address & 0xFF00)) {
        address = (uint16_t(value) << 8) |
                  (address & 0x00FF);
    }

    write(address, value);

    return 0;
}

uint8_t CPU6502::Instr_TXS() {
    _reg.SP = _reg.X;
    return 0;
}

uint8_t CPU6502::Instr_ROR() {
    bool accumulator = (_opcode == 0x6A);

    uint8_t value = accumulator ? _reg.A : fetch();

    // Only memory variants perform the original-value dummy write.
    if (!accumulator) write(_addr_abs, value);

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