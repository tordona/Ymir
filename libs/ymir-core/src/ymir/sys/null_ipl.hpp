#pragma once

// An extremely basic IPL ROM that locks the master SH2 in an infinite do-nothing loop.

#include <ymir/sys/memory_defs.hpp>

#include <ymir/util/data_ops.hpp>

#include <ymir/core/types.hpp>

#include <array>

namespace ymir::nullipl {

inline constexpr uint32 kResetPC = 0x200u;       // Must not be less than 0x200
inline constexpr uint32 kIntrHandlerPC = 0x300u; // Must not be less than 0x200
inline constexpr uint32 kStackLocation = 0x6008000u;

inline const auto kNullIPL = [] {
    std::array<uint8, sys::kIPLSize> ipl{};
    ipl.fill(0);

    // Write vector table
    util::WriteBE<uint32>(&ipl[0x0], kResetPC | 0x20000000u); // Power-on reset PC value
    util::WriteBE<uint32>(&ipl[0x4], kStackLocation);         // Power-on reset SP value
    util::WriteBE<uint32>(&ipl[0x8], kResetPC | 0x20000000u); // Manual reset PC value
    util::WriteBE<uint32>(&ipl[0xC], kStackLocation);         // Manual reset SP value

    // Point every vector to the infinite loop routine
    for (uint32 addr = 0x10; addr < 0x200; addr += 4) {
        util::WriteBE<uint32>(&ipl[addr], kIntrHandlerPC);
    }

    // Write the code
    uint32 pc = kResetPC;
    auto write = [&](uint16 opcode) {
        util::WriteBE<uint16>(&ipl[pc], opcode);
        pc += sizeof(uint16);
    };

    // Infinite loop version
    // write(0x9001); // mov.w @(pc+2), r0   (= 0x00F0)
    // write(0x8BFE); // bf <self>
    // write(0x00F0); // data.w #0x00F0

    // Sleep forever version
    write(0x9006); //   mov.w @(<srval>), r0     ; get value of SR (=0x00F0)
    write(0x400E); //   ldc   r0, sr             ; set SR -> disable interrupts, clear T
    write(0x9005); //   mov.w @(<sbycrval>), r0  ; get address of SBYCR
    write(0xE19F); //   mov #0x9F, r1            ; value of SBYCR: standby mode, halt all modules
    write(0x2010); //   mov r1, @r0              ; set SBYCR
                   // loop:
    write(0x001B); //   sleep                    ; good night!
    write(0xAFFD); //   bra <loop>               ; in case you have NMIghtmares,
    write(0x0009); //   > nop                    ;   do nothing and go back to sleep
                   // srval:
    write(0x00F0); //   data.w #0x00F0           ; M=0, Q=0, T=0, I3-0=0xF
    write(0xFE91); //   data.w #0xFE91           ; address of SBYCR

    pc = kIntrHandlerPC;
    write(0x000B); // rte
    write(0x0009); // nop

    return ipl;
}();

} // namespace ymir::nullipl
