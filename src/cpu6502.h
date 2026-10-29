#pragma once
#include <stdint.h>

class Bus;

union StatusRegister {
    struct {
        uint8_t C : 1;
        uint8_t Z : 1;
        uint8_t I : 1;
        uint8_t D : 1;
        uint8_t B : 1;
        uint8_t U : 1;
        uint8_t V : 1;
        uint8_t N : 1;
    };
    uint8_t reg = 0;
};

struct Registers {
    uint8_t A = 0, X = 0, Y = 0;
    uint16_t PC = 0;
    uint8_t SP = 0;
    StatusRegister SR;
};

class CPU6502 {
    public:
        void connectBus(Bus* bus) { _bus = bus; }
        void reset();
        void clock();
        bool complete()                 const { return _cycles == 0; }
        bool halted()                   const { return _halted; }
        uint8_t opcode()                const { return _opcode; }
        const Registers& registers()    const { return _reg; }
        uint64_t totalCycles()          const { return _totalCycles; }

    private:
        Bus* _bus = nullptr;
        Registers _reg = {};

        uint8_t     _opcode = 0;
        uint8_t     _cycles = 0;
        uint64_t    _totalCycles = 0;
        uint16_t    _addr_abs = 0;
        uint16_t    _addr_rel = 0;
        uint8_t     _fetched = 0;
        bool        _halted = false;

        uint8_t read(uint16_t address);
        void write(uint16_t address, uint8_t value);
        void setZN(uint8_t value);
        uint8_t fetch();

        // Addressing Modes
        uint8_t ABS();	uint8_t IDX();
        uint8_t ABX();	uint8_t IDY();
        uint8_t ABY();	uint8_t REL();
        uint8_t IMM();	uint8_t ZPG();
        uint8_t IMP();	uint8_t ZPX();
        uint8_t IND();	uint8_t ZPY();

        // Instructions
        uint8_t Instr_ADC(); uint8_t Instr_CLI(); uint8_t Instr_LDX(); uint8_t Instr_SED();
        uint8_t Instr_AND(); uint8_t Instr_CLV(); uint8_t Instr_LDY(); uint8_t Instr_SEI();
        uint8_t Instr_ASL(); uint8_t Instr_CMP(); uint8_t Instr_LSR(); uint8_t Instr_STA();
        uint8_t Instr_BCC(); uint8_t Instr_CPX(); uint8_t Instr_NOP(); uint8_t Instr_STX();
        uint8_t Instr_BCS(); uint8_t Instr_CPY(); uint8_t Instr_ORA(); uint8_t Instr_STY();
        uint8_t Instr_BEQ(); uint8_t Instr_DEC(); uint8_t Instr_PHA(); uint8_t Instr_TAX();
        uint8_t Instr_BIT(); uint8_t Instr_DEX(); uint8_t Instr_PHP(); uint8_t Instr_TAY();
        uint8_t Instr_BMI(); uint8_t Instr_DEY(); uint8_t Instr_PLA(); uint8_t Instr_TSX();
        uint8_t Instr_BNE(); uint8_t Instr_EOR(); uint8_t Instr_PLP(); uint8_t Instr_TXA();
        uint8_t Instr_BPL(); uint8_t Instr_INC(); uint8_t Instr_ROL(); uint8_t Instr_TXS();
        uint8_t Instr_BRK(); uint8_t Instr_INX(); uint8_t Instr_ROR(); uint8_t Instr_TYA();
        uint8_t Instr_BVC(); uint8_t Instr_INY(); uint8_t Instr_RTI(); uint8_t Instr_CLD();
        uint8_t Instr_BVS(); uint8_t Instr_JMP(); uint8_t Instr_RTS(); uint8_t Instr_LDA();
        uint8_t Instr_CLC(); uint8_t Instr_JSR(); uint8_t Instr_SBC(); uint8_t Instr_SEC();
        uint8_t Instr_XXX(); 

        uint8_t Instr_SLO(); uint8_t Instr_RLA(); uint8_t Instr_SRE(); uint8_t Instr_RRA();
        uint8_t Instr_SAX(); uint8_t Instr_LAX(); uint8_t Instr_ANC(); uint8_t Instr_DCP(); 
        uint8_t Instr_ISC(); uint8_t Instr_ALR(); uint8_t Instr_ARR(); uint8_t Instr_SHY();
        uint8_t Instr_LXA(); uint8_t Instr_AXS(); uint8_t Instr_SHX();

        struct Instruction {
            const char* name;
            uint8_t (CPU6502::*execute)();
            uint8_t (CPU6502::*addressMode)();
            uint8_t bytes;
            uint8_t cycles;
        };
        static const Instruction instructions[256];
};