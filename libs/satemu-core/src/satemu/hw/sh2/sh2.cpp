#include <satemu/hw/sh2/sh2.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/inline.hpp>

#include <algorithm>

namespace satemu::sh2 {

SH2::SH2(scu::SCU &scu, smpc::SMPC &smpc)
    : m_bus(scu, smpc) {}

void SH2::Reset(bool hard) {
    auto reset = [&](SH2State &state) {
        state.Reset(hard);
        state.PC = MemReadLong(state, 0x00000000);
        state.R[15] = MemReadLong(state, 0x00000004);
    };

    reset(m_masterState);
    reset(m_slaveState);
}

void SH2::LoadIPL(std::span<uint8, kIPLSize> ipl) {
    m_bus.LoadIPL(ipl);
}

void SH2::Step() {
    // TODO: allow switching between interpreter and JIT recompiler
    Step(m_masterState);
    // TODO: step slave SH2 if enabled
    // Step(m_slaveState);
}

void SH2::SetExternalInterruptCallback(CBAcknowledgeExternalInterrupt callback) {
    m_masterState.SetExternalInterruptCallback(callback);
    // TODO: slave callback?
}

void SH2::SetExternalInterrupt(uint8 level, uint8 vecNum) {
    m_masterState.SetExternalInterrupt(level, vecNum);
    // TODO: when to set slave IRL?
    // m_slaveState.SetExternalInterrupt(level, vecNum);
}

// -------------------------------------------------------------------------
// Debugging functions

/*uint64 dbg_count = 0;
static constexpr uint64 dbg_minCount = 99999999999; // 17635778; // 10489689; // 9302150; // 9547530;

template <typename... T>
void dbg_print(fmt::format_string<T...> fmt, T &&...args) {
    if (dbg_count >= dbg_minCount) {
        fmt::print(fmt, static_cast<T &&>(args)...);
    }
}

template <typename... T>
void dbg_println(fmt::format_string<T...> fmt, T &&...args) {
    if (dbg_count >= dbg_minCount) {
        fmt::println(fmt, static_cast<T &&>(args)...);
    }
}*/

// -------------------------------------------------------------------------
// Helper functions

void SH2::EnterException(SH2State &state, uint8 vectorNumber) {
    state.R[15] -= 4;
    MemWriteLong(state, state.R[15], state.SR.u32);
    state.R[15] -= 4;
    MemWriteLong(state, state.R[15], state.PC - 4);
    state.PC = MemReadLong(state, state.VBR + (static_cast<uint32>(vectorNumber) << 2u));
}

// -----------------------------------------------------------------------------
// Interpreter

FLATTEN void SH2::Step(SH2State &state) {
    /*auto bit = [](bool value, std::string_view bit) { return value ? fmt::format(" {}", bit) : ""; };

    dbg_println(" R0 = {:08X}   R4 = {:08X}   R8 = {:08X}  R12 = {:08X}", R[0], R[4], R[8], R[12]);
    dbg_println(" R1 = {:08X}   R5 = {:08X}   R9 = {:08X}  R13 = {:08X}", R[1], R[5], R[9], R[13]);
    dbg_println(" R2 = {:08X}   R6 = {:08X}  R10 = {:08X}  R14 = {:08X}", R[2], R[6], R[10], R[14]);
    dbg_println(" R3 = {:08X}   R7 = {:08X}  R11 = {:08X}  R15 = {:08X}", R[3], R[7], R[11], R[15]);
    dbg_println("GBR = {:08X}  VBR = {:08X}  MAC = {:08X}.{:08X}", GBR, VBR, MAC.H, MAC.L);
    dbg_println(" PC = {:08X}   PR = {:08X}   SR = {:08X} {}{}{}{}{}{}{}{}", PC, PR, SR.u32, bit(SR.M, "M"),
                bit(SR.Q, "Q"), bit(SR.I3, "I3"), bit(SR.I2, "I2"), bit(SR.I1, "I1"), bit(SR.I0, "I0"), bit(SR.S, "S"),
                bit(SR.T, "T"));*/

    // TODO: choose between interpreter (cached or uncached) and JIT recompiler
    Execute<false>(state, state.PC);
    // dbg_println("");
}

template <bool delaySlot>
void SH2::Execute(SH2State &state, uint32 address) {
    if constexpr (!delaySlot) {
        if (state.CheckInterrupts()) [[unlikely]] {
            fmt::println(">> intr level {:02X} vec {:02X}", state.pendingInterrupt.priority,
                         state.pendingInterrupt.vecNum);
            EnterException(state, state.pendingInterrupt.vecNum);
            state.SR.ILevel = state.pendingInterrupt.priority;
            address = state.PC;
        }
    }

    // TODO: emulate fetch - decode - execute - memory access - writeback pipeline
    const uint16 instr = MemReadWord(state, address);

    /*static uint64 dbg_count = 0;
    ++dbg_count;
    // dbg_print("[{:10}] {:08X}{} {:04X}  ", dbg_count, address, delaySlot ? '*' : ' ', instr);
    if (dbg_count > 14000000) {
        fmt::print("{:08X}{} {:04X}  ", address, delaySlot ? '*' : ' ', instr);
        for (int i = 0; i < 16; i++) {
            fmt::print(" {:08X}", state.R[i]);
        }
        fmt::println("  pr = {:08X}  sr = {:08X}  gbr = {:08X}  vbr = {:08X}", state.PR, state.SR.u32, state.GBR,
                     state.VBR);
    }*/

    switch (instr >> 12u) {
    case 0x0:
        switch (instr) {
        case 0x0008: // 0000 0000 0000 1000   CLRT
            CLRT(state);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x0009: // 0000 0000 0000 1001   NOP
            NOP();
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x000B: // 0000 0000 0000 1011   RTS
            if constexpr (delaySlot) {
                // Illegal slot instruction exception
                EnterException(state, 6);
            } else {
                RTS(state);
            }
            break;
        case 0x0018: // 0000 0000 0001 1000   SETT
            SETT(state);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x0019: // 0000 0000 0001 1001   DIV0U
            DIV0U(state);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x001B: // 0000 0000 0001 1011   SLEEP
            SLEEP(state);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x0028: // 0000 0000 0010 1000   CLRMAC
            CLRMAC(state);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x002B: // 0000 0000 0010 1011   RTE
            if constexpr (delaySlot) {
                // Illegal slot instruction exception
                EnterException(state, 6);
            } else {
                RTE(state);
            }
            break;
        default:
            switch (instr & 0xFF) {
            case 0x02: // 0000 nnnn 0000 0010   STC SR, Rn
                STCSR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x03: // 0000 mmmm 0000 0011   BSRF Rm
                if constexpr (delaySlot) {
                    // Illegal slot instruction exception
                    EnterException(state, 6);
                } else {
                    BSRF(state, bit::extract<8, 11>(instr));
                }
                break;
            case 0x0A: // 0000 nnnn 0000 1010   STS MACH, Rn
                STSMACH(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x12: // 0000 nnnn 0001 0010   STC GBR, Rn
                STCGBR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x1A: // 0000 nnnn 0001 1010   STS MACL, Rn
                STSMACL(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x22: // 0000 nnnn 0010 0010   STC VBR, Rn
                STCVBR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x23: // 0000 mmmm 0010 0011   BRAF Rm
                if constexpr (delaySlot) {
                    // Illegal slot instruction exception
                    EnterException(state, 6);
                } else {
                    BRAF(state, bit::extract<8, 11>(instr));
                }
                break;
            case 0x29: // 0000 nnnn 0010 1001   MOVT Rn
                MOVT(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x2A: // 0000 nnnn 0010 1010   STS PR, Rn
                STSPR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            default:
                switch (instr & 0xF) {
                case 0x4: // 0000 nnnn mmmm 0100   MOV.B Rm, @(R0,Rn)
                    MOVBS0(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        state.PC += 2;
                    }
                    break;
                case 0x5: // 0000 nnnn mmmm 0101   MOV.W Rm, @(R0,Rn)
                    MOVWS0(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        state.PC += 2;
                    }
                    break;
                case 0x6: // 0000 nnnn mmmm 0110   MOV.L Rm, @(R0,Rn)
                    MOVLS0(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        state.PC += 2;
                    }
                    break;
                case 0x7: // 0000 nnnn mmmm 0111   MUL.L Rm, Rn
                    MULL(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        state.PC += 2;
                    }
                    break;
                case 0xC: // 0000 nnnn mmmm 1100   MOV.B @(R0,Rm), Rn
                    MOVBL0(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        state.PC += 2;
                    }
                    break;
                case 0xD: // 0000 nnnn mmmm 1101   MOV.W @(R0,Rm), Rn
                    MOVWL0(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        state.PC += 2;
                    }
                    break;
                case 0xE: // 0000 nnnn mmmm 1110   MOV.L @(R0,Rm), Rn
                    MOVLL0(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        state.PC += 2;
                    }
                    break;
                case 0xF: // 0000 nnnn mmmm 1111   MAC.L @Rm+, @Rn+
                    MACL(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        state.PC += 2;
                    }
                    break;
                default: /*dbg_println("unhandled 0000 instruction");*/ __debugbreak(); break;
                }
                break;
            }
            break;
        }
        break;
    case 0x1: // 0001 nnnn mmmm dddd   MOV.L Rm, @(disp,Rn)
        MOVLS4(state, bit::extract<4, 7>(instr), bit::extract<0, 3>(instr), bit::extract<8, 11>(instr));
        if constexpr (!delaySlot) {
            state.PC += 2;
        }
        break;
    case 0x2: {
        const uint16 rm = bit::extract<4, 7>(instr);
        const uint16 rn = bit::extract<8, 11>(instr);
        switch (instr & 0xF) {
        case 0x0: // 0010 nnnn mmmm 0000   MOV.B Rm, @Rn
            MOVBS(state, rm, rn);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x1: // 0010 nnnn mmmm 0001   MOV.W Rm, @Rn
            MOVWS(state, rm, rn);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x2: // 0010 nnnn mmmm 0010   MOV.L Rm, @Rn
            MOVLS(state, rm, rn);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;

            // There's no case 0x3

        case 0x4: // 0010 nnnn mmmm 0100   MOV.B Rm, @-Rn
            MOVBM(state, rm, rn);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x5: // 0010 nnnn mmmm 0101   MOV.W Rm, @-Rn
            MOVWM(state, rm, rn);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x6: // 0010 nnnn mmmm 0110   MOV.L Rm, @-Rn
            MOVLM(state, rm, rn);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x7: // 0010 nnnn mmmm 0110   DIV0S Rm, Rn
            DIV0S(state, rm, rn);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x8: // 0010 nnnn mmmm 1000   TST Rm, Rn
            TST(state, rm, rn);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x9: // 0010 nnnn mmmm 1001   AND Rm, Rn
            AND(state, rm, rn);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xA: // 0010 nnnn mmmm 1010   XOR Rm, Rn
            XOR(state, rm, rn);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xB: // 0010 nnnn mmmm 1011   OR Rm, Rn
            OR(state, rm, rn);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xC: // 0010 nnnn mmmm 1100   CMP/STR Rm, Rn
            CMPSTR(state, rm, rn);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xD: // 0010 nnnn mmmm 1101   XTRCT Rm, Rn
            XTRCT(state, rm, rn);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xE: // 0010 nnnn mmmm 1110   MULU.W Rm, Rn
            MULU(state, rm, rn);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xF: // 0010 nnnn mmmm 1111   MULS.W Rm, Rn
            MULS(state, rm, rn);
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        default: /*dbg_println("unhandled 0010 instruction");*/ __debugbreak(); break;
        }
        break;
    }
    case 0x3:
        switch (instr & 0xF) {
        case 0x0: // 0011 nnnn mmmm 0000   CMP/EQ Rm, Rn
            CMPEQ(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x2: // 0011 nnnn mmmm 0010   CMP/HS Rm, Rn
            CMPHS(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x3: // 0011 nnnn mmmm 0011   CMP/GE Rm, Rn
            CMPGE(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x4: // 0011 nnnn mmmm 0100   DIV1 Rm, Rn
            DIV1(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x5: // 0011 nnnn mmmm 0101   DMULU.L Rm, Rn
            DMULU(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x6: // 0011 nnnn mmmm 0110   CMP/HI Rm, Rn
            CMPHI(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x7: // 0011 nnnn mmmm 0111   CMP/GT Rm, Rn
            CMPGT(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x8: // 0011 nnnn mmmm 1000   SUB Rm, Rn
            SUB(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x9: // 0011 nnnn mmmm 1001   SUBC Rm, Rn
            SUBC(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xA: // 0011 nnnn mmmm 1010   SUBV Rm, Rn
            SUBV(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;

            // There's no case 0xB

        case 0xC: // 0011 nnnn mmmm 1100   ADD Rm, Rn
            ADD(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xD: // 0011 nnnn mmmm 1101   DMULS.L Rm, Rn
            DMULS(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xE: // 0011 nnnn mmmm 1110   ADDC Rm, Rn
            ADDC(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xF: // 0011 nnnn mmmm 1110   ADDV Rm, Rn
            ADDV(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        default: /*dbg_println("unhandled 0011 instruction");*/ __debugbreak(); break;
        }
        break;
    case 0x4:
        if ((instr & 0xF) == 0xF) {
            // 0100 nnnn mmmm 1111   MAC.W @Rm+, @Rn+
            MACW(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        } else {
            switch (instr & 0xFF) {
            case 0x00: // 0100 nnnn 0000 0000   SHLL Rn
                SHLL(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x01: // 0100 nnnn 0000 0001   SHLR Rn
                SHLR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x02: // 0100 nnnn 0000 0010   STS.L MACH, @-Rn
                STSMMACH(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x03: // 0100 nnnn 0000 0010   STC.L SR, @-Rn
                STCMSR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x04: // 0100 nnnn 0000 0100   ROTL Rn
                ROTL(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x05: // 0100 nnnn 0000 0101   ROTR Rn
                ROTR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x06: // 0100 mmmm 0000 0110   LDS.L @Rm+, MACH
                LDSMMACH(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x07: // 0100 mmmm 0000 0111   LDC.L @Rm+, SR
                LDCMSR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x08: // 0100 nnnn 0000 1000   SHLL2 Rn
                SHLL2(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x09: // 0100 nnnn 0000 1001   SHLR2 Rn
                SHLR2(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x0A: // 0100 mmmm 0000 1010   LDS Rm, MACH
                LDSMACH(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x0B: // 0110 mmmm 0000 1110   JSR @Rm
                if constexpr (delaySlot) {
                    // Illegal slot instruction exception
                    EnterException(state, 6);
                } else {
                    JSR(state, bit::extract<8, 11>(instr));
                }
                break;

                // There's no case 0x0C or 0x0D

            case 0x0E: // 0110 mmmm 0000 1110   LDC Rm, SR
                LDCSR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;

                // There's no case 0x0F

            case 0x10: // 0100 nnnn 0001 0000   DT Rn
                DT(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x11: // 0100 nnnn 0001 0001   CMP/PZ Rn
                CMPPZ(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x12: // 0100 nnnn 0001 0010   STS.L MACL, @-Rn
                STSMMACL(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x13: // 0100 nnnn 0001 0011   STC.L GBR, @-Rn
                STCMGBR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;

                // There's no case 0x14

            case 0x15: // 0100 nnnn 0001 0101   CMP/PL Rn
                CMPPL(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x16: // 0100 mmmm 0001 0110   LDS.L @Rm+, MACL
                LDSMMACL(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x17: // 0100 mmmm 0001 0111   LDC.L @Rm+, GBR
                LDCMGBR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x18: // 0100 nnnn 0001 1000   SHLL8 Rn
                SHLL8(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x19: // 0100 nnnn 0001 1001   SHLR8 Rn
                SHLR8(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x1A: // 0100 mmmm 0001 1010   LDS Rm, MACL
                LDSMACL(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x1B: // 0100 nnnn 0001 1011   TAS.B @Rn
                TAS(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;

                // There's no case 0x1C or 0x1D

            case 0x1E: // 0110 mmmm 0001 1110   LDC Rm, GBR
                LDCGBR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;

                // There's no case 0x1F

            case 0x20: // 0100 nnnn 0010 0000   SHAL Rn
                SHAL(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x21: // 0100 nnnn 0010 0001   SHAR Rn
                SHAR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x22: // 0100 nnnn 0010 0010   STS.L PR, @-Rn
                STSMPR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x23: // 0100 nnnn 0010 0011   STC.L VBR, @-Rn
                STCMVBR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x24: // 0100 nnnn 0010 0100   ROTCL Rn
                ROTCL(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x25: // 0100 nnnn 0010 0101   ROTCR Rn
                ROTCR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x26: // 0100 mmmm 0010 0110   LDS.L @Rm+, PR
                LDSMPR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x27: // 0100 mmmm 0010 0111   LDC.L @Rm+, VBR
                LDCMVBR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x28: // 0100 nnnn 0010 1000   SHLL16 Rn
                SHLL16(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x29: // 0100 nnnn 0010 1001   SHLR16 Rn
                SHLR16(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x2A: // 0100 mmmm 0010 1010   LDS Rm, PR
                LDSPR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;
            case 0x2B: // 0100 mmmm 0010 1011   JMP @Rm
                if constexpr (delaySlot) {
                    // Illegal slot instruction exception
                    EnterException(state, 6);
                } else {
                    JMP(state, bit::extract<8, 11>(instr));
                }
                break;

                // There's no case 0x2C or 0x2D

            case 0x2E: // 0110 mmmm 0010 1110   LDC Rm, VBR
                LDCVBR(state, bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    state.PC += 2;
                }
                break;

                // There's no case 0x2F..0xFF

            default: /*dbg_println("unhandled 0100 instruction");*/ __debugbreak(); break;
            }
        }
        break;
    case 0x5: // 0101 nnnn mmmm dddd   MOV.L @(disp,Rm), Rn
        MOVLL4(state, bit::extract<4, 7>(instr), bit::extract<0, 3>(instr), bit::extract<8, 11>(instr));
        if constexpr (!delaySlot) {
            state.PC += 2;
        }
        break;
    case 0x6:
        switch (instr & 0xF) {
        case 0x0: // 0110 nnnn mmmm 0000   MOV.B @Rm, Rn
            MOVBL(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x1: // 0110 nnnn mmmm 0001   MOV.W @Rm, Rn
            MOVWL(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x2: // 0110 nnnn mmmm 0010   MOV.L @Rm, Rn
            MOVLL(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x3: // 0110 nnnn mmmm 0010   MOV Rm, Rn
            MOV(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x4: // 0110 nnnn mmmm 0110   MOV.B @Rm+, Rn
            MOVBP(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x5: // 0110 nnnn mmmm 0110   MOV.W @Rm+, Rn
            MOVWP(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x6: // 0110 nnnn mmmm 0110   MOV.L @Rm+, Rn
            MOVLP(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x7: // 0110 nnnn mmmm 0111   NOT Rm, Rn
            NOT(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x8: // 0110 nnnn mmmm 1000   SWAP.B Rm, Rn
            SWAPB(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x9: // 0110 nnnn mmmm 1001   SWAP.W Rm, Rn
            SWAPW(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xA: // 0110 nnnn mmmm 1010   NEGC Rm, Rn
            NEGC(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xB: // 0110 nnnn mmmm 1011   NEG Rm, Rn
            NEG(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xC: // 0110 nnnn mmmm 1100   EXTU.B Rm, Rn
            EXTUB(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xD: // 0110 nnnn mmmm 1101   EXTU.W Rm, Rn
            EXTUW(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xE: // 0110 nnnn mmmm 1110   EXTS.B Rm, Rn
            EXTSB(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xF: // 0110 nnnn mmmm 1111   EXTS.W Rm, Rn
            EXTSW(state, bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        }
        break;
    case 0x7: // 0111 nnnn iiii iiii   ADD #imm, Rn
        ADDI(state, bit::extract<0, 7>(instr), bit::extract<8, 11>(instr));
        if constexpr (!delaySlot) {
            state.PC += 2;
        }
        break;
    case 0x8:
        switch ((instr >> 8u) & 0xF) {
        case 0x0: // 1000 0000 nnnn dddd   MOV.B R0, @(disp,Rn)
            MOVBS4(state, bit::extract<0, 3>(instr), bit::extract<4, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x1: // 1000 0001 nnnn dddd   MOV.W R0, @(disp,Rn)
            MOVWS4(state, bit::extract<0, 3>(instr), bit::extract<4, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;

            // There's no case 0x2 or 0x3

        case 0x4: // 1000 0100 mmmm dddd   MOV.B @(disp,Rm), R0
            MOVBL4(state, bit::extract<4, 7>(instr), bit::extract<0, 3>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x5: // 1000 0101 mmmm dddd   MOV.W @(disp,Rm), R0
            MOVWL4(state, bit::extract<4, 7>(instr), bit::extract<0, 3>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;

            // There's no case 0x6 or 0x7

        case 0x8: // 1000 1000 iiii iiii   CMP/EQ #imm, R0
            CMPIM(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x9: // 1000 1001 dddd dddd   BT <label>
            if constexpr (delaySlot) {
                // Illegal slot instruction exception
                EnterException(state, 6);
            } else {
                BT(state, bit::extract<0, 7>(instr));
            }
            break;

            // There's no case 0xA

        case 0xB: // 1000 1011 dddd dddd   BF <label>
            if constexpr (delaySlot) {
                // Illegal slot instruction exception
                EnterException(state, 6);
            } else {
                BF(state, bit::extract<0, 7>(instr));
            }
            break;

            // There's no case 0xC

        case 0xD: // 1000 1101 dddd dddd   BT/S <label>
            if constexpr (delaySlot) {
                // Illegal slot instruction exception
                EnterException(state, 6);
            } else {
                BTS(state, bit::extract<0, 7>(instr));
            }
            break;

            // There's no case 0xE

        case 0xF: // 1000 1111 dddd dddd   BF/S <label>
            if constexpr (delaySlot) {
                // Illegal slot instruction exception
                EnterException(state, 6);
            } else {
                BFS(state, bit::extract<0, 7>(instr));
            }
            break;
        default: /*dbg_println("unhandled 1000 instruction");*/ __debugbreak(); break;
        }
        break;
    case 0x9: // 1001 nnnn dddd dddd   MOV.W @(disp,PC), Rn
        MOVWI(state, bit::extract<0, 7>(instr), bit::extract<8, 11>(instr));
        if constexpr (!delaySlot) {
            state.PC += 2;
        }
        break;
    case 0xA: // 1011 dddd dddd dddd   BRA <label>
        if constexpr (delaySlot) {
            // Illegal slot instruction exception
            EnterException(state, 6);
        } else {
            BRA(state, bit::extract<0, 11>(instr));
        }
        break;
    case 0xB: // 1011 dddd dddd dddd   BSR <label>
        if constexpr (delaySlot) {
            // Illegal slot instruction exception
            EnterException(state, 6);
        } else {
            BSR(state, bit::extract<0, 11>(instr));
        }
        break;
    case 0xC:
        switch ((instr >> 8u) & 0xF) {
        case 0x0: // 1100 0000 dddd dddd   MOV.B R0, @(disp,GBR)
            MOVBSG(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x1: // 1100 0001 dddd dddd   MOV.W R0, @(disp,GBR)
            MOVWSG(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x2: // 1100 0010 dddd dddd   MOV.L R0, @(disp,GBR)
            MOVLSG(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x3: // 1100 0011 iiii iiii   TRAPA #imm
            if constexpr (delaySlot) {
                // Illegal slot instruction exception
                EnterException(state, 6);
            } else {
                TRAPA(state, bit::extract<0, 7>(instr));
            }
            break;
        case 0x4: // 1100 0100 dddd dddd   MOV.B @(disp,GBR), R0
            MOVBLG(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x5: // 1100 0101 dddd dddd   MOV.W @(disp,GBR), R0
            MOVWLG(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x6: // 1100 0110 dddd dddd   MOV.L @(disp,GBR), R0
            MOVLLG(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x7: // 1100 0111 dddd dddd   MOVA @(disp,PC), R0
            MOVA(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x8: // 1100 1000 iiii iiii   TST #imm, R0
            TSTI(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0x9: // 1100 1001 iiii iiii   AND #imm, R0
            ANDI(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xA: // 1100 1010 iiii iiii   XOR #imm, R0
            XORI(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xB: // 1100 1011 iiii iiii   OR #imm, R0
            ORI(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xC: // 1100 1100 iiii iiii   TST.B #imm, @(R0,GBR)
            TSTM(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xD: // 1100 1001 iiii iiii   AND #imm, @(R0,GBR)
            ANDM(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xE: // 1100 1001 iiii iiii   XOR #imm, @(R0,GBR)
            XORM(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        case 0xF: // 1100 1001 iiii iiii   OR #imm, @(R0,GBR)
            ORM(state, bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                state.PC += 2;
            }
            break;
        default: /*dbg_println("unhandled 1100 instruction");*/ __debugbreak(); break;
        }
        break;
    case 0xD: // 1101 nnnn dddd dddd   MOV.L @(disp,PC), Rn
        MOVLI(state, bit::extract<0, 7>(instr), bit::extract<8, 11>(instr));
        if constexpr (!delaySlot) {
            state.PC += 2;
        }
        break;
    case 0xE: // 1110 nnnn iiii iiii   MOV #imm, Rn
        MOVI(state, bit::extract<0, 7>(instr), bit::extract<8, 11>(instr));
        if constexpr (!delaySlot) {
            state.PC += 2;
        }
        break;

        // There's no case 0xF

    default: /*dbg_println("unhandled instruction");*/ __debugbreak(); break;
    }
}

// -----------------------------------------------------------------------------
// Instruction interpreters

FORCE_INLINE void SH2::NOP() {
    // dbg_println("nop");
}

FORCE_INLINE void SH2::SLEEP(SH2State &state) {
    // dbg_println("sleep");
    state.PC -= 2;
    // TODO: wait for exception
    __debugbreak();
}

FORCE_INLINE void SH2::MOV(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov r{}, r{}", rm, rn);
    state.R[rn] = state.R[rm];
}

FORCE_INLINE void SH2::MOVBL(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.b @r{}, r{}", rm, rn);
    state.R[rn] = bit::sign_extend<8>(MemReadByte(state, state.R[rm]));
}

FORCE_INLINE void SH2::MOVWL(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.w @r{}, r{}", rm, rn);
    state.R[rn] = bit::sign_extend<16>(MemReadWord(state, state.R[rm]));
}

FORCE_INLINE void SH2::MOVLL(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.l @r{}, r{}", rm, rn);
    state.R[rn] = MemReadLong(state, state.R[rm]);
}

FORCE_INLINE void SH2::MOVBL0(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.b @(r0,r{}), r{}", rm, rn);
    state.R[rn] = bit::sign_extend<8>(MemReadByte(state, state.R[rm] + state.R[0]));
}

FORCE_INLINE void SH2::MOVWL0(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.w @(r0,r{}), r{})", rm, rn);
    state.R[rn] = bit::sign_extend<16>(MemReadWord(state, state.R[rm] + state.R[0]));
}

FORCE_INLINE void SH2::MOVLL0(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.l @(r0,r{}), r{})", rm, rn);
    state.R[rn] = MemReadLong(state, state.R[rm] + state.R[0]);
}

FORCE_INLINE void SH2::MOVBL4(SH2State &state, uint16 rm, uint16 disp) {
    // dbg_println("mov.b @(0x{:X},r{}), r0", disp, rm);
    state.R[0] = bit::sign_extend<8>(MemReadByte(state, state.R[rm] + disp));
}

FORCE_INLINE void SH2::MOVWL4(SH2State &state, uint16 rm, uint16 disp) {
    disp <<= 1u;
    // dbg_println("mov.w @(0x{:X},r{}), r0", disp, rm);
    state.R[0] = bit::sign_extend<16>(MemReadWord(state, state.R[rm] + disp));
}

FORCE_INLINE void SH2::MOVLL4(SH2State &state, uint16 rm, uint16 disp, uint16 rn) {
    disp <<= 2u;
    // dbg_println("mov.l @(0x{:X},r{}), r{}", disp, rm, rn);
    state.R[rn] = MemReadLong(state, state.R[rm] + disp);
}

FORCE_INLINE void SH2::MOVBLG(SH2State &state, uint16 disp) {
    // dbg_println("mov.b @(0x{:X},gbr), r0", disp);
    state.R[0] = bit::sign_extend<8>(MemReadByte(state, state.GBR + disp));
}

FORCE_INLINE void SH2::MOVWLG(SH2State &state, uint16 disp) {
    disp <<= 1u;
    // dbg_println("mov.w @(0x{:X},gbr), r0", disp);
    state.R[0] = bit::sign_extend<16>(MemReadWord(state, state.GBR + disp));
}

FORCE_INLINE void SH2::MOVLLG(SH2State &state, uint16 disp) {
    disp <<= 2u;
    // dbg_println("mov.l @(0x{:X},gbr), r0", disp);
    state.R[0] = MemReadLong(state, state.GBR + disp);
}

FORCE_INLINE void SH2::MOVBM(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.b r{}, @-r{}", rm, rn);
    MemWriteByte(state, state.R[rn] - 1, state.R[rm]);
    state.R[rn] -= 1;
}

FORCE_INLINE void SH2::MOVWM(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.w r{}, @-r{}", rm, rn);
    MemWriteWord(state, state.R[rn] - 2, state.R[rm]);
    state.R[rn] -= 2;
}

FORCE_INLINE void SH2::MOVLM(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.l r{}, @-r{}", rm, rn);
    MemWriteLong(state, state.R[rn] - 4, state.R[rm]);
    state.R[rn] -= 4;
}

FORCE_INLINE void SH2::MOVBP(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.b @r{}+, r{}", rm, rn);
    state.R[rn] = bit::sign_extend<8>(MemReadByte(state, state.R[rm]));
    if (rn != rm) {
        state.R[rm] += 1;
    }
}

FORCE_INLINE void SH2::MOVWP(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.w @r{}+, r{}", rm, rn);
    state.R[rn] = bit::sign_extend<16>(MemReadWord(state, state.R[rm]));
    if (rn != rm) {
        state.R[rm] += 2;
    }
}

FORCE_INLINE void SH2::MOVLP(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.l @r{}+, r{}", rm, rn);
    state.R[rn] = MemReadLong(state, state.R[rm]);
    if (rn != rm) {
        state.R[rm] += 4;
    }
}

FORCE_INLINE void SH2::MOVBS(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.b r{}, @r{}", rm, rn);
    MemWriteByte(state, state.R[rn], state.R[rm]);
}

FORCE_INLINE void SH2::MOVWS(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.w r{}, @r{}", rm, rn);
    MemWriteWord(state, state.R[rn], state.R[rm]);
}

FORCE_INLINE void SH2::MOVLS(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.l r{}, @r{}", rm, rn);
    MemWriteLong(state, state.R[rn], state.R[rm]);
}

FORCE_INLINE void SH2::MOVBS0(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.b r{}, @(r0,r{})", rm, rn);
    MemWriteByte(state, state.R[rn] + state.R[0], state.R[rm]);
}

FORCE_INLINE void SH2::MOVWS0(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.w r{}, @(r0,r{})", rm, rn);
    MemWriteWord(state, state.R[rn] + state.R[0], state.R[rm]);
}

FORCE_INLINE void SH2::MOVLS0(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mov.l r{}, @(r0,r{})", rm, rn);
    MemWriteLong(state, state.R[rn] + state.R[0], state.R[rm]);
}

FORCE_INLINE void SH2::MOVBS4(SH2State &state, uint16 disp, uint16 rn) {
    // dbg_println("mov.b r0, @(0x{:X},r{})", disp, rn);
    MemWriteByte(state, state.R[rn] + disp, state.R[0]);
}

FORCE_INLINE void SH2::MOVWS4(SH2State &state, uint16 disp, uint16 rn) {
    disp <<= 1u;
    // dbg_println("mov.w r0, @(0x{:X},r{})", disp, rn);
    MemWriteWord(state, state.R[rn] + disp, state.R[0]);
}

FORCE_INLINE void SH2::MOVLS4(SH2State &state, uint16 rm, uint16 disp, uint16 rn) {
    disp <<= 2u;
    // dbg_println("mov.l r{}, @(0x{:X},r{})", rm, disp, rn);
    MemWriteLong(state, state.R[rn] + disp, state.R[rm]);
}

FORCE_INLINE void SH2::MOVBSG(SH2State &state, uint16 disp) {
    // dbg_println("mov.b r0, @(0x{:X},gbr)", disp);
    MemWriteByte(state, state.GBR + disp, state.R[0]);
}

FORCE_INLINE void SH2::MOVWSG(SH2State &state, uint16 disp) {
    disp <<= 1u;
    // dbg_println("mov.w r0, @(0x{:X},gbr)", disp);
    MemWriteWord(state, state.GBR + disp, state.R[0]);
}

FORCE_INLINE void SH2::MOVLSG(SH2State &state, uint16 disp) {
    disp <<= 2u;
    // dbg_println("mov.l r0, @(0x{:X},gbr)", disp);
    MemWriteLong(state, state.GBR + disp, state.R[0]);
}

FORCE_INLINE void SH2::MOVI(SH2State &state, uint16 imm, uint16 rn) {
    sint32 simm = bit::sign_extend<8>(imm);
    // dbg_println("mov #{}0x{:X}, r{}", (simm < 0 ? "-" : ""), abs(simm), rn);
    state.R[rn] = simm;
}

FORCE_INLINE void SH2::MOVWI(SH2State &state, uint16 disp, uint16 rn) {
    disp <<= 1u;
    // dbg_println("mov.w @(0x{:08X},pc), r{}", PC + 4 + disp, rn);
    state.R[rn] = bit::sign_extend<16>(MemReadWord(state, state.PC + 4 + disp));
}

FORCE_INLINE void SH2::MOVLI(SH2State &state, uint16 disp, uint16 rn) {
    disp <<= 2u;
    // dbg_println("mov.l @(0x{:08X},pc), r{}", ((PC + 4) & ~3) + disp, rn);
    state.R[rn] = MemReadLong(state, ((state.PC + 4) & ~3u) + disp);
}

FORCE_INLINE void SH2::MOVA(SH2State &state, uint16 disp) {
    disp = (disp << 2u) + 4;
    // dbg_println("mova @(0x{:X},pc), r0", (PC & ~3) + disp);
    state.R[0] = (state.PC & ~3) + disp;
}

FORCE_INLINE void SH2::MOVT(SH2State &state, uint16 rn) {
    // dbg_println("movt r{}", rn);
    state.R[rn] = state.SR.T;
}

FORCE_INLINE void SH2::CLRT(SH2State &state) {
    // dbg_println("clrt");
    state.SR.T = 0;
}

FORCE_INLINE void SH2::SETT(SH2State &state) {
    // dbg_println("sett");
    state.SR.T = 1;
}

FORCE_INLINE void SH2::EXTSB(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("exts.b r{}, r{}", rm, rn);
    state.R[rn] = bit::sign_extend<8>(state.R[rm]);
}

FORCE_INLINE void SH2::EXTSW(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("exts.w r{}, r{}", rm, rn);
    state.R[rn] = bit::sign_extend<16>(state.R[rm]);
}

FORCE_INLINE void SH2::EXTUB(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("extu.b r{}, r{}", rm, rn);
    state.R[rn] = state.R[rm] & 0xFF;
}

FORCE_INLINE void SH2::EXTUW(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("extu.w r{}, r{}", rm, rn);
    state.R[rn] = state.R[rm] & 0xFFFF;
}

FORCE_INLINE void SH2::LDCGBR(SH2State &state, uint16 rm) {
    // dbg_println("ldc r{}, gbr", rm);
    state.GBR = state.R[rm];
}

FORCE_INLINE void SH2::LDCSR(SH2State &state, uint16 rm) {
    // dbg_println("ldc r{}, sr", rm);
    state.SR.u32 = state.R[rm] & 0x000003F3;
}

FORCE_INLINE void SH2::LDCVBR(SH2State &state, uint16 rm) {
    // dbg_println("ldc r{}, vbr", rm);
    state.VBR = state.R[rm];
}

FORCE_INLINE void SH2::LDCMGBR(SH2State &state, uint16 rm) {
    // dbg_println("ldc.l @r{}+, gbr", rm);
    state.GBR = MemReadLong(state, state.R[rm]);
    state.R[rm] += 4;
}

FORCE_INLINE void SH2::LDCMSR(SH2State &state, uint16 rm) {
    // dbg_println("ldc.l @r{}+, sr", rm);
    state.SR.u32 = MemReadLong(state, state.R[rm]) & 0x000003F3;
    state.R[rm] += 4;
}

FORCE_INLINE void SH2::LDCMVBR(SH2State &state, uint16 rm) {
    // dbg_println("ldc.l @r{}+, vbr", rm);
    state.VBR = MemReadLong(state, state.R[rm]);
    state.R[rm] += 4;
}

FORCE_INLINE void SH2::LDSMACH(SH2State &state, uint16 rm) {
    // dbg_println("lds r{}, mach", rm);
    state.MAC.H = state.R[rm];
}

FORCE_INLINE void SH2::LDSMACL(SH2State &state, uint16 rm) {
    // dbg_println("lds r{}, macl", rm);
    state.MAC.L = state.R[rm];
}

FORCE_INLINE void SH2::LDSPR(SH2State &state, uint16 rm) {
    // dbg_println("lds r{}, pr", rm);
    state.PR = state.R[rm];
}

FORCE_INLINE void SH2::LDSMMACH(SH2State &state, uint16 rm) {
    // dbg_println("lds.l @r{}+, mach", rm);
    state.MAC.H = MemReadLong(state, state.R[rm]);
    state.R[rm] += 4;
}

FORCE_INLINE void SH2::LDSMMACL(SH2State &state, uint16 rm) {
    // dbg_println("lds.l @r{}+, macl", rm);
    state.MAC.L = MemReadLong(state, state.R[rm]);
    state.R[rm] += 4;
}

FORCE_INLINE void SH2::LDSMPR(SH2State &state, uint16 rm) {
    // dbg_println("lds.l @r{}+, pr", rm);
    state.PR = MemReadLong(state, state.R[rm]);
    state.R[rm] += 4;
}

FORCE_INLINE void SH2::STCGBR(SH2State &state, uint16 rn) {
    // dbg_println("stc gbr, r{}", rn);
    state.R[rn] = state.GBR;
}

FORCE_INLINE void SH2::STCSR(SH2State &state, uint16 rn) {
    // dbg_println("stc sr, r{}", rn);
    state.R[rn] = state.SR.u32;
}

FORCE_INLINE void SH2::STCVBR(SH2State &state, uint16 rn) {
    // dbg_println("stc vbr, r{}", rn);
    state.R[rn] = state.VBR;
}

FORCE_INLINE void SH2::STCMGBR(SH2State &state, uint16 rn) {
    // dbg_println("stc.l gbr, @-r{}", rn);
    state.R[rn] -= 4;
    MemWriteLong(state, state.R[rn], state.GBR);
}

FORCE_INLINE void SH2::STCMSR(SH2State &state, uint16 rn) {
    // dbg_println("stc.l sr, @-r{}", rn);
    state.R[rn] -= 4;
    MemWriteLong(state, state.R[rn], state.SR.u32);
}

FORCE_INLINE void SH2::STCMVBR(SH2State &state, uint16 rn) {
    // dbg_println("stc.l vbr, @-r{}", rn);
    state.R[rn] -= 4;
    MemWriteLong(state, state.R[rn], state.VBR);
}

FORCE_INLINE void SH2::STSMACH(SH2State &state, uint16 rn) {
    // dbg_println("sts mach, r{}", rn);
    state.R[rn] = state.MAC.H;
}

FORCE_INLINE void SH2::STSMACL(SH2State &state, uint16 rn) {
    // dbg_println("sts macl, r{}", rn);
    state.R[rn] = state.MAC.L;
}

FORCE_INLINE void SH2::STSPR(SH2State &state, uint16 rn) {
    // dbg_println("sts pr, r{}", rn);
    state.R[rn] = state.PR;
}

FORCE_INLINE void SH2::STSMMACH(SH2State &state, uint16 rn) {
    // dbg_println("sts.l mach, @-r{}", rn);
    state.R[rn] -= 4;
    MemWriteLong(state, state.R[rn], state.MAC.H);
}

FORCE_INLINE void SH2::STSMMACL(SH2State &state, uint16 rn) {
    // dbg_println("sts.l macl, @-r{}", rn);
    state.R[rn] -= 4;
    MemWriteLong(state, state.R[rn], state.MAC.L);
}

FORCE_INLINE void SH2::STSMPR(SH2State &state, uint16 rn) {
    // dbg_println("sts.l pr, @-r{}", rn);
    state.R[rn] -= 4;
    MemWriteLong(state, state.R[rn], state.PR);
}

FORCE_INLINE void SH2::SWAPB(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("swap.b r{}, r{}", rm, rn);

    const uint32 tmp0 = state.R[rm] & 0xFFFF0000;
    const uint32 tmp1 = (state.R[rm] & 0xFF) << 8u;
    state.R[rn] = ((state.R[rm] >> 8u) & 0xFF) | tmp1 | tmp0;
}

FORCE_INLINE void SH2::SWAPW(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("swap.w r{}, r{}", rm, rn);

    const uint32 tmp = state.R[rm] >> 16u;
    state.R[rn] = (state.R[rm] << 16u) | tmp;
}

FORCE_INLINE void SH2::XTRCT(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("xtrct r{}, r{}", rm, rn);
    state.R[rn] = (state.R[rn] >> 16u) | (state.R[rm] << 16u);
}

FORCE_INLINE void SH2::ADD(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("add r{}, r{}", rm, rn);
    state.R[rn] += state.R[rm];
}

FORCE_INLINE void SH2::ADDI(SH2State &state, uint16 imm, uint16 rn) {
    const sint32 simm = bit::sign_extend<8>(imm);
    // dbg_println("add #{}0x{:X}, r{}", (simm < 0 ? "-" : ""), abs(simm), rn);
    state.R[rn] += simm;
}

FORCE_INLINE void SH2::ADDC(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("addc r{}, r{}", rm, rn);
    const uint32 tmp1 = state.R[rn] + state.R[rm];
    const uint32 tmp0 = state.R[rn];
    state.R[rn] = tmp1 + state.SR.T;
    state.SR.T = (tmp0 > tmp1) || (tmp1 > state.R[rn]);
}

FORCE_INLINE void SH2::ADDV(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("addv r{}, r{}", rm, rn);

    const bool dst = static_cast<sint32>(state.R[rn]) < 0;
    const bool src = static_cast<sint32>(state.R[rm]) < 0;

    state.R[rn] += state.R[rm];

    bool ans = static_cast<sint32>(state.R[rn]) < 0;
    ans ^= dst;
    state.SR.T = (src == dst) & ans;
}

FORCE_INLINE void SH2::AND(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("and r{}, r{}", rm, rn);
    state.R[rn] &= state.R[rm];
}

FORCE_INLINE void SH2::ANDI(SH2State &state, uint16 imm) {
    // dbg_println("and #0x{:X}, r0", imm);
    state.R[0] &= imm;
}

FORCE_INLINE void SH2::ANDM(SH2State &state, uint16 imm) {
    // dbg_println("and.b #0x{:X}, @(r0,gbr)", imm);
    uint8 tmp = MemReadByte(state, state.GBR + state.R[0]);
    tmp &= imm;
    MemWriteByte(state, state.GBR + state.R[0], tmp);
}

FORCE_INLINE void SH2::NEG(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("neg r{}, r{}", rm, rn);
    state.R[rn] = -state.R[rm];
}

FORCE_INLINE void SH2::NEGC(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("negc r{}, r{}", rm, rn);
    const uint32 tmp = -state.R[rm];
    state.R[rn] = tmp - state.SR.T;
    state.SR.T = (0 < tmp) || (tmp < state.R[rn]);
}

FORCE_INLINE void SH2::NOT(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("not r{}, r{}", rm, rn);
    state.R[rn] = ~state.R[rm];
}

FORCE_INLINE void SH2::OR(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("or r{}, r{}", rm, rn);
    state.R[rn] |= state.R[rm];
}

FORCE_INLINE void SH2::ORI(SH2State &state, uint16 imm) {
    // dbg_println("or #0x{:X}, r0", imm);
    state.R[0] |= imm;
}

FORCE_INLINE void SH2::ORM(SH2State &state, uint16 imm) {
    // dbg_println("or.b #0x{:X}, @(r0,gbr)", imm);
    uint8 tmp = MemReadByte(state, state.GBR + state.R[0]);
    tmp |= imm;
    MemWriteByte(state, state.GBR + state.R[0], tmp);
}

FORCE_INLINE void SH2::ROTCL(SH2State &state, uint16 rn) {
    // dbg_println("rotcl r{}", rn);
    const bool tmp = state.R[rn] >> 31u;
    state.R[rn] = (state.R[rn] << 1u) | state.SR.T;
    state.SR.T = tmp;
}

FORCE_INLINE void SH2::ROTCR(SH2State &state, uint16 rn) {
    // dbg_println("rotcr r{}", rn);
    const bool tmp = state.R[rn] & 1u;
    state.R[rn] = (state.R[rn] >> 1u) | (state.SR.T << 31u);
    state.SR.T = tmp;
}

FORCE_INLINE void SH2::ROTL(SH2State &state, uint16 rn) {
    // dbg_println("rotl r{}", rn);
    state.SR.T = state.R[rn] >> 31u;
    state.R[rn] = (state.R[rn] << 1u) | state.SR.T;
}

FORCE_INLINE void SH2::ROTR(SH2State &state, uint16 rn) {
    // dbg_println("rotr r{}", rn);
    state.SR.T = state.R[rn] & 1u;
    state.R[rn] = (state.R[rn] >> 1u) | (state.SR.T << 31u);
}

FORCE_INLINE void SH2::SHAL(SH2State &state, uint16 rn) {
    // dbg_println("shal r{}", rn);
    state.SR.T = state.R[rn] >> 31u;
    state.R[rn] <<= 1u;
}

FORCE_INLINE void SH2::SHAR(SH2State &state, uint16 rn) {
    // dbg_println("shar r{}", rn);
    state.SR.T = state.R[rn] & 1u;
    state.R[rn] = static_cast<sint32>(state.R[rn]) >> 1;
}

FORCE_INLINE void SH2::SHLL(SH2State &state, uint16 rn) {
    // dbg_println("shll r{}", rn);
    state.SR.T = state.R[rn] >> 31u;
    state.R[rn] <<= 1u;
}

FORCE_INLINE void SH2::SHLL2(SH2State &state, uint16 rn) {
    // dbg_println("shll2 r{}", rn);
    state.R[rn] <<= 2u;
}

FORCE_INLINE void SH2::SHLL8(SH2State &state, uint16 rn) {
    // dbg_println("shll8 r{}", rn);
    state.R[rn] <<= 8u;
}

FORCE_INLINE void SH2::SHLL16(SH2State &state, uint16 rn) {
    // dbg_println("shll16 r{}", rn);
    state.R[rn] <<= 16u;
}

FORCE_INLINE void SH2::SHLR(SH2State &state, uint16 rn) {
    // dbg_println("shlr r{}", rn);
    state.SR.T = state.R[rn] & 1u;
    state.R[rn] >>= 1u;
}

FORCE_INLINE void SH2::SHLR2(SH2State &state, uint16 rn) {
    // dbg_println("shlr2 r{}", rn);
    state.R[rn] >>= 2u;
}

FORCE_INLINE void SH2::SHLR8(SH2State &state, uint16 rn) {
    // dbg_println("shlr8 r{}", rn);
    state.R[rn] >>= 8u;
}

FORCE_INLINE void SH2::SHLR16(SH2State &state, uint16 rn) {
    // dbg_println("shlr16 r{}", rn);
    state.R[rn] >>= 16u;
}

FORCE_INLINE void SH2::SUB(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("sub r{}, r{}", rm, rn);
    state.R[rn] -= state.R[rm];
}

FORCE_INLINE void SH2::SUBC(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("subc r{}, r{}", rm, rn);
    const uint32 tmp1 = state.R[rn] - state.R[rm];
    const uint32 tmp0 = state.R[rn];
    state.R[rn] = tmp1 - state.SR.T;
    state.SR.T = (tmp0 < tmp1) || (tmp1 < state.R[rn]);
}

FORCE_INLINE void SH2::SUBV(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("subv r{}, r{}", rm, rn);

    const bool dst = static_cast<sint32>(state.R[rn]) < 0;
    const bool src = static_cast<sint32>(state.R[rm]) < 0;

    state.R[rn] -= state.R[rm];

    bool ans = static_cast<sint32>(state.R[rn]) < 0;
    ans ^= dst;
    state.SR.T = (src != dst) & ans;
}

FORCE_INLINE void SH2::XOR(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("xor r{}, r{}", rm, rn);
    state.R[rn] ^= state.R[rm];
}

FORCE_INLINE void SH2::XORI(SH2State &state, uint16 imm) {
    // dbg_println("xor #0x{:X}, r0", imm);
    state.R[0] ^= imm;
}

FORCE_INLINE void SH2::XORM(SH2State &state, uint16 imm) {
    // dbg_println("xor.b #0x{:X}, @(r0,gbr)", imm);
    uint8 tmp = MemReadByte(state, state.GBR + state.R[0]);
    tmp ^= imm;
    MemWriteByte(state, state.GBR + state.R[0], tmp);
}

FORCE_INLINE void SH2::DT(SH2State &state, uint16 rn) {
    // dbg_println("dt r{}", rn);
    state.R[rn]--;
    state.SR.T = state.R[rn] == 0;
}

FORCE_INLINE void SH2::CLRMAC(SH2State &state) {
    // dbg_println("clrmac");
    state.MAC.u64 = 0;
}

FORCE_INLINE void SH2::MACW(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mac.w @r{}+, @r{}+)", rm, rn);

    const sint32 op2 = bit::sign_extend<16, sint32>(MemReadWord(state, state.R[rn]));
    state.R[rn] += 2;
    const sint32 op1 = bit::sign_extend<16, sint32>(MemReadWord(state, state.R[rm]));
    state.R[rm] += 2;

    const sint32 mul = op1 * op2;
    if (state.SR.S) {
        const sint64 result = static_cast<sint64>(static_cast<sint32>(state.MAC.L)) + mul;
        const sint32 saturatedResult = std::clamp<sint64>(result, 0xFFFFFFFF'80000000, 0x00000000'7FFFFFFF);
        if (result == saturatedResult) {
            state.MAC.L = result;
        } else {
            state.MAC.L = saturatedResult;
            state.MAC.H = 1;
        }
    } else {
        state.MAC.u64 += mul;
    }
}

FORCE_INLINE void SH2::MACL(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mac.l @r{}+, @r{}+)", rm, rn);

    const sint64 op2 = static_cast<sint64>(static_cast<sint32>(MemReadLong(state, state.R[rn])));
    state.R[rn] += 4;
    const sint64 op1 = static_cast<sint64>(static_cast<sint32>(MemReadLong(state, state.R[rm])));
    state.R[rm] += 4;

    const sint64 mul = op1 * op2;
    sint64 result = mul + state.MAC.u64;
    if (state.SR.S) {
        if (bit::extract<63>((result ^ state.MAC.u64) & (result ^ mul))) {
            if (bit::extract<63>(state.MAC.u64)) {
                result = 0xFFFF8000'00000000;
            } else {
                result = 0x00007FFF'FFFFFFFF;
            }
        } else {
            result = std::clamp<sint64>(result, 0xFFFF8000'00000000, 0x00007FFF'FFFFFFFF);
        }
    }
    state.MAC.u64 = result;
}

FORCE_INLINE void SH2::MULL(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mul.l r{}, r{}", rm, rn);
    state.MAC.L = state.R[rm] * state.R[rn];
}

FORCE_INLINE void SH2::MULS(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("muls.w r{}, r{}", rm, rn);
    state.MAC.L = bit::sign_extend<16>(state.R[rm]) * bit::sign_extend<16>(state.R[rn]);
}

FORCE_INLINE void SH2::MULU(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("mulu.w r{}, r{}", rm, rn);
    auto cast = [](uint32 val) { return static_cast<uint32>(static_cast<uint16>(val)); };
    state.MAC.L = cast(state.R[rm]) * cast(state.R[rn]);
}

FORCE_INLINE void SH2::DMULS(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("dmuls.l r{}, r{}", rm, rn);
    auto cast = [](uint32 val) { return static_cast<sint64>(static_cast<sint32>(val)); };
    state.MAC.u64 = cast(state.R[rm]) * cast(state.R[rn]);
}

FORCE_INLINE void SH2::DMULU(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("dmulu.l r{}, r{}", rm, rn);
    state.MAC.u64 = static_cast<uint64>(state.R[rm]) * static_cast<uint64>(state.R[rn]);
}

FORCE_INLINE void SH2::DIV0S(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("div0s r{}, r{}", rm, rn);
    state.SR.M = static_cast<sint32>(state.R[rm]) < 0;
    state.SR.Q = static_cast<sint32>(state.R[rn]) < 0;
    state.SR.T = state.SR.M != state.SR.Q;
}

FORCE_INLINE void SH2::DIV0U(SH2State &state) {
    // dbg_println("div0u");
    state.SR.M = 0;
    state.SR.Q = 0;
    state.SR.T = 0;
}

FORCE_INLINE void SH2::DIV1(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("div1 r{}, r{}", rm, rn);

    const bool oldQ = state.SR.Q;
    state.SR.Q = static_cast<sint32>(state.R[rn]) < 0;
    state.R[rn] = (state.R[rn] << 1u) | state.SR.T;

    const uint32 prevVal = state.R[rn];
    if (oldQ == state.SR.M) {
        state.R[rn] -= state.R[rm];
    } else {
        state.R[rn] += state.R[rm];
    }

    if (oldQ) {
        if (state.SR.M) {
            state.SR.Q ^= state.R[rn] <= prevVal;
        } else {
            state.SR.Q ^= state.R[rn] < prevVal;
        }
    } else {
        if (state.SR.M) {
            state.SR.Q ^= state.R[rn] >= prevVal;
        } else {
            state.SR.Q ^= state.R[rn] > prevVal;
        }
    }

    state.SR.T = state.SR.Q == state.SR.M;
}

FORCE_INLINE void SH2::CMPIM(SH2State &state, uint16 imm) {
    const sint32 simm = bit::sign_extend<8>(imm);
    // dbg_println("cmp/eq #{}0x{:X}, r0", (simm < 0 ? "-" : ""), abs(simm));
    state.SR.T = state.R[0] == simm;
}

FORCE_INLINE void SH2::CMPEQ(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("cmp/eq r{}, r{}", rm, rn);
    state.SR.T = state.R[rn] == state.R[rm];
}

FORCE_INLINE void SH2::CMPGE(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("cmp/ge r{}, r{}", rm, rn);
    state.SR.T = static_cast<sint32>(state.R[rn]) >= static_cast<sint32>(state.R[rm]);
}

FORCE_INLINE void SH2::CMPGT(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("cmp/gt r{}, r{}", rm, rn);
    state.SR.T = static_cast<sint32>(state.R[rn]) > static_cast<sint32>(state.R[rm]);
}

FORCE_INLINE void SH2::CMPHI(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("cmp/hi r{}, r{}", rm, rn);
    state.SR.T = state.R[rn] > state.R[rm];
}

FORCE_INLINE void SH2::CMPHS(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("cmp/hs r{}, r{}", rm, rn);
    state.SR.T = state.R[rn] >= state.R[rm];
}

FORCE_INLINE void SH2::CMPPL(SH2State &state, uint16 rn) {
    // dbg_println("cmp/pl r{}", rn);
    state.SR.T = static_cast<sint32>(state.R[rn]) > 0;
}

FORCE_INLINE void SH2::CMPPZ(SH2State &state, uint16 rn) {
    // dbg_println("cmp/pz r{}", rn);
    state.SR.T = static_cast<sint32>(state.R[rn]) >= 0;
}

FORCE_INLINE void SH2::CMPSTR(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("cmp/str r{}, r{}", rm, rn);
    const uint32 tmp = state.R[rm] & state.R[rn];
    const uint8 hh = tmp >> 24u;
    const uint8 hl = tmp >> 16u;
    const uint8 lh = tmp >> 8u;
    const uint8 ll = tmp >> 0u;
    state.SR.T = !(hh && hl && lh && ll);
}

FORCE_INLINE void SH2::TAS(SH2State &state, uint16 rn) {
    // dbg_println("tas.b @r{}", rn);
    // dbg_println("WARNING: bus lock not implemented!");

    // TODO: enable bus lock on this read
    const uint8 tmp = MemReadByte(state, state.R[rn]);
    state.SR.T = tmp == 0;
    // TODO: disable bus lock on this write
    MemWriteByte(state, state.R[rn], tmp | 0x80);
}

FORCE_INLINE void SH2::TST(SH2State &state, uint16 rm, uint16 rn) {
    // dbg_println("tst r{}, r{}", rm, rn);
    state.SR.T = (state.R[rn] & state.R[rm]) == 0;
}

FORCE_INLINE void SH2::TSTI(SH2State &state, uint16 imm) {
    // dbg_println("tst #0x{:X}, r0", imm);
    state.SR.T = (state.R[0] & imm) == 0;
}

FORCE_INLINE void SH2::TSTM(SH2State &state, uint16 imm) {
    // dbg_println("tst.b #0x{:X}, @(r0,gbr)", imm);
    const uint8 tmp = MemReadByte(state, state.GBR + state.R[0]);
    state.SR.T = (tmp & imm) == 0;
}

FORCE_INLINE void SH2::BF(SH2State &state, uint16 disp) {
    const sint32 sdisp = (bit::sign_extend<8>(disp) << 1) + 4;
    // dbg_println("bf 0x{:08X}", PC + sdisp);

    if (!state.SR.T) {
        state.PC += sdisp;
    } else {
        state.PC += 2;
    }
}

FORCE_INLINE void SH2::BFS(SH2State &state, uint16 disp) {
    const sint32 sdisp = (bit::sign_extend<8>(disp) << 1) + 4;
    // dbg_println("bf/s 0x{:08X}", PC + sdisp);

    if (!state.SR.T) {
        const uint32 delaySlot = state.PC + 2;
        state.PC += sdisp;
        Execute<true>(state, delaySlot);
    } else {
        state.PC += 2;
    }
}

FORCE_INLINE void SH2::BT(SH2State &state, uint16 disp) {
    const sint32 sdisp = (bit::sign_extend<8>(disp) << 1) + 4;
    // dbg_println("bt 0x{:08X}", PC + sdisp);

    if (state.SR.T) {
        state.PC += sdisp;
    } else {
        state.PC += 2;
    }
}

FORCE_INLINE void SH2::BTS(SH2State &state, uint16 disp) {
    const sint32 sdisp = (bit::sign_extend<8>(disp) << 1) + 4;
    // dbg_println("bt/s 0x{:08X}", PC + sdisp);

    if (state.SR.T) {
        const uint32 delaySlot = state.PC + 2;
        state.PC += sdisp;
        Execute<true>(state, delaySlot);
    } else {
        state.PC += 2;
    }
}

FORCE_INLINE void SH2::BRA(SH2State &state, uint16 disp) {
    const sint32 sdisp = (bit::sign_extend<12>(disp) << 1) + 4;
    // dbg_println("bra 0x{:08X}", PC + sdisp);
    const uint32 delaySlot = state.PC + 2;
    state.PC += sdisp;
    Execute<true>(state, delaySlot);
}

FORCE_INLINE void SH2::BRAF(SH2State &state, uint16 rm) {
    // dbg_println("braf r{}", rm);
    const uint32 delaySlot = state.PC + 2;
    state.PC += state.R[rm] + 4;
    Execute<true>(state, delaySlot);
}

FORCE_INLINE void SH2::BSR(SH2State &state, uint16 disp) {
    const sint32 sdisp = (bit::sign_extend<12>(disp) << 1) + 4;
    // dbg_println("bsr 0x{:08X}", PC + sdisp);

    state.PR = state.PC;
    state.PC += sdisp;
    Execute<true>(state, state.PR + 2);
}

FORCE_INLINE void SH2::BSRF(SH2State &state, uint16 rm) {
    // dbg_println("bsrf r{}", rm);
    state.PR = state.PC;
    state.PC += state.R[rm] + 4;
    Execute<true>(state, state.PR + 2);
}

FORCE_INLINE void SH2::JMP(SH2State &state, uint16 rm) {
    // dbg_println("jmp @r{}", rm);
    const uint32 delaySlot = state.PC + 2;
    state.PC = state.R[rm];
    Execute<true>(state, delaySlot);
}

FORCE_INLINE void SH2::JSR(SH2State &state, uint16 rm) {
    // dbg_println("jsr @r{}", rm);
    state.PR = state.PC;
    state.PC = state.R[rm];
    Execute<true>(state, state.PR + 2);
}

FORCE_INLINE void SH2::TRAPA(SH2State &state, uint16 imm) {
    // dbg_println("trapa #0x{:X}", imm);
    state.R[15] -= 4;
    MemWriteLong(state, state.R[15], state.SR.u32);
    state.R[15] -= 4;
    MemWriteLong(state, state.R[15], state.PC - 2);
    state.PC = MemReadLong(state, state.VBR + (imm << 2u));
}

FORCE_INLINE void SH2::RTE(SH2State &state) {
    // dbg_println("rte");
    const uint32 delaySlot = state.PC + 2;
    state.PC = MemReadLong(state, state.R[15]) + 4;
    state.R[15] += 4;
    state.SR.u32 = MemReadLong(state, state.R[15]) & 0x000003F3;
    state.R[15] += 4;
    Execute<true>(state, delaySlot);
}

FORCE_INLINE void SH2::RTS(SH2State &state) {
    // dbg_println("rts");
    const uint32 delaySlot = state.PC + 2;
    state.PC = state.PR + 4;
    Execute<true>(state, delaySlot);
}

} // namespace satemu::sh2
