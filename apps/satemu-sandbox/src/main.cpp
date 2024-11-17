#include <satemu/satemu.hpp>

#include <fmt/format.h>

#include <array>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

using sint8 = int8_t;
using sint16 = int16_t;
using sint32 = int32_t;
using sint64 = int64_t;

constexpr size_t operator""_KiB(size_t sz) {
    return sz * 1024;
}

constexpr size_t kIPLSize = 512_KiB;
constexpr size_t kWRAMLowSize = 1024_KiB;
constexpr size_t kWRAMHighSize = 1024_KiB;

template <unsigned B, std::integral T>
constexpr auto SignExtend(T x) {
    using ST = std::make_signed_t<T>;
    struct {
        ST x : B;
    } s{static_cast<ST>(x)};
    return s.x;
}

class SH2Bus {
public:
    SH2Bus() {
        m_IPL.fill(0);
        Reset(true);
    }

    void Reset(bool hard) {
        m_WRAMLow.fill(0);
        m_WRAMHigh.fill(0);
    }

    void LoadIPL(std::span<uint8, kIPLSize> ipl) {
        std::copy(ipl.begin(), ipl.end(), m_IPL.begin());
    }

    uint8 ReadByte(uint32 baseAddress) {
        // const uint32 region = address >> 29;
        uint32 address = baseAddress & 0x7FFFFFF;

        if (baseAddress >= 0xFFFFFE00) {
            address &= 0x1FF;
            return OnChipRegReadByte(address);
        } else if (address <= 0x000FFFFF) {
            address &= 0x7FFFF;
            return m_IPL[address];
        } else if (address - 0x100000 <= 0x0007FFFF) {
            address &= 0x7F;
            return SMPCRead(address);
        } else if (address - 0x200000 <= 0x000FFFFF) {
            address &= 0xFFFFF;
            return m_WRAMLow[address];
        } else if (address - 0x6000000 <= 0x01FFFFFF) {
            address &= 0xFFFFF;
            return m_WRAMHigh[address];
        } else {
            fmt::println("unhandled SH2 bus 8-bit read from {:08X}", baseAddress);
            return 0;
        }
    }

    uint16 ReadWord(uint32 baseAddress) {
        // const uint32 region = address >> 29;
        uint32 address = baseAddress & 0x7FFFFFE;

        if (baseAddress >= 0xFFFFFE00) {
            address &= 0x1FF;
            return OnChipRegReadWord(address);
        } else if (address <= 0x000FFFFF) {
            address &= 0x7FFFF;
            return (m_IPL[address + 0] << 8u) | m_IPL[address + 1];
        } else if (address - 0x200000 <= 0x000FFFFF) {
            address &= 0xFFFFF;
            return (m_WRAMLow[address + 0] << 8u) | m_WRAMLow[address + 1];
        } else if (address - 0x6000000 <= 0x01FFFFFF) {
            address &= 0xFFFFF;
            return (m_WRAMHigh[address + 0] << 8u) | m_WRAMHigh[address + 1];
        } else {
            fmt::println("unhandled SH2 bus 16-bit read from {:08X}", baseAddress);
            return 0;
        }
    }

    uint32 ReadLong(uint32 baseAddress) {
        // const uint32 region = address >> 29;
        uint32 address = baseAddress & 0x7FFFFFC;

        if (baseAddress >= 0xFFFFFE00) {
            address &= 0x1FF;
            return OnChipRegReadLong(address);
        } else if (address <= 0x000FFFFF) {
            address &= 0x7FFFF;
            return (m_IPL[address + 0] << 24u) | (m_IPL[address + 1] << 16u) | (m_IPL[address + 2] << 8u) |
                   m_IPL[address + 3];
        } else if (address - 0x200000 <= 0x000FFFFF) {
            address &= 0xFFFFF;
            return (m_WRAMLow[address + 0] << 24u) | (m_WRAMLow[address + 1] << 16u) | (m_WRAMLow[address + 2] << 8u) |
                   m_WRAMLow[address + 3];
        } else if (address - 0x6000000 <= 0x01FFFFFF) {
            address &= 0xFFFFF;
            return (m_WRAMHigh[address + 0] << 24u) | (m_WRAMHigh[address + 1] << 16u) |
                   (m_WRAMHigh[address + 2] << 8u) | m_WRAMHigh[address + 3];
        } else {
            fmt::println("unhandled SH2 bus 32-bit read from {:08X}", baseAddress);
            return 0;
        }
    }

