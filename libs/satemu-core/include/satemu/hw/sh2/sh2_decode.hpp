#pragma once

#include <satemu/core_types.hpp>

namespace satemu::sh2 {

// -----------------------------------------------------------------------------
// Instruction decoder

// xxxx: instruction code
// mmmm: source register (Rm)
// nnnn: destination register (Rn)
// iiii: immediate data (imm)
// dddd: displacement (disp)

// 0 format: xxxx xxxx xxxx xxxx

// n format: xxxx nnnn xxxx xxxx
union InstrN {
    InstrN(uint16 u16)
        : u16(u16) {}

    uint16 u16;
    struct {
        uint16 : 8;
        uint16 Rn : 4;
    };
};

// m format: xxxx mmmm xxxx xxxx
union InstrM {
    InstrM(uint16 u16)
        : u16(u16) {}

    uint16 u16;
    struct {
        uint16 : 8;
        uint16 Rm : 4;
    };
};

// nm format: xxxx nnnn mmmm xxxx
union InstrNM {
    InstrNM(uint16 u16)
        : u16(u16) {}

    uint16 u16;
    struct {
        uint16 : 4;
        uint16 Rm : 4;
        uint16 Rn : 4;
    };
};

// md format: xxxx xxxx mmmm dddd
union InstrMD {
    InstrMD(uint16 u16)
        : u16(u16) {}

    uint16 u16;
    struct {
        uint16 disp : 4;
        uint16 Rm : 4;
    };
};

// nd4 format: xxxx xxxx nnnn dddd
union InstrND4 {
    InstrND4(uint16 u16)
        : u16(u16) {}

    uint16 u16;
    struct {
        uint16 disp : 4;
        uint16 Rn : 4;
    };
};

// nmd format: xxxx nnnn mmmm dddd
union InstrNMD {
    InstrNMD(uint16 u16)
        : u16(u16) {}

    uint16 u16;
    struct {
        uint16 disp : 4;
        uint16 Rm : 4;
        uint16 Rn : 4;
    };
};

// d format: xxxx xxxx dddd dddd
union InstrD {
    InstrD(uint16 u16)
        : u16(u16) {}

    uint16 u16;
    struct {
        uint16 disp : 8;
    };
};

// d12 format: xxxx dddd dddd dddd
union InstrD12 {
    InstrD12(uint16 u16)
        : u16(u16) {}

    uint16 u16;
    struct {
        uint16 disp : 12;
    };
};

// nd8 format: xxxx nnnn dddd dddd
union InstrND8 {
    InstrND8(uint16 u16)
        : u16(u16) {}

    uint16 u16;
    struct {
        uint16 disp : 8;
        uint16 Rn : 4;
    };
};

// i format: xxxx xxxx iiii iiii
union InstrI {
    InstrI(uint16 u16)
        : u16(u16) {}

    uint16 u16;
    struct {
        uint16 imm : 8;
    };
};

// ni format: xxxx nnnn iiii iiii
union InstrNI {
    InstrNI(uint16 u16)
        : u16(u16) {}

    uint16 u16;
    struct {
        uint16 imm : 8;
        uint16 Rn : 4;
    };
};

} // namespace satemu::sh2