    void WriteByte(uint32 baseAddress, uint8 value) {
        // const uint32 region = address >> 29;
        uint32 address = baseAddress & 0x7FFFFFF;

        if (baseAddress >= 0xFFFFFE00) {
            address &= 0x1FF;
            OnChipRegWriteByte(address, value);
        } else if (address - 0x100000 <= 0x0007FFFF && (address & 1)) {
            address &= 0x7F;
            SMPCWrite(address, value);
        } else if (address - 0x200000 <= 0x000FFFFF) {
            address &= 0xFFFFF;
            m_WRAMLow[address] = value;
        } else if (address - 0x6000000 <= 0x01FFFFFF) {
            address &= 0xFFFFF;
            m_WRAMHigh[address] = value;
        } else {
            fmt::println("unhandled SH2 bus 8-bit write to {:08X} = {:02X}", baseAddress, value);
        }
    }

    void WriteWord(uint32 baseAddress, uint16 value) {
        // const uint32 region = address >> 29;
        uint32 address = baseAddress & 0x7FFFFFE;

        if (baseAddress >= 0xFFFFFE00) {
            address &= 0x1FF;
            OnChipRegWriteWord(address, value);
        } else if (address - 0x100000 <= 0x0007FFFF) {
            address &= 0x7F;
            SMPCWrite(address | 1, value);
        } else if (address - 0x200000 <= 0x000FFFFF) {
            address &= 0xFFFFF;
            m_WRAMLow[address + 0] = value >> 8u;
            m_WRAMLow[address + 1] = value >> 0u;
        } else if (address - 0x6000000 <= 0x01FFFFFF) {
            address &= 0xFFFFF;
            m_WRAMHigh[address + 0] = value >> 8u;
            m_WRAMHigh[address + 1] = value >> 0u;
        } else {
            fmt::println("unhandled SH2 bus 16-bit write to {:08X} = {:04X}", baseAddress, value);
        }
    }

    void WriteLong(uint32 baseAddress, uint32 value) {
        // const uint32 region = address >> 29;
        uint32 address = baseAddress & 0x7FFFFFC;

        if (baseAddress >= 0xFFFFFE00) {
            address &= 0x1FF;
            OnChipRegWriteLong(address, value);
        } else if (address - 0x200000 <= 0x000FFFFF) {
            address &= 0xFFFFF;
            m_WRAMLow[address + 0] = value >> 24u;
            m_WRAMLow[address + 1] = value >> 16u;
            m_WRAMLow[address + 2] = value >> 8u;
            m_WRAMLow[address + 3] = value >> 0u;
        } else if (address - 0x6000000 <= 0x01FFFFFF) {
            address &= 0xFFFFF;
            m_WRAMHigh[address + 0] = value >> 24u;
            m_WRAMHigh[address + 1] = value >> 16u;
            m_WRAMHigh[address + 2] = value >> 8u;
            m_WRAMHigh[address + 3] = value >> 0u;
        } else {
            fmt::println("unhandled SH2 bus 32-bit write to {:08X} = {:08X}", baseAddress, value);
        }
    }

private:
    std::array<uint8, kIPLSize> m_IPL; // aka BIOS ROM
    std::array<uint8, kWRAMLowSize> m_WRAMLow;
    std::array<uint8, kWRAMHighSize> m_WRAMHigh;

    uint8 SMPCRead(uint32 address) {
        fmt::println("unhandled SMPC read from {:02X}", address);
        return 0;
    }

    void SMPCWrite(uint32 address, uint8 value) {
        fmt::println("unhandled SMPC write to {:02X} = {:02X}", address, value);
    }

    uint8 OnChipRegReadByte(uint32 address) {
        fmt::println("unhandled on-chip register 8-bit read from {:02X}", address);
        return 0;
    }

    uint16 OnChipRegReadWord(uint32 address) {
        fmt::println("unhandled on-chip register 16-bit read from {:02X}", address);
        return 0;
    }

    uint32 OnChipRegReadLong(uint32 address) {
        fmt::println("unhandled on-chip register 32-bit read from {:02X}", address);
        return 0;
    }

    void OnChipRegWriteByte(uint32 address, uint8 value) {
        fmt::println("unhandled on-chip register 8-bit write to {:02X} = {:02X}", address, value);
    }

    void OnChipRegWriteWord(uint32 address, uint16 value) {
        fmt::println("unhandled on-chip register 16-bit write to {:02X} = {:04X}", address, value);
    }

    void OnChipRegWriteLong(uint32 address, uint32 value) {
        fmt::println("unhandled on-chip register 32-bit write to {:02X} = {:08X}", address, value);
    }

    // On-chip registers  (from SH7604 manual)
    //
    // --- SCI module ---
    //
    // addr r/w  access   init  code    name
    // 000  R/W  8        00    SMR     Serial Mode Register
    //   b  r/w  code  description
    //   7  R/W  C/nA  Communication Mode (0=async, 1=clocked sync)
    //   6  R/W  CHR   Character Length (0=8-bit, 1=7-bit)
    //   5  R/W  PE    Parity Enable (0=disable, 1=enable)
    //   4  R/W  O/nE  Parity Mode (0=even, 1=odd)
    //   3  R/W  STOP  Stop Bit Length (0=one, 1=two)
    //   2  R/W  MP    Multiprocessor Mode (0=disabled, 1=enabled)
    //   1  R/W  CKS1  Clock Select bit 1  (00=phi/4,  01=phi/16,
    //   0  R/W  CKS0  Clock Select bit 0   10=phi/64, 11=phi/256)
    //
    // 001  R/W  8        FF    BRR     Bit Rate Register
    // 002  R/W  8        00    SCR     Serial Control Register
    // 003  R/W  8        FF    TDR     Transmit Data Register
    // 004  R/W* 8        84    SSR     Serial Status Register
    //   * Can only write a 0 to clear the flags
    //
    // 005  R    8        00    RDR     Receive Data Register
    //
    // --- FRT module ---
    //
    // 010  ?    8        ??    TIER    ???
    // 011  ?    8        ??    FTCSR   ???
    // 012  ?    8        ??    FRC     ???
    // 013  ?    16?      ??    OCRA/B  ???
    // 015  ?    16?      ??    TCR     ???
    // 017  ?    8        ??    TOCR    ???
    // 018  ?    16?      ??    FICR    ???
    //
    // --- INTC module ---
    //
    // 060  ?    16?      ??    IPRB    ???
    // 062  ?    16?      ??    VCRA    ???
    // 064  ?    16?      ??    VCRB    ???
    // 066  ?    16?      ??    VCRC    ???
    // 068  ?    16?      ??    VCRD    ???
    //
    // --- DMAC module ---
    //
    // 071  ?    8        ??    DRCR0   ???
    // 072  ?    8        ??    DRCR1   ???
    //
    // --- WDT module ---
    //
    // 080  R    8        ??    WTCSR   ???
    // 081  R    8        ??    WTCNT   ???
    // 083  R    8        ??    RSTCSR  ???
    //
    // 080  W    8        ??    WTCSR   ???
    // 080  W    8        ??    WTCNT   ???
    // 082  W    8        ??    RSTCSR  ???
    //
    // --- Power-down module ---
    //
    // 091  ?    8        ??    SBYCR   ???
    //
    // --- Cache module ---
    //
    // 092  R/W  8        00    CCR     Cache Control Register
    //   b  cd  r/w    description
    //   7  W1  R/W    Way Specification (MSB)
    //   6  W0  R/W    Way Specification (LSB)
    //   5  -   R      [reserved]
    //   4  CP  R/W    Cache Purge (0=normal, 1=purge)
    //   3  TW  R/W    Two-Way Mode (0=four-way, 1=two-way)
    //   2  OD  R/W    Data Replacement Disable (0=disabled, 1=data cache not updated on miss)
    //   1  ID  R/W    Instruction Replacement Disabled (same as above, but for code cache)
    //   0  CE  R/W    Cache Enable (0=disable, 1=enable)
    //
    //   Memory address space is partitioned on bits 31-29:
    //   31 29  Partition                       Cache operation
    //    000   Cache area                      Used when CCR.CE=1
    //    001   Cache-through area              Bypassed
    //    010   Associative purge area          Purge accessed cache lines (reads return 0x2312)
    //    011   Address array read/write area   Cache addresses acessed directly (1 KiB, mirrored)
    //    100   [ same as 110 ]
    //    101   [ same as 001 ]
    //    110   Data array read/write area      Cache data acessed directly (4 KiB, mirrored)
    //    111   I/O area                        Bypassed
    //
    // --- INTC module (part 2) ---
    //
    // 0E0  ?    16?      ??    ICR     ???
    // 0E2  ?    16?      ??    IPRA    ???
    // 0E4  ?    16?      ??    VCRWDT  ???
    //
    // --- DIVU module ---
    //
    // 100  ?    32?      ??    DVSR    ???
    // 104  ?    32?      ??    DVDNT   ???
    // 108  ?    32?      ??    DVCR    ???
    // 10C  ?    32?      ??    VCRDIV  ???
    // 110  ?    32?      ??    DVDNTH  ???
    // 114  ?    32?      ??    DVDNTL  ???
    //
    // --- UBC module (channel A) ---
    //
    // 140  ?    16?      ??    BARAH   ???
    // 142  ?    16?      ??    BARAL   ???
    // 144  ?    16?      ??    BAMRAH  ???
    // 146  ?    16?      ??    BAMRAL  ???
    // 148  ?    16?      ??    BBRA    ???
    //
    // --- UBC module (channel B) ---
    //
    // 160  ?    16?      ??    BARBH   ???
    // 162  ?    16?      ??    BARBL   ???
    // 164  ?    16?      ??    BAMRBH  ???
    // 166  ?    16?      ??    BAMRBL  ???
    // 168  ?    16?      ??    BBRB    ???
    // 170  ?    16?      ??    BDRBH   ???
    // 172  ?    16?      ??    BDRBL   ???
    // 174  ?    16?      ??    BDMRBH  ???
    // 176  ?    16?      ??    BDMRBL  ???
    // 178  ?    16?      ??    BRCR    ???
    //
    // --- DMAC module (channel 0) ---
    //
    // 180  ?    32?      ??    SAR0    ???
    // 184  ?    32?      ??    DAR0    ???
    // 188  ?    32?      ??    TCR0    ???
    // 18C  ?    32?      ??    CHCR0   ???
    // 1A0  ?    32?      ??    VCRDMA0 ???
    //
    // --- DMAC module (channel 1) ---
    //
    // 190  ?    32?      ??    SAR1    ???
    // 194  ?    32?      ??    DAR1    ???
    // 198  ?    32?      ??    TCR1    ???
    // 19C  ?    32?      ??    CHCR1   ???
    // 1A8  ?    32?      ??    VCRDMA1 ???
    //
    // --- DMAC module (both channels) ---
    //
    // 1B0  ?    32?      ??    DMAOR   ???
    //
    // --- BSC module ---
    //
    // 1E0  R/W  16,32    03F0  BCR1    Bus Control Register 1
    // 1E4  R/W  16,32    00FC  BCR2    Bus Control Register 2
    // 1E8  R/W  16,32    AAFF  WCR     Wait Control Register
    // 1EC  R/W  16,32    0000  MCR     Individual Memory Control Register
    // 1F0  R/W  16,32    0000  RTCSR   Refresh Timer Control/Status Register
    // 1F4  R/W  16,32    0000  RTCNT   Refresh Timer Counter
    // 1F8  R/W  16,32    0000  RTCOR   Refresh Timer Constant Register
};

class SH2 {
public:
    SH2(SH2Bus &bus)
        : m_bus(bus) {
        Reset(true);
    }

    void Reset(bool hard) {
        // Initial values:
        // - R0-R14 = undefined
        // - R15 = ReadLong(VBR + 4)

        // - SR = bits I3-I0 set, reserved bits clear, the rest is undefined
        // - GBR = undefined
        // - VBR = 0x00000000

        // - MACH, MACL = undefined
        // - PR = undefined
        // - PC = ReadLong(VBR)

        R.fill(0);
        PR = 0;

        SR.u32 = 0;
        SR.I0 = SR.I1 = SR.I2 = SR.I3 = 1;
        GBR = 0;
        VBR = 0x00000000;

        MAC.u64 = 0;

        PC = m_bus.ReadLong(VBR);
        R[15] = m_bus.ReadLong(VBR + 4);
    }

    void Step() {
        auto bit = [](bool value, std::string_view bit) { return value ? fmt::format(" {}", bit) : ""; };

        fmt::println(" R0 = {:08X}   R4 = {:08X}   R8 = {:08X}  R12 = {:08X}", R[0], R[4], R[8], R[12]);
        fmt::println(" R1 = {:08X}   R5 = {:08X}   R9 = {:08X}  R13 = {:08X}", R[1], R[5], R[9], R[13]);
        fmt::println(" R2 = {:08X}   R6 = {:08X}  R10 = {:08X}  R14 = {:08X}", R[2], R[6], R[10], R[14]);
        fmt::println(" R3 = {:08X}   R7 = {:08X}  R11 = {:08X}  R15 = {:08X}", R[3], R[7], R[11], R[15]);
        fmt::println("GBR = {:08X}  VBR = {:08X}  MAC = {:08X}.{:08X}", GBR, VBR, MAC.H, MAC.L);
        fmt::println(" PC = {:08X}   PR = {:08X}   SR = {:08X} {}{}{}{}{}{}{}{}", PC, PR, SR.u32, bit(SR.M, "M"),
                     bit(SR.Q, "Q"), bit(SR.I3, "I3"), bit(SR.I2, "I2"), bit(SR.I1, "I1"), bit(SR.I0, "I0"),
                     bit(SR.S, "S"), bit(SR.T, "T"));

        Execute<false>(PC);
        fmt::println("");
    }

    uint32 GetPC() const {
        return PC;
    }

private:
    std::array<uint32, 16> R;

    uint32 PC;
    uint32 PR;

    union SR_t {
        uint32 u32;
        struct {
            uint32 T : 1;
            uint32 S : 1;
            uint32 : 2;
            uint32 I0 : 1;
            uint32 I1 : 1;
            uint32 I2 : 1;
            uint32 I3 : 1;
            uint32 Q : 1;
            uint32 M : 1;
        };
    } SR;
    uint32 GBR;
    uint32 VBR;

    union MAC_t {
        uint64 u64;
        struct {
            uint32 H;
            uint32 L;
        };
    } MAC;

    SH2Bus &m_bus;

    uint64 count = 0;

    template <bool delaySlot>
    void Execute(uint32 address) {
        ++count;
        const uint16 instr = m_bus.ReadWord(address);
        fmt::print("[{:5}] {:08X}{} {:04X}  ", count, address, delaySlot ? '*' : ' ', instr);

        switch (instr >> 12u) {
        case 0x0:
            switch (instr) {
            case 0x0008:
                CLRT();
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x0009:
                NOP();
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x000B:
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    fmt::println("illegal delay slot instruction");
                } else {
                    RTS();
                }
                break;
            case 0x0018:
                SETT();
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x0019:
                DIV0U();
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x001B:
                SLEEP();
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x0028:
                CLRMAC();
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x002B:
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    fmt::println("illegal delay slot instruction");
                } else {
                    RTE();
                }
                break;
            default:
                switch (instr & 0xF) {
                case 0x3:
                    switch ((instr >> 4u) & 0xF) {
                    case 0x0: // 0000 mmmm 0000 0011   BSRF Rm
                        if constexpr (delaySlot) {
                            // TODO: illegal instruction
                            fmt::println("illegal delay slot instruction");
                        } else {
                            BSRF((instr >> 8u) & 0xF);
                        }
                        break;
                    case 0x2: // 0000 mmmm 0010 0011   BRAF Rm
                        if constexpr (delaySlot) {
                            // TODO: illegal instruction
                            fmt::println("illegal delay slot instruction");
                        } else {
                            BRAF((instr >> 8u) & 0xF);
                        }
                        break;
                    default: fmt::println("illegal instruction?"); break;
                    }
                    break;
                case 0xA: {
                    const uint16 rn = (instr >> 8u) & 0xF;
                    switch ((instr >> 4u) & 0xF) {
                    case 0x0:
                        STSMACH(rn);
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                    case 0x1:
                        STSMACL(rn);
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                    case 0x2:
                        STSPR(rn);
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                    default: fmt::println("illegal instruction?"); break;
                    }
                    break;
                }
                default: fmt::println("unhandled 0000 instruction"); break;
                }
                break;
            }
            break;
        case 0x2: {
            const uint16 rm = (instr >> 4u) & 0xF;
            const uint16 rn = (instr >> 8u) & 0xF;
            switch (instr & 0xF) {
            case 0x0: // 0010 nnnn mmmm 0000   MOV.B Rm, @Rn
                MOVBS(rm, rn);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x1: // 0010 nnnn mmmm 0001   MOV.W Rm, @Rn
                MOVWS(rm, rn);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x2: // 0010 nnnn mmmm 0010   MOV.L Rm, @Rn
                MOVLS(rm, rn);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xA: // 0010 nnnn mmmm 1010   XOR Rm, Rn
                XOR(rm, rn);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            default: fmt::println("unhandled 0010 instruction"); break;
            }
            break;
        }
        case 0x3:
            switch (instr & 0xF) {
            case 0x0: // 0011 nnnn mmmm 0000   CMP/EQ Rm, Rn
                CMPEQ((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x6: // 0011 nnnn mmmm 0110   CMP/HI Rm, Rn
                CMPHI((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xC: // 0011 nnnn mmm 1100   ADD Rm, Rn
                ADD((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            default: fmt::println("unhandled 0011 instruction"); break;
            }
            break;
        case 0x4:
            if ((instr & 0xF) == 0xF) {
                // 0100 nnnn mmmm 1111   MAC.W @Rm+, @Rn+
                // TODO: implement
                fmt::println("unhandled MAC.W instruction");
            } else {
                switch (instr & 0xFF) {
                case 0x00: // 0100 nnnn 0000 0000   SHLL Rn
                    SHLL((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x08: // 0100 nnnn 0000 1000   SHLL2 Rn
                    SHLL2((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x0E: // 0110 mmmm 0000 1110   LDC Rm, SR
                    LDCSR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x10: // 0100 nnnn 0001 0000   DT Rn
                    DT((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x18: // 0100 nnnn 0001 1000   SHLL8 Rn
                    SHLL8((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x1E: // 0110 mmmm 0001 1110   LDC Rm, GBR
                    LDCGBR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x28: // 0100 nnnn 0010 1000   SHLL16 Rn
                    SHLL16((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x2A: // 0100 mmmm 0010 1010   LDS Rm, PR
                    LDSPR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x2E: // 0110 mmmm 0010 1110   LDC Rm, VBR
                    LDCVBR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                default: fmt::println("unhandled 0100 instruction"); break;
                }
            }
            break;
        case 0x6:
            switch (instr & 0xF) {
            case 0x2: // 0110 nnnn mmmm 0010   MOV.L @Rm, Rn
                MOVLL((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x6: // 0110 nnnn mmmm 0110   MOV.L @Rm+, Rn
                MOVLP((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xC: // 0110 nnnn mmmm 1100   EXTU.B Rm, Rn
                EXTUB((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xD: // 0110 nnnn mmmm 1101   EXTU.W Rm, Rn
                EXTUW((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xE: // 0110 nnnn mmmm 1100   EXTS.B Rm, Rn
                EXTSB((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xF: // 0110 nnnn mmmm 1101   EXTS.W Rm, Rn
                EXTSW((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            default: fmt::println("unhandled 0110 instruction"); break;
            }
            break;
        case 0x7: // 0111 nnnn iiii iiii   ADD #imm, Rn
            ADDI(instr & 0xFF, (instr >> 8u) & 0xF);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x8:
            switch ((instr >> 8u) & 0xF) {
            case 0x9: // 1000 1001 dddd dddd   BT <label>
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    fmt::println("illegal delay slot instruction");
                } else {
                    BT(instr & 0xFF);
                }
                break;
            case 0xB: // 1000 1011 dddd dddd   BF <label>
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    fmt::println("illegal delay slot instruction");
                } else {
                    BF(instr & 0xFF);
                }
                break;
            case 0xD: // 1000 1101 dddd dddd   BT/S <label>
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    fmt::println("illegal delay slot instruction");
                } else {
                    BTS(instr & 0xFF);
                }
                break;
            case 0xF: // 1000 1111 dddd dddd   BF/S <label>
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    fmt::println("illegal delay slot instruction");
                } else {
                    BFS(instr & 0xFF);
                }
                break;
            default: fmt::println("unhandled 1000 instruction"); break;
            }
            break;
        case 0xA: // 1011 dddd dddd dddd   BRA <label>
            if constexpr (delaySlot) {
                // TODO: illegal instruction
                fmt::println("illegal delay slot instruction");
            } else {
                BRA(instr & 0xFFF);
            }
            break;
        case 0xB: // 1011 dddd dddd dddd   BSR <label>
            if constexpr (delaySlot) {
                // TODO: illegal instruction
                fmt::println("illegal delay slot instruction");
            } else {
                BSR(instr & 0xFFF);
            }
            break;
        case 0xC:
            switch ((instr >> 8u) & 0xF) {
            case 0x2: // 1100 0010 dddd dddd   MOV.L R0, @(disp,GBR)
                MOVLSG(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x7: // 1100 0111 dddd dddd   MOVA @(disp,PC), R0
                MOVA(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            default: fmt::println("unhandled 1100 instruction"); break;
            }
            break;
        case 0xD: // 1101 nnnn dddd dddd   MOV.L @(disp,PC), Rn
            MOVLI(instr & 0xFF, (instr >> 8) & 0xF);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xE: // 1110 nnnn iiii iiii   MOV #imm, Rn
            MOVI(instr & 0xFF, (instr >> 8) & 0xF);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        default: fmt::println("unhandled instruction"); break;
        }
    }

    void ADD(uint16 rm, uint16 rn) {
        fmt::println("add r{}, r{}", rm, rn);
        R[rn] += R[rm];
    }

    void ADDI(uint16 imm, uint16 rn) {
        sint32 simm = SignExtend<8>(imm);
        fmt::println("add #{}0x{:X}, r{}", (simm < 0 ? "-" : ""), abs(simm), rn);
        R[rn] += simm;
    }

    void BF(uint16 disp) {
        sint32 sdisp = (SignExtend<8>(disp) << 1) + 4;
        fmt::println("bf 0x{:08X}", PC + sdisp);

        if (!SR.T) {
            PC += sdisp;
        } else {
            PC += 2;
        }
    }

    void BFS(uint16 disp) {
        sint32 sdisp = (SignExtend<8>(disp) << 1) + 4;
        fmt::println("bf/s 0x{:08X}", PC + sdisp);

        if (!SR.T) {
            uint32 delaySlot = PC + 2;
            PC += sdisp;
            Execute<true>(delaySlot);
        } else {
            PC += 2;
        }
    }

    void BRA(uint16 disp) {
        sint32 sdisp = (SignExtend<12>(disp) << 1) + 4;
        fmt::println("bra 0x{:08X}", PC + sdisp);

        uint32 delaySlot = PC + 2;
        PC += sdisp;
        Execute<true>(delaySlot);
    }

    void BRAF(uint16 rm) {
        fmt::println("braf r{}", rm);
        uint32 delaySlot = PC + 2;
        PC += R[rm] + 4;
        Execute<true>(delaySlot);
    }

    void BSR(uint16 disp) {
        sint32 sdisp = (SignExtend<12>(disp) << 1) + 4;
        fmt::println("bsr 0x{:08X}", PC + sdisp);

        PR = PC;
        PC += sdisp;
        Execute<true>(PR + 2);
    }

    void BSRF(uint16 rm) {
        fmt::println("bsrf r{}", rm);
        PR = PC;
        PC += R[rm] + 4;
        Execute<true>(PR + 2);
    }

    void BT(uint16 disp) {
        sint32 sdisp = (SignExtend<8>(disp) << 1) + 4;
        fmt::println("bt 0x{:08X}", PC + sdisp);

        if (SR.T) {
            PC += sdisp;
        } else {
            PC += 2;
        }
    }

    void BTS(uint16 disp) {
        sint32 sdisp = (SignExtend<8>(disp) << 1) + 4;
        fmt::println("bt/s 0x{:08X}", PC + sdisp);

        if (SR.T) {
            uint32 delaySlot = PC + 2;
            PC += sdisp;
            Execute<true>(delaySlot);
        } else {
            PC += 2;
        }
    }

    void CLRT() {
        fmt::println("clrt");
        SR.T = 0;
    }

    void CLRMAC() {
        fmt::println("clrmac");
        MAC.u64 = 0;
    }

    void CMPEQ(uint16 rm, uint16 rn) {
        fmt::println("cmp/eq r{}, r{}", rm, rn);
        SR.T = R[rn] == R[rm];
    }

    void CMPHI(uint16 rm, uint16 rn) {
        fmt::println("cmp/hi r{}, r{}", rm, rn);
        SR.T = R[rn] > R[rm];
    }

    void DIV0U() {
        fmt::println("div0u");
        SR.M = 0;
        SR.Q = 0;
        SR.T = 0;
    }

    void DT(uint16 rn) {
        fmt::println("dt r{}", rn);
        R[rn]--;
        SR.T = R[rn] == 0;
    }

    void EXTSB(uint16 rm, uint16 rn) {
        fmt::println("exts.b r{}, r{}", rm, rn);
        R[rn] = SignExtend<8>(R[rm]);
    }

    void EXTSW(uint16 rm, uint16 rn) {
        fmt::println("exts.w r{}, r{}", rm, rn);
        R[rn] = SignExtend<16>(R[rm]);
    }

    void EXTUB(uint16 rm, uint16 rn) {
        fmt::println("extu.b r{}, r{}", rm, rn);
        R[rn] = R[rm] & 0xFF;
    }

    void EXTUW(uint16 rm, uint16 rn) {
        fmt::println("extu.w r{}, r{}", rm, rn);
        R[rn] = R[rm] & 0xFFFF;
    }

    void LDCGBR(uint16 rm) {
        fmt::println("ldc r{}, gbr", rm);
        GBR = R[rm];
    }

    void LDCSR(uint16 rm) {
        fmt::println("ldc r{}, sr", rm);
        SR.u32 = R[rm] & 0x000003F3;
    }

    void LDCVBR(uint16 rm) {
        fmt::println("ldc r{}, vbr", rm);
        VBR = R[rm];
    }

    void LDSPR(uint16 rm) {
        fmt::println("lds r{}, pr", rm);
        PR = R[rm];
    }

    void MOVA(uint16 disp) {
        disp = (disp << 2u) + 4;
        fmt::println("mova @(0x{:X},pc), r0", (PC & ~3) + disp);
        R[0] = (PC & ~3) + disp;
    }

    void MOVLL(uint16 rm, uint16 rn) {
        fmt::println("mov.l @r{}, r{}", rm, rn);
        R[rn] = m_bus.ReadLong(R[rm]);
    }

    void MOVLP(uint16 rm, uint16 rn) {
        fmt::println("mov.l @r{}+, r{}", rm, rn);
        R[rn] = m_bus.ReadLong(R[rm]);
        if (rn != rm) {
            R[rm] += 4;
        }
    }

    void MOVBS(uint16 rm, uint16 rn) {
        fmt::println("mov.b r{}, @r{}", rm, rn);
        m_bus.WriteByte(R[rn], R[rm]);
    }

    void MOVWS(uint16 rm, uint16 rn) {
        fmt::println("mov.w r{}, @r{}", rm, rn);
        m_bus.WriteWord(R[rn], R[rm]);
    }

    void MOVLS(uint16 rm, uint16 rn) {
        fmt::println("mov.l r{}, @r{}", rm, rn);
        m_bus.WriteLong(R[rn], R[rm]);
    }

    void MOVLSG(uint16 disp) {
        disp <<= 2;
        fmt::println("mov.l r0, @(0x{:X},gbr)", disp);
        m_bus.WriteLong(GBR + disp, R[0]);
    }

    void MOVI(uint16 imm, uint16 rn) {
        sint32 simm = SignExtend<8>(imm);
        fmt::println("mov #{}0x{:X}, r{}", (simm < 0 ? "-" : ""), abs(simm), rn);
        R[rn] = simm;
    }

    void MOVLI(uint16 disp, uint16 rn) {
        disp <<= 2;
        fmt::println("mov.l @(0x{:08X},pc), r{}", ((PC + 4) & ~3) + disp, rn);
        R[rn] = m_bus.ReadLong(((PC + 4) & ~3u) + disp);
    }

    void NOP() {
        fmt::println("nop");
    }

    void RTE() {
        fmt::println("rte");
        uint32 delaySlot = PC + 2;
        PC = m_bus.ReadLong(R[15] + 4);
        R[15] += 4;
        SR.u32 = m_bus.ReadLong(R[15]) & 0x000003F3;
        R[15] += 4;
        Execute<true>(delaySlot);
    }

    void RTS() {
        fmt::println("rts");
        uint32 delaySlot = PC + 2;
        PC = PR + 4;
        Execute<true>(delaySlot);
    }

    void SETT() {
        fmt::println("sett");
        SR.T = 1;
    }

    void SHLL(uint16 rn) {
        fmt::println("shll r{}", rn);
        SR.T = R[rn] >> 31u;
        R[rn] <<= 1u;
    }

    void SHLL2(uint16 rn) {
        fmt::println("shll2 r{}", rn);
        R[rn] <<= 2u;
    }

    void SHLL8(uint16 rn) {
        fmt::println("shll8 r{}", rn);
        R[rn] <<= 8u;
    }

    void SHLL16(uint16 rn) {
        fmt::println("shll16 r{}", rn);
        R[rn] <<= 16u;
    }

    void SLEEP() {
        PC -= 2;
        // TODO: wait for exception
    }

    void STSMACH(uint16 rn) {
        fmt::println("sts mach, r{}", rn);
        R[rn] = MAC.H;
    }

    void STSMACL(uint16 rn) {
        fmt::println("sts macl, r{}", rn);
        R[rn] = MAC.L;
    }

    void STSPR(uint16 rn) {
        fmt::println("sts pr, r{}", rn);
        R[rn] = PR;
    }

    void XOR(uint16 rm, uint16 rn) {
        fmt::println("xor r{}, r{}", rm, rn);
        R[rn] ^= R[rm];
    }
};

class Saturn {
public:
    Saturn()
        : m_masterSH2(m_sh2bus) {
        Reset(true);
    }

    void Reset(bool hard) {
        m_sh2bus.Reset(hard);
        m_masterSH2.Reset(hard);
    }

    void LoadIPL(std::span<uint8, kIPLSize> ipl) {
        m_sh2bus.LoadIPL(ipl);
    }

    void Step() {
        m_masterSH2.Step();
    }

    SH2 &MasterSH2() {
        return m_masterSH2;
    }

private:
    SH2Bus m_sh2bus;
    SH2 m_masterSH2;
};

std::vector<uint8_t> loadROM(std::filesystem::path romPath) {
    fmt::print("Loading ROM from {}... ", romPath.string());

    std::vector<uint8_t> rom;
    std::ifstream romStream{romPath, std::ios::binary | std::ios::ate};
    if (romStream.is_open()) {
        auto size = romStream.tellg();
        romStream.seekg(0, std::ios::beg);
        fmt::println("{} bytes", (size_t)size);

        rom.resize(size);
        romStream.read(reinterpret_cast<char *>(rom.data()), size);
    } else {
        fmt::println("failed!");
    }
    return rom;
}

int main(int argc, char **argv) {
    fmt::println("satemu " satemu_VERSION);
    if (argc < 2) {
        fmt::println("missing argument: rompath");
        fmt::println("    rompath   Path to Saturn BIOS ROM");
        return EXIT_FAILURE;
    }

    auto saturn = std::make_unique<Saturn>();

    auto rom = loadROM(argv[1]);
    if (rom.size() != kIPLSize) {
        fmt::println("IPL ROM size mismatch: expected {} bytes, got {} bytes", kIPLSize, rom.size());
        return EXIT_FAILURE;
    }
    saturn->LoadIPL(std::span<uint8, kIPLSize>(rom));
    fmt::println("IPL ROM loaded");

    saturn->Reset(true);
    auto &msh2 = saturn->MasterSH2();
    uint32 prevPC = msh2.GetPC();
    for (;;) {
        saturn->Step();
        uint32 currPC = msh2.GetPC();
        if (currPC == prevPC) {
            break;
        }
        prevPC = currPC;
    }

    return EXIT_SUCCESS;
}
