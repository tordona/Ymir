#pragma once

#include "sh2_bus.hpp"

#include "satemu/core_types.hpp"

#include "satemu/util/bit_ops.hpp"
#include "satemu/util/unreachable.hpp"

#include <algorithm>

namespace satemu {

class SH2 {
public:
    SH2(SH2Bus &bus, bool master)
        : m_bus(bus) {
        BCR1.MASTER = !master;
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

        PC = MemReadLong(VBR);
        R[15] = MemReadLong(VBR + 4);

        // On-chip registers
        IPRB.val.u16 = 0x0000;
        VCRA.val.u16 = 0x0000;
        VCRB.val.u16 = 0x0000;
        VCRC.val.u16 = 0x0000;
        VCRD.val.u16 = 0x0000;
        ICR.val.u16 = 0x0000;
        IPRA.val.u16 = 0x0000;
        VCRWDT.val.u16 = 0x0000;
        VCRDIV = 0x0000; // undefined initial value
        VCRDMA0 = 0x00;  // undefined initial value
        VCRDMA1 = 0x00;  // undefined initial value
        BCR1.u15 = 0x03F0;
        BCR2.u16 = 0x00FC;
        WCR.u16 = 0xAAFF;
        MCR.u16 = 0x0000;

        m_cacheEntries.fill({});
        WriteCCR(0x00);
    }

    void Step() {
        auto bit = [](bool value, std::string_view bit) { return value ? fmt::format(" {}", bit) : ""; };

        dbg_println(" R0 = {:08X}   R4 = {:08X}   R8 = {:08X}  R12 = {:08X}", R[0], R[4], R[8], R[12]);
        dbg_println(" R1 = {:08X}   R5 = {:08X}   R9 = {:08X}  R13 = {:08X}", R[1], R[5], R[9], R[13]);
        dbg_println(" R2 = {:08X}   R6 = {:08X}  R10 = {:08X}  R14 = {:08X}", R[2], R[6], R[10], R[14]);
        dbg_println(" R3 = {:08X}   R7 = {:08X}  R11 = {:08X}  R15 = {:08X}", R[3], R[7], R[11], R[15]);
        dbg_println("GBR = {:08X}  VBR = {:08X}  MAC = {:08X}.{:08X}", GBR, VBR, MAC.H, MAC.L);
        dbg_println(" PC = {:08X}   PR = {:08X}   SR = {:08X} {}{}{}{}{}{}{}{}", PC, PR, SR.u32, bit(SR.M, "M"),
                    bit(SR.Q, "Q"), bit(SR.I3, "I3"), bit(SR.I2, "I2"), bit(SR.I1, "I1"), bit(SR.I0, "I0"),
                    bit(SR.S, "S"), bit(SR.T, "T"));

        Execute<false>(PC);
        dbg_println("");
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
            uint32 L;
            uint32 H;
        };
    } MAC;

    SH2Bus &m_bus;

    uint64 dbg_count = 0;
    static constexpr uint64 dbg_minCount = 17635778; // 10489689; // 9302150; // 9547530;

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
    }

    // -------------------------------------------------------------------------
    // Memory accessors

    // According to the SH7604 manual, the address space is divided into these areas:
    //
    // Address range            Space                           Memory
    // 0x00000000..0x01FFFFFF   CS0 space, cache area           Ordinary space or burst ROM
    // 0x02000000..0x03FFFFFF   CS1 space, cache area           Ordinary space
    // 0x04000000..0x05FFFFFF   CS2 space, cache area           Ordinary space or synchronous DRAM
    // 0x06000000..0x07FFFFFF   CS3 space, cache area           Ordinary space, synchronous SDRAM, DRAM or pseudo-DRAM
    // 0x08000000..0x1FFFFFFF   Reserved
    // 0x20000000..0x21FFFFFF   CS0 space, cache-through area   Ordinary space or burst ROM
    // 0x22000000..0x23FFFFFF   CS1 space, cache-through area   Ordinary space
    // 0x24000000..0x25FFFFFF   CS2 space, cache-through area   Ordinary space or synchronous DRAM
    // 0x26000000..0x27FFFFFF   CS3 space, cache-through area   Ordinary space, synchronous SDRAM, DRAM or pseudo-DRAM
    // 0x28000000..0x3FFFFFFF   Reserved
    // 0x40000000..0x47FFFFFF   Associative purge space
    // 0x48000000..0x5FFFFFFF   Reserved
    // 0x60000000..0x7FFFFFFF   Address array, read/write space
    // 0x80000000..0x9FFFFFFF   Reserved  [undocumented mirror of 0xC0000000..0xDFFFFFFF]
    // 0xA0000000..0xBFFFFFFF   Reserved  [undocumented mirror of 0x20000000..0x3FFFFFFF]
    // 0xC0000000..0xC0000FFF   Data array, read/write space
    // 0xC0001000..0xDFFFFFFF   Reserved
    // 0xE0000000..0xFFFF7FFF   Reserved
    // 0xFFFF8000..0xFFFFBFFF   For setting synchronous DRAM mode
    // 0xFFFFC000..0xFFFFFDFF   Reserved
    // 0xFFFFFE00..0xFFFFFFFF   On-chip peripheral modules
    //
    // The cache uses address bits 31..29 to specify its behavior:
    //    Bits  Partition                       Cache operation
    //    000   Cache area                      Cache used when CCR.CE=1
    //    001   Cache-through area              Cache bypassed
    //    010   Associative purge area          Purge accessed cache lines (reads return 0x2312)
    //    011   Address array read/write area   Cache addresses acessed directly (1 KiB, mirrored)
    //    100   [undocumented, same as 110]
    //    101   [undocumented, same as 001]
    //    110   Data array read/write area      Cache data acessed directly (4 KiB, mirrored)
    //    111   I/O area (on-chip registers)    Cache bypassed

    template <mem_access_type T>
    T MemRead(uint32 address) {
        const uint32 partition = (address >> 29u) & 0b111;
        if (address & static_cast<uint32>(sizeof(T) - 1)) {
            fmt::println("WARNING: misaligned {}-bit read from {:08X}", sizeof(T) * 8, address);
            // TODO: address error (misaligned access)
            // - might have to store data in a class member instead of returning
        }

        switch (partition) {
        case 0b000: // cache

            if (CCR.CE) {
                // TODO: use cache
            }
            // fallthrough
        case 0b001:
        case 0b101: // cache-through
            return m_bus.Read<T>(address & 0x7FFFFFF);
        case 0b010: // associative purge

            // TODO: implement
            fmt::println("unhandled {}-bit SH-2 associative purge read from {:08X}", sizeof(T) * 8, address);
            return (address & 1) ? static_cast<T>(0x12231223) : static_cast<T>(0x23122312);
        case 0b011: { // cache address array
            uint32 entry = (address >> 4u) & 0x3F;
            return m_cacheEntries[entry].tag[CCR.Wn]; // TODO: include LRU data
        }
        case 0b100:
        case 0b110: // cache data array

            // TODO: implement
            fmt::println("unhandled {}-bit SH-2 cache data array read from {:08X}", sizeof(T) * 8, address);
            return 0;
        case 0b111: // I/O area
            if ((address & 0xE0004000) == 0xE0004000) {
                // bits 31-29 and 14 must be set
                // bits 8-0 index the register
                // bits 28 and 12 must be both set to access the lower half of the registers
                if ((address & 0x100) || (address & 0x10001000) == 0x10001000) {
                    return OnChipRegRead<T>(address & 0x1FF);
                } else {
                    return OpenBusSeqRead<T>(address);
                }
            } else {
                // TODO: implement
                fmt::println("unhandled {}-bit SH-2 I/O area read from {:08X}", sizeof(T) * 8, address);
                return 0;
            }
        }

        util::unreachable();
    }

    template <mem_access_type T>
    void MemWrite(uint32 address, T value) {
        const uint32 partition = address >> 29u;
        if (address & static_cast<uint32>(sizeof(T) - 1)) {
            fmt::println("WARNING: misaligned {}-bit write to {:08X} = {:X}", sizeof(T) * 8, address, value);
            // TODO: address error (misaligned access)
        }

        switch (partition) {
        case 0b000: // cache
            if (CCR.CE) {
                // TODO: use cache
            }
            // fallthrough
        case 0b001:
        case 0b101: // cache-through
            m_bus.Write<T>(address & 0x7FFFFFF, value);
            break;
        case 0b010: // associative purge
            // TODO: implement
            fmt::println("unhandled {}-bit SH-2 associative purge write to {:08X} = {:X}", sizeof(T) * 8, address,
                         value);
            break;
        case 0b011: { // cache address array
            uint32 entry = (address >> 4u) & 0x3F;
            m_cacheEntries[entry].tag[CCR.Wn] = address & 0x1FFFFC04;
            // TODO: update LRU data
            break;
        }
        case 0b100:
        case 0b110: // cache data array
            // TODO: implement
            fmt::println("unhandled {}-bit SH-2 cache data array write to {:08X} = {:X}", sizeof(T) * 8, address,
                         value);
            break;
        case 0b111: // I/O area
            if ((address & 0xE0004000) == 0xE0004000) {
                // bits 31-29 and 14 must be set
                // bits 8-0 index the register
                // bits 28 and 12 must be both set to access the lower half of the registers
                if ((address & 0x100) || (address & 0x10001000) == 0x10001000) {
                    OnChipRegWrite<T>(address & 0x1FF, value);
                }
            } else if ((address >> 12u) == 0xFFFF8) {
                // DRAM setup stuff
                switch (address) {
                case 0xFFFF8426: fmt::println("16-bit CAS latency 1"); break;
                case 0xFFFF8446: fmt::println("16-bit CAS latency 2"); break;
                case 0xFFFF8466: fmt::println("16-bit CAS latency 3"); break;
                case 0xFFFF8848: fmt::println("32-bit CAS latency 1"); break;
                case 0xFFFF8888: fmt::println("32-bit CAS latency 2"); break;
                case 0xFFFF88C8: fmt::println("32-bit CAS latency 3"); break;
                default: fmt::println("unhandled {}-bit SH-2 I/O area read from {:08X}", sizeof(T) * 8, address); break;
                }
            } else {
                // TODO: implement
                fmt::println("unhandled {}-bit SH-2 I/O area write to {:08X} = {:X}", sizeof(T) * 8, address, value);
            }
            break;
        }
    }

    uint8 MemReadByte(uint32 address) {
        return MemRead<uint8>(address);
    }

    uint16 MemReadWord(uint32 address) {
        return MemRead<uint16>(address);
    }

    uint32 MemReadLong(uint32 address) {
        return MemRead<uint32>(address);
    }

    void MemWriteByte(uint32 address, uint8 value) {
        MemWrite<uint8>(address, value);
    }

    void MemWriteWord(uint32 address, uint16 value) {
        MemWrite<uint16>(address, value);
    }

    void MemWriteLong(uint32 address, uint32 value) {
        MemWrite<uint32>(address, value);
    }

    // Returns 00 00 00 01 00 02 00 03 00 04 00 05 00 06 00 07 ... repeating
    template <mem_access_type T>
    T OpenBusSeqRead(uint32 address) {
        if constexpr (std::is_same_v<T, uint8>) {
            return (address & 1u) * ((address >> 1u) & 0x7);
            // return OpenBusSeqRead<uint16>(address) >> (((address & 1) ^ 1) * 8);
        } else if constexpr (std::is_same_v<T, uint16>) {
            return (address >> 1u) & 0x7;
        } else if constexpr (std::is_same_v<T, uint32>) {
            return (OpenBusSeqRead<uint16>(address + 1) << 16u) | OpenBusSeqRead<uint16>(address);
        }
    }

    // -------------------------------------------------------------------------
    // On-chip peripherals

    union Reg16 {
        uint16 u16;
        uint8 u8[2];
    };

    // --- SCI module ---
    //
    // addr r/w  access   init  code    name
    // 000  R/W  8        00    SMR     Serial Mode Register
    //
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

    // 060  R/W  8,16     0000  IPRB    Interrupt priority setting register B
    //
    //   bits   r/w  code       description
    //   15-12  R/W  SCIIP3-0   Serial Communication Interface (SCI) Interrupt Priority Level
    //   11-8   R/W  FRTIP3-0   Free-Running Timer (FRT) Interrupt Priority Level
    //    7-0   R/W  Reserved   Must be zero
    //
    //   Interrupt priority levels range from 0 to 15.
    union IPRB_t {
        Reg16 val;
        struct {
            uint16 _rsvd0_7 : 8;
            uint16 FRTIPn : 4;
            uint16 SCIIPn : 4;
        };
    } IPRB;

    // 062  R/W  8,16     0000  VCRA    Vector number setting register A
    //
    //   bits   r/w  code     description
    //     15   R    -        Reserved - must be zero
    //   14-8   R/W  SERV6-0  Serial Communication Interface (SCI) Receive-Error Interrupt Vector Number
    //      7   R    -        Reserved - must be zero
    //    6-0   R/W  SRXV6-0  Serial Communication Interface (SCI) Receive-Data-Full Interrupt Vector Number
    union VCRA_t {
        Reg16 val;
        struct {
            uint16 SRXVn : 7;
            uint16 _rsvd7 : 1;
            uint16 SERVn : 7;
            uint16 _rsvd15 : 1;
        };
    } VCRA;

    // 064  R/W  8,16     0000  VCRB    Vector number setting register B
    //
    //   bits   r/w  code     description
    //     15   R    -        Reserved - must be zero
    //   14-8   R/W  STXV6-0  Serial Communication Interface (SCI) Transmit-Data-Empty Interrupt Vector Number
    //      7   R    -        Reserved - must be zero
    //    6-0   R/W  STEV6-0  Serial Communication Interface (SCI) Transmit-End Interrupt Vector Number
    union VCRB_t {
        Reg16 val;
        struct {
            uint16 STEVn : 7;
            uint16 _rsvd7 : 1;
            uint16 STXVn : 7;
            uint16 _rsvd15 : 1;
        };
    } VCRB;

    // 066  R/W  8,16     0000  VCRC    Vector number setting register C
    //
    //   bits   r/w  code     description
    //     15   R    -        Reserved - must be zero
    //   14-8   R/W  FICV6-0  Free-Running Timer (FRT) Input-Capture Interrupt Vector Number
    //      7   R    -        Reserved - must be zero
    //    6-0   R/W  FOCV6-0  Free-Running Timer (FRT) Output-Compare Interrupt Vector Number
    union VCRC_t {
        Reg16 val;
        struct {
            uint16 FOCVn : 7;
            uint16 _rsvd7 : 1;
            uint16 FICVn : 7;
            uint16 _rsvd15 : 1;
        };
    } VCRC;

    // 068  R/W  8,16     0000  VCRD    Vector number setting register D
    //
    //   bits   r/w  code     description
    //     15   R    -        Reserved - must be zero
    //   14-8   R/W  FOVV6-0  Free-Running Timer (FRT) Overflow Interrupt Vector Number
    //    7-0   R    -        Reserved - must be zero
    union VCRD_t {
        Reg16 val;
        struct {
            uint16 _rsvd0_7 : 8;
            uint16 FOVVn : 7;
            uint16 _rsvd15 : 1;
        };
    } VCRD;

    // 0E0  R/W  8,16     0000  ICR     Interrupt control register
    //
    //   bits   r/w  code   description
    //     15   R    NMIL   NMI Input Level
    //   14-9   R    -      Reserved - must be zero
    //      8   R/W  NMIE   NMI Edge Select (0=falling, 1=rising)
    //    7-1   R    -      Reserved - must be zero
    //      0   R/W  VECMD  IRL Interrupt Vector Mode Select (0=auto, 1=external)
    //                      Auto-vector mode assigns 71 to IRL15 and IRL14, and 64 to IRL1.
    //                      External vector mode reads from external vector number input pins D7-D0.
    //
    //    The default value may be either 8000 or 0000 because NMIL is an external signal.
    union ICR_t {
        Reg16 val;
        struct {
            uint16 VECMD : 1;
            uint16 _rsvd1_7 : 7;
            uint16 NMIE : 1;
            uint16 _rsvd9_14 : 6;
            uint16 NMIL : 1;
        };
    } ICR;

    // 0E2  R/W  8,16     0000  IPRA    Interrupt priority setting register A
    //
    //   bits   r/w  code       description
    //   15-12  R/W  DIVUIP3-0  Division Unit (DIVU) Interrupt Priority Level
    //   11-8   R/W  DMACIP3-0  DMA Controller (DMAC) Interrupt Priority Level
    //    7-4   R/W  WDTIP3-0   Watchdog Timer (WDT) Interrupt Priority Level
    //    3-0   R    -          Reserved - must be zero
    //
    //   Interrupt priority levels range from 0 to 15.
    //
    //   The DMAC priority level is assigned to both channels.
    //   If both channels raise an interrupt, channel 0 is prioritized.
    //
    //   WDTIP3-0 includes both the watchdog timer and bus state controller (BSC).
    //   WDT interrupt has priority over BSC.
    union IPRA_t {
        Reg16 val;
        struct {
            uint16 _rsvd0_3 : 4;
            uint16 WDTIPn : 4;
            uint16 DMACIPn : 4;
            uint16 DIVUIPn : 4;
        };
    } IPRA;

    // 0E4  R/W  8,16     0000  VCRWDT  Vector number setting register WDT
    //
    //   bits   r/w  code     description
    //     15   R    -        Reserved - must be zero
    //   14-8   R/W  WITV6-0  Watchdog Timer (WDT) Interval Interrupt Vector Number
    //      7   R    -        Reserved - must be zero
    //    6-0   R/W  BCMV6-0  Bus State Controller (BSC) Compare Match Interrupt Vector Number
    union VCRWDT_t {
        Reg16 val;
        struct {
            uint16 BCMVn : 7;
            uint16 _rsvd7 : 1;
            uint16 WITVn : 7;
            uint16 _rsvd15 : 1;
        };
    } VCRWDT;

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

    static constexpr size_t kCacheWays = 4;
    static constexpr size_t kCacheEntries = 64;
    static constexpr size_t kCacheLineSize = 16;

    struct CacheEntry {
        // Tag layout:
        //   28..10: tag
        //        2: valid bit
        // All other bits must be zero
        // This matches the address array structure
        std::array<uint32, kCacheWays> tag;
        std::array<std::array<uint8, kCacheLineSize>, kCacheWays> line;
    };
    alignas(16) std::array<CacheEntry, kCacheEntries> m_cacheEntries;

    // 092  R/W  8        00    CCR     Cache Control Register
    //
    //   bits   r/w  code   description
    //      7   R/W  W1     Way Specification (MSB)
    //      6   R/W  W0     Way Specification (LSB)
    //      5   R    -      Reserved - must be zero
    //      4   R/W  CP     Cache Purge (0=normal, 1=purge)
    //      3   R/W  TW     Two-Way Mode (0=four-way, 1=two-way)
    //      2   R/W  OD     Data Replacement Disable (0=disabled, 1=data cache not updated on miss)
    //      1   R/W  ID     Instruction Replacement Disabled (same as above, but for code cache)
    //      0   R/W  CE     Cache Enable (0=disable, 1=enable)
    union CCR_t {
        uint8 u8;
        struct {
            uint8 CE : 1;
            uint8 ID : 1;
            uint8 OD : 1;
            uint8 TW : 1;
            uint8 CP : 1;
            uint8 _rsvd5 : 1;
            uint8 Wn : 2;
        };
    } CCR;

    void WriteCCR(uint8 value) {
        if (CCR.u8 == value) {
            return;
        }

        // fmt::println("CCR changed from 0x{:02X} to 0x{:02X}", CCR.u8, value);
        CCR.u8 = value;
        if (CCR.CP) {
            // fmt::println("  cache purged");
            // TODO: purge cache
            CCR.CP = 0;
        }
    }

    // 0E0, 0E2, 0E4 are in INTC module above

    // --- DIVU module ---
    //
    // 100  ?    32?      ??    DVSR    ???
    // 104  ?    32?      ??    DVDNT   ???
    // 108  ?    32?      ??    DVCR    ???

    // 10C  R/W  16,32    ??    VCRDIV  Vector number register setting DIV
    //
    //   bits   r/w  code   description
    //  31-16   R    -      Reserved - must be zero
    //   15-0   R/W  -      Interrupt Vector Number
    uint16 VCRDIV;

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
    // --- DMAC module ---

    // 180  ?    32?      ??    SAR0    ???
    // 184  ?    32?      ??    DAR0    ???
    // 188  ?    32?      ??    TCR0    ???
    // 18C  ?    32?      ??    CHCR0   ???

    // 1A0  R/W  32       ??    VCRDMA0 DMA vector number register 0
    //
    //   bits   r/w  code   description
    //   31-8   R    -      Reserved - must be zero
    //    7-0   R/W  VC7-0  Vector Number
    uint8 VCRDMA0;

    //
    // 190  ?    32?      ??    SAR1    ???
    // 194  ?    32?      ??    DAR1    ???
    // 198  ?    32?      ??    TCR1    ???
    // 19C  ?    32?      ??    CHCR1   ???

    // 1A8  R/W  32       ??    VCRDMA1 DMA vector number register 1
    //
    //   bits   r/w  code   description
    //   31-8   R    -      Reserved - must be zero
    //    7-0   R/W  VC7-0  Vector Number
    uint8 VCRDMA1;

    // 1B0  ?    32?      ??    DMAOR   ???
    //
    // --- BSC module ---
    //
    // 1E0  R/W  16,32    03F0  BCR1    Bus Control Register 1
    union BCR1_t {
        uint16 u16;
        struct {
            uint16 DRAMn : 3;
            uint16 _rsvd3 : 1;
            uint16 A0LWn : 2;
            uint16 A1LWn : 2;
            uint16 AHLWn : 2;
            uint16 PSHR : 1;
            uint16 BSTROM : 1;
            uint16 ENDIAN : 1;
            uint16 _rsvd13 : 1;
            uint16 _rsvd14 : 1;
            uint16 MASTER : 1;
        };
        uint16 u15 : 15;
    } BCR1;

    // 1E4  R/W  16,32    00FC  BCR2    Bus Control Register 2
    union BCR2_t {
        uint16 u16;
        struct {
            uint16 _rsvd0 : 1;
            uint16 _rsvd1 : 1;
            uint16 A1SZn : 2;
            uint16 A2SZn : 2;
            uint16 A3SZn : 2;
        };
    } BCR2;

    // 1E8  R/W  16,32    AAFF  WCR     Wait Control Register
    union WCR_t {
        uint16 u16;
        struct {
            uint16 W0n : 2;
            uint16 W1n : 2;
            uint16 W2n : 2;
            uint16 W3n : 2;
            uint16 IW0n : 2;
            uint16 IW1n : 2;
            uint16 IW2n : 2;
            uint16 IW3n : 2;
        };
    } WCR;

    // 1EC  R/W  16,32    0000  MCR     Individual Memory Control Register
    union MCR_t {
        uint16 u16;
        struct {
            uint16 _rsvd0 : 1;
            uint16 _rsvd1 : 1;
            uint16 RMD : 1;
            uint16 RFSH : 1;
            uint16 AMX0 : 1;
            uint16 AMX1 : 1;
            uint16 SZ : 1;
            uint16 AMX2 : 1;
            uint16 _rsvd8 : 1;
            uint16 RASD : 1;
            uint16 BE : 1;
            uint16 TRASn : 2;
            uint16 TRWL : 1;
            uint16 RCD : 1;
            uint16 TRP : 1;
        };
    } MCR;

    // 1F0  R/W  16,32    0000  RTCSR   Refresh Timer Control/Status Register
    union RTCSR_t {
        uint16 u16;
        struct {
            uint16 _rsvd0 : 1;
            uint16 _rsvd1 : 1;
            uint16 _rsvd2 : 1;
            uint16 CKSn : 3;
            uint16 CMIE : 1;
            uint16 CMF : 1;
        };
    } RTCSR;

    // 1F4  R/W  16,32    0000  RTCNT   Refresh Timer Counter
    uint8 RTCNT;

    // 1F8  R/W  16,32    0000  RTCOR   Refresh Timer Constant Register
    uint8 RTCOR;

    template <mem_access_type T>
    T OnChipRegRead(uint32 address) {
        // Misaligned memory accesses raise an address error, meaning all accesses here are aligned.
        // Therefore:
        //   (address & 3) == 2 is only valid for 16-bit accesses
        //   (address & 1) == 1 is only valid for 8-bit accesses
        // Additionally:
        //   (address & 1) == 0 has special cases for registers 0-255:
        //     8-bit read from a 16-bit register:  r >> 8u
        //     16-bit read from a 8-bit register: (r << 8u) | r
        //     Every other access returns just r

        // Registers 0-255 do not accept 32-bit accesses
        if constexpr (std::is_same_v<T, uint32>) {
            if (address < 0x100) {
                // TODO: raise an address error
            }
        }

        // Registers 256-511 do not accept 8-bit accesses
        if constexpr (std::is_same_v<T, uint8>) {
            if (address >= 0x100) {
                // TODO: raise an address error
            }
        }

        auto readWordLower = [&](Reg16 value) -> T {
            if constexpr (std::is_same_v<T, uint8>) {
                return value.u8[(address & 1) ^ 1];
            } else {
                return value.u16;
            }
        };
        auto readByteLower = [&](uint8 value) -> T {
            if constexpr (std::is_same_v<T, uint16>) {
                return (address & 1) ? value : ((value << 8u) | value);
            } else {
                return value;
            }
        };

        // Note: Clang generates marginally faster code with case ranges here.
        // See:
        // https://quick-bench.com/q/vB2HZ3bzAIlqIazYoVxy7A0ooKg

        switch (address) {
        case 0x60 ... 0x61: return readWordLower(IPRB.val);
        case 0x62 ... 0x63: return readWordLower(VCRA.val);
        case 0x64 ... 0x65: return readWordLower(VCRB.val);
        case 0x66 ... 0x67: return readWordLower(VCRC.val);
        case 0x68 ... 0x69: return readWordLower(VCRD.val);
        case 0x92 ... 0x9F: return readByteLower(CCR.u8);
        case 0xE0 ... 0xE1: return readWordLower(ICR.val);
        case 0xE2 ... 0xE3: return readWordLower(IPRA.val);
        case 0xE4 ... 0xE5: return readWordLower(VCRWDT.val);

        case 0x10C: return VCRDIV;

        case 0x1A0: return VCRDMA0;
        case 0x1A8: return VCRDMA1;

        case 0x1E0 ... 0x1E2: return BCR1.u16;
        case 0x1E4 ... 0x1E6: return BCR2.u16;
        case 0x1E8 ... 0x1EA: return WCR.u16;
        case 0x1EC ... 0x1EE: return MCR.u16;
        case 0x1F0 ... 0x1F2: return RTCSR.u16;
        case 0x1F4 ... 0x1F6: return RTCNT;
        case 0x1F8 ... 0x1FA: return RTCOR;

        default: //
            fmt::println("unhandled {}-bit on-chip register read from {:02X}", sizeof(T) * 8, address);
            return 0;
        }
    }

    template <mem_access_type T>
    void OnChipRegWrite(uint32 address, T baseValue) {
        // Misaligned memory accesses raise an address error, meaning all accesses here are aligned.
        // Therefore:
        //   (address & 3) == 2 is only valid for 16-bit accesses
        //   (address & 1) == 1 is only valid for 8-bit accesses

        // Registers 0-255 do not accept 32-bit accesses
        if constexpr (std::is_same_v<T, uint32>) {
            if (address < 0x100) {
                // TODO: raise an address error
            }
        }

        // Registers 256-511 do not accept 8-bit accesses
        uint32 value = baseValue;
        if constexpr (std::is_same_v<T, uint8>) {
            if (address >= 0x100) {
                // TODO: raise an address error
                value |= value << 8u;
            }
        }

        // For registers 0-255, 8-bit writes to 16-bit registers change the corresponding byte
        auto writeWordLower = [&](Reg16 &reg, T value, uint16 mask) {
            if constexpr (std::is_same_v<T, uint8>) {
                uint32 index = ((address & 1) ^ 1);
                mask >>= index * 8;
                if ((mask & 0xFF) != 0) {
                    reg.u8[index] = value & mask;
                }
            } else {
                reg.u16 = value & mask;
            }
        };

        // Note: the repeated cases below might seem redundant, but it actually causes Clang to generate better code.
        // Case ranges (case 0x61 ... 0x62) or fallthrough (case 0x61: case 0x62) generate suboptimal code.
        // See:
        // https://quick-bench.com/q/j3XUHw-u-Y75chqoIPnxwyF3JXw
        // https://godbolt.org/z/5nbPxqd5d

        switch (address) {
        case 0x60: writeWordLower(IPRB.val, value, 0xFF00); break;
        case 0x61: writeWordLower(IPRB.val, value, 0xFF00); break;
        case 0x62: writeWordLower(VCRA.val, value, 0x7F7F); break;
        case 0x63: writeWordLower(VCRA.val, value, 0x7F7F); break;
        case 0x64: writeWordLower(VCRB.val, value, 0x7F7F); break;
        case 0x65: writeWordLower(VCRB.val, value, 0x7F7F); break;
        case 0x66: writeWordLower(VCRC.val, value, 0x7F7F); break;
        case 0x67: writeWordLower(VCRC.val, value, 0x7F7F); break;
        case 0x68: writeWordLower(VCRD.val, value, 0x7F00); break;
        case 0x69: writeWordLower(VCRD.val, value, 0x7F00); break;

        case 0x92: WriteCCR(value); break;

        case 0xE0: writeWordLower(ICR.val, value, 0x0101); break;
        case 0xE1: writeWordLower(ICR.val, value, 0x0101); break;
        case 0xE2: writeWordLower(IPRA.val, value, 0xFFF0); break;
        case 0xE3: writeWordLower(IPRA.val, value, 0xFFF0); break;
        case 0xE4: writeWordLower(VCRWDT.val, value, 0x7F7F); break;
        case 0xE5: writeWordLower(VCRWDT.val, value, 0x7F7F); break;

        case 0x10C: VCRDIV = value; break;

        case 0x1A0: VCRDMA0 = value; break;
        case 0x1A8: VCRDMA1 = value; break;

        case 0x1E0: // BCR1
            // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
            if constexpr (std::is_same_v<T, uint32>) {
                if ((value >> 16u) == 0xA55A) {
                    BCR1.u15 = value & 0x1FF7;
                }
            }
            break;
        case 0x1E4: // BCR2
            // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
            if constexpr (std::is_same_v<T, uint32>) {
                if ((value >> 16u) == 0xA55A) {
                    BCR2.u16 = value & 0xFC;
                }
            }
            break;
        case 0x1E8: // WCR
            // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
            if constexpr (std::is_same_v<T, uint32>) {
                if ((value >> 16u) == 0xA55A) {
                    WCR.u16 = value;
                }
            }
            break;
        case 0x1EC: // MCR
            // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
            if constexpr (std::is_same_v<T, uint32>) {
                if ((value >> 16u) == 0xA55A) {
                    MCR.u16 = value & 0xFEFC;
                }
            }
            break;
        case 0x1F0: // RTCSR
            // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
            if constexpr (std::is_same_v<T, uint32>) {
                if ((value >> 16u) == 0xA55A) {
                    // TODO: implement the set/clear rules for RTCSR.CMF
                    RTCSR.u16 = (value & 0x78) | (RTCSR.u16 & 0x80);
                }
            }
            break;
        case 0x1F4: // RTCNT
            // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
            if constexpr (std::is_same_v<T, uint32>) {
                if ((value >> 16u) == 0xA55A) {
                    RTCNT = value;
                }
            }
            break;
        case 0x1F8: // RTCOR
            // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
            if constexpr (std::is_same_v<T, uint32>) {
                if ((value >> 16u) == 0xA55A) {
                    RTCOR = value;
                }
            }
            break;
        default: //
            fmt::println("unhandled {}-bit on-chip register write to {:02X} = {:X}", sizeof(T) * 8, address, value);
            break;
        }
    }

    // -------------------------------------------------------------------------
    // Execution

    template <bool delaySlot>
    void Execute(uint32 address) {
        const uint16 instr = MemReadWord(address);

        ++dbg_count;
        dbg_print("[{:10}] {:08X}{} {:04X}  ", dbg_count, address, delaySlot ? '*' : ' ', instr);

        switch (instr >> 12u) {
        case 0x0:
            switch (instr) {
            case 0x0008: // 0000 0000 0000 1000   CLRT
                CLRT();
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x0009: // 0000 0000 0000 1001   NOP
                NOP();
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x000B: // 0000 0000 0000 1011   RTS
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    dbg_println("illegal delay slot instruction");
                } else {
                    RTS();
                }
                break;
            case 0x0018: // 0000 0000 0001 1000   SETT
                SETT();
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x0019: // 0000 0000 0001 1001   DIV0U
                DIV0U();
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x001B: // 0000 0000 0001 1011   SLEEP
                SLEEP();
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x0028: // 0000 0000 0010 1000   CLRMAC
                CLRMAC();
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x002B: // 0000 0000 0010 1011   RTE
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    dbg_println("illegal delay slot instruction");
                } else {
                    RTE();
                }
                break;
            default:
                switch (instr & 0xFF) {
                case 0x02: // 0000 nnnn 0000 0010   STC SR, Rn
                    STCSR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x03: // 0000 mmmm 0000 0011   BSRF Rm
                    if constexpr (delaySlot) {
                        // TODO: illegal instruction
                        dbg_println("illegal delay slot instruction");
                    } else {
                        BSRF(bit::extract<8, 11>(instr));
                    }
                    break;
                case 0x0A: // 0000 nnnn 0000 1010   STS MACH, Rn
                    STSMACH(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x12: // 0000 nnnn 0001 0010   STC GBR, Rn
                    STCGBR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x1A: // 0000 nnnn 0001 1010   STS MACL, Rn
                    STSMACL(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x22: // 0000 nnnn 0010 0010   STC VBR, Rn
                    STCVBR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x23: // 0000 mmmm 0010 0011   BRAF Rm
                    if constexpr (delaySlot) {
                        // TODO: illegal instruction
                        dbg_println("illegal delay slot instruction");
                    } else {
                        BRAF(bit::extract<8, 11>(instr));
                    }
                    break;
                case 0x29: // 0000 nnnn 0010 1001   MOVT Rn
                    MOVT(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x2A: // 0000 nnnn 0010 1010   STS PR, Rn
                    STSPR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                default:
                    switch (instr & 0xF) {
                    case 0x4: // 0000 nnnn mmmm 0100   MOV.B Rm, @(R0,Rn)
                        MOVBS0(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                    case 0x5: // 0000 nnnn mmmm 0101   MOV.W Rm, @(R0,Rn)
                        MOVWS0(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                    case 0x6: // 0000 nnnn mmmm 0110   MOV.L Rm, @(R0,Rn)
                        MOVLS0(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                    case 0x7: // 0000 nnnn mmmm 0111   MUL.L Rm, Rn
                        MULL(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                    case 0xC: // 0000 nnnn mmmm 1100   MOV.B @(R0,Rm), Rn
                        MOVBL0(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                    case 0xD: // 0000 nnnn mmmm 1101   MOV.W @(R0,Rm), Rn
                        MOVWL0(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                    case 0xE: // 0000 nnnn mmmm 1110   MOV.L @(R0,Rm), Rn
                        MOVLL0(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                    case 0xF: // 0000 nnnn mmmm 1111   MAC.L @Rm+, @Rn+
                        MACL(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                    default: dbg_println("unhandled 0000 instruction"); break;
                    }
                    break;
                }
                break;
            }
            break;
        case 0x1: // 0001 nnnn mmmm dddd   MOV.L Rm, @(disp,Rn)
            MOVLS4(bit::extract<4, 7>(instr), bit::extract<0, 3>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x2: {
            const uint16 rm = bit::extract<4, 7>(instr);
            const uint16 rn = bit::extract<8, 11>(instr);
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

                // There's no case 0x3

            case 0x4: // 0010 nnnn mmmm 0100   MOV.B Rm, @-Rn
                MOVBM(rm, rn);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x5: // 0010 nnnn mmmm 0101   MOV.W Rm, @-Rn
                MOVWM(rm, rn);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x6: // 0010 nnnn mmmm 0110   MOV.L Rm, @-Rn
                MOVLM(rm, rn);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x7: // 0010 nnnn mmmm 0110   DIV0S Rm, Rn
                DIV0S(rm, rn);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x8: // 0010 nnnn mmmm 1000   TST Rm, Rn
                TST(rm, rn);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x9: // 0010 nnnn mmmm 1001   AND Rm, Rn
                AND(rm, rn);
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
            case 0xB: // 0010 nnnn mmmm 1011   OR Rm, Rn
                OR(rm, rn);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xC: // 0010 nnnn mmmm 1100   CMP/STR Rm, Rn
                CMPSTR(rm, rn);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xD: // 0010 nnnn mmmm 1101   XTRCT Rm, Rn
                XTRCT(rm, rn);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xE: // 0010 nnnn mmmm 1110   MULU.W Rm, Rn
                MULU(rm, rn);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xF: // 0010 nnnn mmmm 1111   MULS.W Rm, Rn
                MULS(rm, rn);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            default: dbg_println("unhandled 0010 instruction"); break;
            }
            break;
        }
        case 0x3:
            switch (instr & 0xF) {
            case 0x0: // 0011 nnnn mmmm 0000   CMP/EQ Rm, Rn
                CMPEQ(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x2: // 0011 nnnn mmmm 0010   CMP/HS Rm, Rn
                CMPHS(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x3: // 0011 nnnn mmmm 0011   CMP/GE Rm, Rn
                CMPGE(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x4: // 0011 nnnn mmmm 0100   DIV1 Rm, Rn
                DIV1(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x5: // 0011 nnnn mmmm 0101   DMULU.L Rm, Rn
                DMULU(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x6: // 0011 nnnn mmmm 0110   CMP/HI Rm, Rn
                CMPHI(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x7: // 0011 nnnn mmmm 0111   CMP/GT Rm, Rn
                CMPGT(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x8: // 0011 nnnn mmmm 1000   SUB Rm, Rn
                SUB(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x9: // 0011 nnnn mmmm 1001   SUBC Rm, Rn
                SUBC(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xA: // 0011 nnnn mmmm 1010   SUBV Rm, Rn
                SUBV(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;

                // There's no case 0xB

            case 0xC: // 0011 nnnn mmmm 1100   ADD Rm, Rn
                ADD(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xD: // 0011 nnnn mmmm 1101   DMULS.L Rm, Rn
                DMULS(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xE: // 0011 nnnn mmmm 1110   ADDC Rm, Rn
                ADDC(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xF: // 0011 nnnn mmmm 1110   ADDV Rm, Rn
                ADDV(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            default: dbg_println("unhandled 0011 instruction"); break;
            }
            break;
        case 0x4:
            if ((instr & 0xF) == 0xF) {
                // 0100 nnnn mmmm 1111   MAC.W @Rm+, @Rn+
                MACW(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            } else {
                switch (instr & 0xFF) {
                case 0x00: // 0100 nnnn 0000 0000   SHLL Rn
                    SHLL(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x01: // 0100 nnnn 0000 0001   SHLR Rn
                    SHLR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x02: // 0100 nnnn 0000 0010   STS.L MACH, @-Rn
                    STSMMACH(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x03: // 0100 nnnn 0000 0010   STC.L SR, @-Rn
                    STCMSR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x04: // 0100 nnnn 0000 0100   ROTL Rn
                    ROTL(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x05: // 0100 nnnn 0000 0101   ROTR Rn
                    ROTR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x06: // 0100 mmmm 0000 0110   LDS.L @Rm+, MACH
                    LDSMMACH(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x07: // 0100 mmmm 0000 0111   LDC.L @Rm+, SR
                    LDCMSR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x08: // 0100 nnnn 0000 1000   SHLL2 Rn
                    SHLL2(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x09: // 0100 nnnn 0000 1001   SHLR2 Rn
                    SHLR2(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x0A: // 0100 mmmm 0000 1010   LDS Rm, MACH
                    LDSMACH(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x0B: // 0110 mmmm 0000 1110   JSR @Rm
                    if constexpr (delaySlot) {
                        // TODO: illegal instruction
                        dbg_println("illegal delay slot instruction");
                    } else {
                        JSR(bit::extract<8, 11>(instr));
                    }
                    break;

                    // There's no case 0x0C or 0x0D

                case 0x0E: // 0110 mmmm 0000 1110   LDC Rm, SR
                    LDCSR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;

                    // There's no case 0x0F

                case 0x10: // 0100 nnnn 0001 0000   DT Rn
                    DT(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x11: // 0100 nnnn 0001 0001   CMP/PZ Rn
                    CMPPZ(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x12: // 0100 nnnn 0001 0010   STS.L MACL, @-Rn
                    STSMMACL(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x13: // 0100 nnnn 0001 0011   STC.L GBR, @-Rn
                    STCMGBR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;

                    // There's no case 0x14

                case 0x15: // 0100 nnnn 0001 0101   CMP/PL Rn
                    CMPPL(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x16: // 0100 mmmm 0001 0110   LDS.L @Rm+, MACL
                    LDSMMACL(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x17: // 0100 mmmm 0001 0111   LDC.L @Rm+, GBR
                    LDCMGBR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x18: // 0100 nnnn 0001 1000   SHLL8 Rn
                    SHLL8(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x19: // 0100 nnnn 0001 1001   SHLR8 Rn
                    SHLR8(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x1A: // 0100 mmmm 0001 1010   LDS Rm, MACL
                    LDSMACL(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x1B: // 0100 nnnn 0001 1011   TAS.B @Rn
                    TAS(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;

                    // There's no case 0x1C or 0x1D

                case 0x1E: // 0110 mmmm 0001 1110   LDC Rm, GBR
                    LDCGBR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;

                    // There's no case 0x1F

                case 0x20: // 0100 nnnn 0010 0000   SHAL Rn
                    SHAL(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x21: // 0100 nnnn 0010 0001   SHAR Rn
                    SHAR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x22: // 0100 nnnn 0010 0010   STS.L PR, @-Rn
                    STSMPR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x23: // 0100 nnnn 0010 0011   STC.L VBR, @-Rn
                    STCMVBR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x24: // 0100 nnnn 0010 0100   ROTCL Rn
                    ROTCL(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x25: // 0100 nnnn 0010 0101   ROTCR Rn
                    ROTCR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x26: // 0100 mmmm 0010 0110   LDS.L @Rm+, PR
                    LDSMPR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x27: // 0100 mmmm 0010 0111   LDC.L @Rm+, VBR
                    LDCMVBR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x28: // 0100 nnnn 0010 1000   SHLL16 Rn
                    SHLL16(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x29: // 0100 nnnn 0010 1001   SHLR16 Rn
                    SHLR16(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x2A: // 0100 mmmm 0010 1010   LDS Rm, PR
                    LDSPR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x2B: // 0100 mmmm 0010 1011   JMP @Rm
                    if constexpr (delaySlot) {
                        // TODO: illegal instruction
                        dbg_println("illegal delay slot instruction");
                    } else {
                        JMP(bit::extract<8, 11>(instr));
                    }
                    break;

                    // There's no case 0x2C or 0x2D

                case 0x2E: // 0110 mmmm 0010 1110   LDC Rm, VBR
                    LDCVBR(bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;

                    // There's no case 0x2F..0xFF

                default: dbg_println("unhandled 0100 instruction"); break;
                }
            }
            break;
        case 0x5: // 0101 nnnn mmmm dddd   MOV.L @(disp,Rm), Rn
            MOVLL4(bit::extract<4, 7>(instr), bit::extract<0, 3>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x6:
            switch (instr & 0xF) {
            case 0x0: // 0110 nnnn mmmm 0000   MOV.B @Rm, Rn
                MOVBL(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x1: // 0110 nnnn mmmm 0001   MOV.W @Rm, Rn
                MOVWL(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x2: // 0110 nnnn mmmm 0010   MOV.L @Rm, Rn
                MOVLL(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x3: // 0110 nnnn mmmm 0010   MOV Rm, Rn
                MOV(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x4: // 0110 nnnn mmmm 0110   MOV.B @Rm+, Rn
                MOVBP(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x5: // 0110 nnnn mmmm 0110   MOV.W @Rm+, Rn
                MOVWP(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x6: // 0110 nnnn mmmm 0110   MOV.L @Rm+, Rn
                MOVLP(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x7: // 0110 nnnn mmmm 0111   NOT Rm, Rn
                NOT(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x8: // 0110 nnnn mmmm 1000   SWAP.B Rm, Rn
                SWAPB(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x9: // 0110 nnnn mmmm 1001   SWAP.W Rm, Rn
                SWAPW(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xA: // 0110 nnnn mmmm 1010   NEGC Rm, Rn
                NEGC(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xB: // 0110 nnnn mmmm 1011   NEG Rm, Rn
                NEG(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xC: // 0110 nnnn mmmm 1100   EXTU.B Rm, Rn
                EXTUB(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xD: // 0110 nnnn mmmm 1101   EXTU.W Rm, Rn
                EXTUW(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xE: // 0110 nnnn mmmm 1110   EXTS.B Rm, Rn
                EXTSB(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xF: // 0110 nnnn mmmm 1111   EXTS.W Rm, Rn
                EXTSW(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            }
            break;
        case 0x7: // 0111 nnnn iiii iiii   ADD #imm, Rn
            ADDI(bit::extract<0, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x8:
            switch ((instr >> 8u) & 0xF) {
            case 0x0: // 1000 0000 nnnn dddd   MOV.B R0, @(disp,Rn)
                MOVBS4(bit::extract<0, 3>(instr), bit::extract<4, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x1: // 1000 0001 nnnn dddd   MOV.W R0, @(disp,Rn)
                MOVWS4(bit::extract<0, 3>(instr), bit::extract<4, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;

                // There's no case 0x2 or 0x3

            case 0x4: // 1000 0100 mmmm dddd   MOV.B @(disp,Rm), R0
                MOVBL4(bit::extract<4, 7>(instr), bit::extract<0, 3>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x5: // 1000 0101 mmmm dddd   MOV.W @(disp,Rm), R0
                MOVWL4(bit::extract<4, 7>(instr), bit::extract<0, 3>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;

                // There's no case 0x6 or 0x7

            case 0x8: // 1000 1000 iiii iiii   CMP/EQ #imm, R0
                CMPIM(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x9: // 1000 1001 dddd dddd   BT <label>
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    dbg_println("illegal delay slot instruction");
                } else {
                    BT(bit::extract<0, 7>(instr));
                }
                break;

                // There's no case 0xA

            case 0xB: // 1000 1011 dddd dddd   BF <label>
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    dbg_println("illegal delay slot instruction");
                } else {
                    BF(bit::extract<0, 7>(instr));
                }
                break;

                // There's no case 0xC

            case 0xD: // 1000 1101 dddd dddd   BT/S <label>
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    dbg_println("illegal delay slot instruction");
                } else {
                    BTS(bit::extract<0, 7>(instr));
                }
                break;

                // There's no case 0xE

            case 0xF: // 1000 1111 dddd dddd   BF/S <label>
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    dbg_println("illegal delay slot instruction");
                } else {
                    BFS(bit::extract<0, 7>(instr));
                }
                break;
            default: dbg_println("unhandled 1000 instruction"); break;
            }
            break;
        case 0x9: // 1001 nnnn dddd dddd   MOV.W @(disp,PC), Rn
            MOVWI(bit::extract<0, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xA: // 1011 dddd dddd dddd   BRA <label>
            if constexpr (delaySlot) {
                // TODO: illegal instruction
                dbg_println("illegal delay slot instruction");
            } else {
                BRA(bit::extract<0, 11>(instr));
            }
            break;
        case 0xB: // 1011 dddd dddd dddd   BSR <label>
            if constexpr (delaySlot) {
                // TODO: illegal instruction
                dbg_println("illegal delay slot instruction");
            } else {
                BSR(bit::extract<0, 11>(instr));
            }
            break;
        case 0xC:
            switch ((instr >> 8u) & 0xF) {
            case 0x0: // 1100 0000 dddd dddd   MOV.B R0, @(disp,GBR)
                MOVBSG(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x1: // 1100 0001 dddd dddd   MOV.W R0, @(disp,GBR)
                MOVWSG(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x2: // 1100 0010 dddd dddd   MOV.L R0, @(disp,GBR)
                MOVLSG(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x3: // 1100 0011 iiii iiii   TRAPA #imm
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    dbg_println("illegal delay slot instruction");
                } else {
                    TRAPA(bit::extract<0, 7>(instr));
                }
                break;
            case 0x4: // 1100 0100 dddd dddd   MOV.B @(disp,GBR), R0
                MOVBLG(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x5: // 1100 0101 dddd dddd   MOV.W @(disp,GBR), R0
                MOVWLG(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x6: // 1100 0110 dddd dddd   MOV.L @(disp,GBR), R0
                MOVLLG(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x7: // 1100 0111 dddd dddd   MOVA @(disp,PC), R0
                MOVA(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x8: // 1100 1000 iiii iiii   TST #imm, R0
                TSTI(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x9: // 1100 1001 iiii iiii   AND #imm, R0
                ANDI(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xA: // 1100 1010 iiii iiii   XOR #imm, R0
                XORI(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xB: // 1100 1011 iiii iiii   OR #imm, R0
                ORI(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xC: // 1100 1100 iiii iiii   TST.B #imm, @(R0,GBR)
                TSTM(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xD: // 1100 1001 iiii iiii   AND #imm, @(R0,GBR)
                ANDM(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xE: // 1100 1001 iiii iiii   XOR #imm, @(R0,GBR)
                XORM(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xF: // 1100 1001 iiii iiii   OR #imm, @(R0,GBR)
                ORM(bit::extract<0, 7>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            default: dbg_println("unhandled 1100 instruction"); break;
            }
            break;
        case 0xD: // 1101 nnnn dddd dddd   MOV.L @(disp,PC), Rn
            MOVLI(bit::extract<0, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xE: // 1110 nnnn iiii iiii   MOV #imm, Rn
            MOVI(bit::extract<0, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;

            // There's no case 0xF

        default: dbg_println("unhandled instruction"); break;
        }

        // if ((PC & 0x7FFFFFF) == 0x0003b6e)
        // if ((PC & 0x7FFFFFF) == 0x0004a9c)
        // if ((PC & 0x7FFFFFF) == 0x6001278)
        // if ((PC & 0x7FFFFFF) == 0x0003378)
        // if ((PC & 0x7FFFFFF) == 0x00033d0)
        // if ((PC & 0x7FFFFFF) == 0x0002630)
        //    __debugbreak();
    }

    void ADD(uint16 rm, uint16 rn) {
        dbg_println("add r{}, r{}", rm, rn);
        R[rn] += R[rm];
    }

    void ADDI(uint16 imm, uint16 rn) {
        sint32 simm = bit::sign_extend<8>(imm);
        dbg_println("add #{}0x{:X}, r{}", (simm < 0 ? "-" : ""), abs(simm), rn);
        R[rn] += simm;
    }

    void ADDC(uint16 rm, uint16 rn) {
        dbg_println("addc r{}, r{}", rm, rn);
        uint32 tmp1 = R[rn] + R[rm];
        uint32 tmp0 = R[rn];
        R[rn] = tmp1 + SR.T;
        SR.T = (tmp0 > tmp1) || (tmp1 > R[rn]);
    }

    void ADDV(uint16 rm, uint16 rn) {
        dbg_println("addv r{}, r{}", rm, rn);

        bool dst = static_cast<sint32>(R[rn]) < 0;
        bool src = static_cast<sint32>(R[rm]) < 0;

        R[rn] += R[rm];

        bool ans = static_cast<sint32>(R[rn]) < 0;
        ans ^= dst;
        SR.T = (src == dst) & ans;
    }

    void AND(uint16 rm, uint16 rn) {
        dbg_println("and r{}, r{}", rm, rn);
        R[rn] &= R[rm];
    }

    void ANDI(uint16 imm) {
        dbg_println("and #0x{:X}, r0", imm);
        R[0] &= imm;
    }

    void ANDM(uint16 imm) {
        dbg_println("and.b #0x{:X}, @(r0,gbr)", imm);
        uint8 tmp = MemReadByte(GBR + R[0]);
        tmp &= imm;
        MemWriteByte(GBR + R[0], tmp);
    }

    void BF(uint16 disp) {
        sint32 sdisp = (bit::sign_extend<8>(disp) << 1) + 4;
        dbg_println("bf 0x{:08X}", PC + sdisp);

        if (!SR.T) {
            PC += sdisp;
        } else {
            PC += 2;
        }
    }

    void BFS(uint16 disp) {
        sint32 sdisp = (bit::sign_extend<8>(disp) << 1) + 4;
        dbg_println("bf/s 0x{:08X}", PC + sdisp);

        if (!SR.T) {
            uint32 delaySlot = PC + 2;
            PC += sdisp;
            Execute<true>(delaySlot);
        } else {
            PC += 2;
        }
    }

    void BRA(uint16 disp) {
        sint32 sdisp = (bit::sign_extend<12>(disp) << 1) + 4;
        dbg_println("bra 0x{:08X}", PC + sdisp);

        uint32 delaySlot = PC + 2;
        PC += sdisp;
        Execute<true>(delaySlot);
    }

    void BRAF(uint16 rm) {
        dbg_println("braf r{}", rm);
        uint32 delaySlot = PC + 2;
        PC += R[rm] + 4;
        Execute<true>(delaySlot);
    }

    void BSR(uint16 disp) {
        sint32 sdisp = (bit::sign_extend<12>(disp) << 1) + 4;
        dbg_println("bsr 0x{:08X}", PC + sdisp);

        PR = PC;
        PC += sdisp;
        Execute<true>(PR + 2);
    }

    void BSRF(uint16 rm) {
        dbg_println("bsrf r{}", rm);
        PR = PC;
        PC += R[rm] + 4;
        Execute<true>(PR + 2);
    }

    void BT(uint16 disp) {
        sint32 sdisp = (bit::sign_extend<8>(disp) << 1) + 4;
        dbg_println("bt 0x{:08X}", PC + sdisp);

        if (SR.T) {
            PC += sdisp;
        } else {
            PC += 2;
        }
    }

    void BTS(uint16 disp) {
        sint32 sdisp = (bit::sign_extend<8>(disp) << 1) + 4;
        dbg_println("bt/s 0x{:08X}", PC + sdisp);

        if (SR.T) {
            uint32 delaySlot = PC + 2;
            PC += sdisp;
            Execute<true>(delaySlot);
        } else {
            PC += 2;
        }
    }

    void CLRMAC() {
        dbg_println("clrmac");
        MAC.u64 = 0;
    }

    void CLRT() {
        dbg_println("clrt");
        SR.T = 0;
    }

    void CMPEQ(uint16 rm, uint16 rn) {
        dbg_println("cmp/eq r{}, r{}", rm, rn);
        SR.T = R[rn] == R[rm];
    }

    void CMPGE(uint16 rm, uint16 rn) {
        dbg_println("cmp/ge r{}, r{}", rm, rn);
        SR.T = static_cast<sint32>(R[rn]) >= static_cast<sint32>(R[rm]);
    }

    void CMPGT(uint16 rm, uint16 rn) {
        dbg_println("cmp/gt r{}, r{}", rm, rn);
        SR.T = static_cast<sint32>(R[rn]) > static_cast<sint32>(R[rm]);
    }

    void CMPHI(uint16 rm, uint16 rn) {
        dbg_println("cmp/hi r{}, r{}", rm, rn);
        SR.T = R[rn] > R[rm];
    }

    void CMPHS(uint16 rm, uint16 rn) {
        dbg_println("cmp/hs r{}, r{}", rm, rn);
        SR.T = R[rn] >= R[rm];
    }

    void CMPIM(uint16 imm) {
        sint32 simm = bit::sign_extend<8>(imm);
        dbg_println("cmp/eq #{}0x{:X}, r0", (simm < 0 ? "-" : ""), abs(simm));
        SR.T = R[0] == simm;
    }

    void CMPPL(uint16 rn) {
        dbg_println("cmp/pl r{}", rn);
        SR.T = R[rn] > 0;
    }

    void CMPPZ(uint16 rn) {
        dbg_println("cmp/pz r{}", rn);
        SR.T = R[rn] >= 0;
    }

    void CMPSTR(uint16 rm, uint16 rn) {
        dbg_println("cmp/str r{}, r{}", rm, rn);
        uint16 tmp = R[rm] & R[rn];
        uint8 hh = tmp >> 12u;
        uint8 hl = tmp >> 8u;
        uint8 lh = tmp >> 4u;
        uint8 ll = tmp >> 0u;
        SR.T = !(hh && hl && lh && ll);
    }

    void DIV0S(uint16 rm, uint16 rn) {
        dbg_println("div0s r{}, r{}", rm, rn);
        SR.M = static_cast<sint32>(R[rm]) < 0;
        SR.Q = static_cast<sint32>(R[rn]) < 0;
        SR.T = SR.M != SR.Q;
    }

    void DIV0U() {
        dbg_println("div0u");
        SR.M = 0;
        SR.Q = 0;
        SR.T = 0;
    }

    void DIV1(uint16 rm, uint16 rn) {
        dbg_println("div1 r{}, r{}", rm, rn);

        const bool oldQ = SR.Q;
        SR.Q = static_cast<sint32>(R[rn]) < 0;
        R[rn] = (R[rn] << 1u) | SR.T;

        const uint32 prevVal = R[rn];
        if (oldQ == SR.M) {
            R[rn] -= R[rm];
        } else {
            R[rn] += R[rm];
        }

        if (oldQ) {
            if (SR.M) {
                SR.Q ^= R[rn] <= prevVal;
            } else {
                SR.Q ^= R[rn] < prevVal;
            }
        } else {
            if (SR.M) {
                SR.Q ^= R[rn] >= prevVal;
            } else {
                SR.Q ^= R[rn] > prevVal;
            }
        }

        SR.T = SR.Q == SR.M;
    }

    void DMULS(uint16 rm, uint16 rn) {
        dbg_println("dmuls.l r{}, r{}", rm, rn);
        auto cast = [](uint32 val) { return static_cast<sint64>(static_cast<sint32>(val)); };
        MAC.u64 = cast(R[rm]) * cast(R[rn]);
    }

    void DMULU(uint16 rm, uint16 rn) {
        dbg_println("dmulu.l r{}, r{}", rm, rn);
        MAC.u64 = static_cast<uint64>(R[rm]) * static_cast<uint64>(R[rn]);
    }

    void DT(uint16 rn) {
        dbg_println("dt r{}", rn);
        R[rn]--;
        SR.T = R[rn] == 0;
    }

    void EXTSB(uint16 rm, uint16 rn) {
        dbg_println("exts.b r{}, r{}", rm, rn);
        R[rn] = bit::sign_extend<8>(R[rm]);
    }

    void EXTSW(uint16 rm, uint16 rn) {
        dbg_println("exts.w r{}, r{}", rm, rn);
        R[rn] = bit::sign_extend<16>(R[rm]);
    }

    void EXTUB(uint16 rm, uint16 rn) {
        dbg_println("extu.b r{}, r{}", rm, rn);
        R[rn] = R[rm] & 0xFF;
    }

    void EXTUW(uint16 rm, uint16 rn) {
        dbg_println("extu.w r{}, r{}", rm, rn);
        R[rn] = R[rm] & 0xFFFF;
    }

    void JMP(uint16 rm) {
        dbg_println("jmp @r{}", rm);
        uint32 delaySlot = PC + 2;
        PC = R[rm];
        Execute<true>(delaySlot);
    }

    void JSR(uint16 rm) {
        dbg_println("jsr @r{}", rm);
        PR = PC;
        PC = R[rm];
        Execute<true>(PR + 2);
    }

    void LDCGBR(uint16 rm) {
        dbg_println("ldc r{}, gbr", rm);
        GBR = R[rm];
    }

    void LDCSR(uint16 rm) {
        dbg_println("ldc r{}, sr", rm);
        SR.u32 = R[rm] & 0x000003F3;
    }

    void LDCVBR(uint16 rm) {
        dbg_println("ldc r{}, vbr", rm);
        VBR = R[rm];
    }

    void LDCMSR(uint16 rm) {
        dbg_println("ldc.l @r{}+, sr", rm);
        SR.u32 = MemReadLong(R[rm]) & 0x000003F3;
        R[rm] += 4;
    }

    void LDCMGBR(uint16 rm) {
        dbg_println("ldc.l @r{}+, gbr", rm);
        GBR = MemReadLong(R[rm]);
        R[rm] += 4;
    }

    void LDCMVBR(uint16 rm) {
        dbg_println("ldc.l @r{}+, vbr", rm);
        VBR = MemReadLong(R[rm]);
        R[rm] += 4;
    }

    void LDSMACH(uint16 rm) {
        dbg_println("lds r{}, mach", rm);
        MAC.H = R[rm];
    }

    void LDSMACL(uint16 rm) {
        dbg_println("lds r{}, macl", rm);
        MAC.L = R[rm];
    }

    void LDSPR(uint16 rm) {
        dbg_println("lds r{}, pr", rm);
        PR = R[rm];
    }

    void LDSMMACH(uint16 rm) {
        dbg_println("lds.l @r{}+, mach", rm);
        MAC.H = MemReadLong(R[rm]);
        R[rm] += 4;
    }

    void LDSMMACL(uint16 rm) {
        dbg_println("lds.l @r{}+, macl", rm);
        MAC.L = MemReadLong(R[rm]);
        R[rm] += 4;
    }

    void LDSMPR(uint16 rm) {
        dbg_println("lds.l @r{}+, pr", rm);
        PR = MemReadLong(R[rm]);
        R[rm] += 4;
    }

    void MOV(uint16 rm, uint16 rn) {
        dbg_println("mov r{}, r{}", rm, rn);
        R[rn] = R[rm];
    }

    void MACW(uint16 rm, uint16 rn) {
        dbg_println("mac.w @r{}+, $r{}+)", rm, rn);

        sint32 op2 = bit::sign_extend<16, sint32>(MemReadWord(R[rn]));
        R[rn] += 2;
        sint32 op1 = bit::sign_extend<16, sint32>(MemReadWord(R[rm]));
        R[rm] += 2;

        sint32 mul = op1 * op2;
        if (SR.S) {
            sint64 result = static_cast<sint64>(static_cast<sint32>(MAC.L)) + mul;
            sint32 saturatedResult = std::clamp<sint64>(result, 0xFFFFFFFF'80000000, 0x00000000'7FFFFFFF);
            if (result == saturatedResult) {
                MAC.L = result;
            } else {
                MAC.L = saturatedResult;
                MAC.H = 1;
            }
        } else {
            MAC.u64 += mul;
        }
    }

    void MACL(uint16 rm, uint16 rn) {
        dbg_println("mac.l @r{}+, $r{}+)", rm, rn);

        sint64 op2 = static_cast<sint64>(static_cast<sint32>(MemReadLong(R[rn])));
        R[rn] += 4;
        sint64 op1 = static_cast<sint64>(static_cast<sint32>(MemReadLong(R[rm])));
        R[rm] += 4;

        sint64 mul = op1 * op2;
        sint64 result = mul + MAC.u64;
        if (SR.S) {
            if (bit::extract<63>((result ^ MAC.u64) & (result ^ mul))) {
                if (bit::extract<63>(MAC.u64)) {
                    result = 0xFFFF8000'00000000;
                } else {
                    result = 0x00007FFF'FFFFFFFF;
                }
            } else {
                result = std::clamp<sint64>(result, 0xFFFF8000'00000000, 0x00007FFF'FFFFFFFF);
            }
        }
        MAC.u64 = result;
    }

    void MOVA(uint16 disp) {
        disp = (disp << 2u) + 4;
        dbg_println("mova @(0x{:X},pc), r0", (PC & ~3) + disp);
        R[0] = (PC & ~3) + disp;
    }

    void MOVBL(uint16 rm, uint16 rn) {
        dbg_println("mov.b @r{}, r{}", rm, rn);
        R[rn] = bit::sign_extend<8>(MemReadByte(R[rm]));
    }

    void MOVWL(uint16 rm, uint16 rn) {
        dbg_println("mov.w @r{}, r{}", rm, rn);
        R[rn] = bit::sign_extend<16>(MemReadWord(R[rm]));
    }

    void MOVLL(uint16 rm, uint16 rn) {
        dbg_println("mov.l @r{}, r{}", rm, rn);
        R[rn] = MemReadLong(R[rm]);
    }

    void MOVBL0(uint16 rm, uint16 rn) {
        dbg_println("mov.b @(r0,r{}), r{}", rm, rn);
        R[rn] = bit::sign_extend<8>(MemReadByte(R[rm] + R[0]));
    }

    void MOVWL0(uint16 rm, uint16 rn) {
        dbg_println("mov.w @(r0,r{}), r{})", rm, rn);
        R[rn] = bit::sign_extend<16>(MemReadWord(R[rm] + R[0]));
    }

    void MOVLL0(uint16 rm, uint16 rn) {
        dbg_println("mov.l @(r0,r{}), r{})", rm, rn);
        R[rn] = MemReadLong(R[rm] + R[0]);
    }

    void MOVBL4(uint16 rm, uint16 disp) {
        dbg_println("mov.b @(0x{:X},r{}), r0", disp, rm);
        R[0] = bit::sign_extend<8>(MemReadByte(R[rm] + disp));
    }

    void MOVWL4(uint16 rm, uint16 disp) {
        disp <<= 1u;
        dbg_println("mov.w @(0x{:X},r{}), r0", disp, rm);
        R[0] = bit::sign_extend<16>(MemReadWord(R[rm] + disp));
    }

    void MOVLL4(uint16 rm, uint16 disp, uint16 rn) {
        disp <<= 2u;
        dbg_println("mov.l @(0x{:X},r{}), r{}", disp, rm, rn);
        R[rn] = MemReadLong(R[rm] + disp);
    }

    void MOVBLG(uint16 disp) {
        dbg_println("mov.b @(0x{:X},gbr), r0", disp);
        R[0] = bit::sign_extend<8>(MemReadByte(GBR + disp));
    }

    void MOVWLG(uint16 disp) {
        disp <<= 1u;
        dbg_println("mov.w @(0x{:X},gbr), r0", disp);
        R[0] = bit::sign_extend<16>(MemReadWord(GBR + disp));
    }

    void MOVLLG(uint16 disp) {
        disp <<= 2u;
        dbg_println("mov.l @(0x{:X},gbr), r0", disp);
        R[0] = MemReadLong(GBR + disp);
    }

    void MOVBM(uint16 rm, uint16 rn) {
        dbg_println("mov.b r{}, @-r{}", rm, rn);
        MemWriteByte(R[rn] - 1, R[rm]);
        R[rn] -= 1;
    }

    void MOVWM(uint16 rm, uint16 rn) {
        dbg_println("mov.w r{}, @-r{}", rm, rn);
        MemWriteWord(R[rn] - 2, R[rm]);
        R[rn] -= 2;
    }

    void MOVLM(uint16 rm, uint16 rn) {
        dbg_println("mov.l r{}, @-r{}", rm, rn);
        MemWriteLong(R[rn] - 4, R[rm]);
        R[rn] -= 4;
    }

    void MOVBP(uint16 rm, uint16 rn) {
        dbg_println("mov.b @r{}+, r{}", rm, rn);
        R[rn] = bit::sign_extend<8>(MemReadByte(R[rm]));
        if (rn != rm) {
            R[rm] += 1;
        }
    }

    void MOVWP(uint16 rm, uint16 rn) {
        dbg_println("mov.w @r{}+, r{}", rm, rn);
        R[rn] = bit::sign_extend<16>(MemReadWord(R[rm]));
        if (rn != rm) {
            R[rm] += 2;
        }
    }

    void MOVLP(uint16 rm, uint16 rn) {
        dbg_println("mov.l @r{}+, r{}", rm, rn);
        R[rn] = MemReadLong(R[rm]);
        if (rn != rm) {
            R[rm] += 4;
        }
    }

    void MOVBS(uint16 rm, uint16 rn) {
        dbg_println("mov.b r{}, @r{}", rm, rn);
        MemWriteByte(R[rn], R[rm]);
    }

    void MOVWS(uint16 rm, uint16 rn) {
        dbg_println("mov.w r{}, @r{}", rm, rn);
        MemWriteWord(R[rn], R[rm]);
    }

    void MOVLS(uint16 rm, uint16 rn) {
        dbg_println("mov.l r{}, @r{}", rm, rn);
        MemWriteLong(R[rn], R[rm]);
    }

    void MOVBS0(uint16 rm, uint16 rn) {
        dbg_println("mov.b r{}, @(r0,r{})", rm, rn);
        MemWriteByte(R[rn] + R[0], R[rm]);
    }

    void MOVWS0(uint16 rm, uint16 rn) {
        dbg_println("mov.w r{}, @(r0,r{})", rm, rn);
        MemWriteWord(R[rn] + R[0], R[rm]);
    }

    void MOVLS0(uint16 rm, uint16 rn) {
        dbg_println("mov.l r{}, @(r0,r{})", rm, rn);
        MemWriteLong(R[rn] + R[0], R[rm]);
    }

    void MOVBS4(uint16 disp, uint16 rn) {
        dbg_println("mov.b r0, @(0x{:X},r{})", disp, rn);
        MemWriteByte(R[rn] + disp, R[0]);
    }

    void MOVWS4(uint16 disp, uint16 rn) {
        disp <<= 1u;
        dbg_println("mov.w r0, @(0x{:X},r{})", disp, rn);
        MemWriteWord(R[rn] + disp, R[0]);
    }

    void MOVLS4(uint16 rm, uint16 disp, uint16 rn) {
        disp <<= 2u;
        dbg_println("mov.l r{}, @(0x{:X},r{})", rm, disp, rn);
        MemWriteLong(R[rn] + disp, R[rm]);
    }

    void MOVBSG(uint16 disp) {
        dbg_println("mov.b r0, @(0x{:X},gbr)", disp);
        MemWriteByte(GBR + disp, R[0]);
    }

    void MOVWSG(uint16 disp) {
        disp <<= 1u;
        dbg_println("mov.w r0, @(0x{:X},gbr)", disp);
        MemWriteWord(GBR + disp, R[0]);
    }

    void MOVLSG(uint16 disp) {
        disp <<= 2u;
        dbg_println("mov.l r0, @(0x{:X},gbr)", disp);
        MemWriteLong(GBR + disp, R[0]);
    }

    void MOVI(uint16 imm, uint16 rn) {
        sint32 simm = bit::sign_extend<8>(imm);
        dbg_println("mov #{}0x{:X}, r{}", (simm < 0 ? "-" : ""), abs(simm), rn);
        R[rn] = simm;
    }

    void MOVWI(uint16 disp, uint16 rn) {
        disp <<= 1u;
        dbg_println("mov.w @(0x{:08X},pc), r{}", PC + 4 + disp, rn);
        R[rn] = bit::sign_extend<16>(MemReadWord(PC + 4 + disp));
    }

    void MOVLI(uint16 disp, uint16 rn) {
        disp <<= 2u;
        dbg_println("mov.l @(0x{:08X},pc), r{}", ((PC + 4) & ~3) + disp, rn);
        R[rn] = MemReadLong(((PC + 4) & ~3u) + disp);
    }

    void MOVT(uint16 rn) {
        dbg_println("movt r{}", rn);
        R[rn] = SR.T;
    }

    void MULL(uint16 rm, uint16 rn) {
        dbg_println("mul.l r{}, r{}", rm, rn);
        MAC.L = R[rm] * R[rn];
    }

    void MULS(uint16 rm, uint16 rn) {
        dbg_println("muls.w r{}, r{}", rm, rn);
        MAC.L = bit::sign_extend<16>(R[rm]) * bit::sign_extend<16>(R[rn]);
    }

    void MULU(uint16 rm, uint16 rn) {
        dbg_println("mulu.w r{}, r{}", rm, rn);
        auto cast = [](uint32 val) { return static_cast<uint32>(static_cast<uint16>(val)); };
        MAC.L = cast(R[rm]) * cast(R[rn]);
    }

    void NOP() {
        dbg_println("nop");
    }

    void NEG(uint16 rm, uint16 rn) {
        dbg_println("neg r{}, r{}", rm, rn);
        R[rn] = -R[rm];
    }

    void NEGC(uint16 rm, uint16 rn) {
        dbg_println("negc r{}, r{}", rm, rn);
        uint32 tmp = -R[rm];
        R[rn] = tmp - SR.T;
        SR.T = (0 < tmp) || (tmp < R[rn]);
    }

    void NOT(uint16 rm, uint16 rn) {
        dbg_println("not r{}, r{}", rm, rn);
        R[rn] = ~R[rm];
    }

    void OR(uint16 rm, uint16 rn) {
        dbg_println("or r{}, r{}", rm, rn);
        R[rn] |= R[rm];
    }

    void ORI(uint16 imm) {
        dbg_println("or #0x{:X}, r0", imm);
        R[0] |= imm;
    }

    void ORM(uint16 imm) {
        dbg_println("or.b #0x{:X}, @(r0,gbr)", imm);
        uint8 tmp = MemReadByte(GBR + R[0]);
        tmp |= imm;
        MemWriteByte(GBR + R[0], tmp);
    }

    void ROTCL(uint16 rn) {
        dbg_println("rotcl r{}", rn);
        uint16 tmp = R[rn] >> 31u;
        R[rn] = (R[rn] << 1u) | SR.T;
        SR.T = tmp;
    }

    void ROTCR(uint16 rn) {
        dbg_println("rotcr r{}", rn);
        uint16 tmp = R[rn] & 1u;
        R[rn] = (R[rn] >> 1u) | (SR.T << 31u);
        SR.T = tmp;
    }

    void ROTL(uint16 rn) {
        dbg_println("rotl r{}", rn);
        SR.T = R[rn] >> 31u;
        R[rn] = (R[rn] << 1u) | SR.T;
    }

    void ROTR(uint16 rn) {
        dbg_println("rotr r{}", rn);
        SR.T = R[rn] & 1u;
        R[rn] = (R[rn] >> 1u) | (SR.T << 31u);
    }

    void RTE() {
        dbg_println("rte");
        uint32 delaySlot = PC + 2;
        PC = MemReadLong(R[15] + 4);
        R[15] += 4;
        SR.u32 = MemReadLong(R[15]) & 0x000003F3;
        R[15] += 4;
        Execute<true>(delaySlot);
    }

    void RTS() {
        dbg_println("rts");
        uint32 delaySlot = PC + 2;
        PC = PR + 4;
        Execute<true>(delaySlot);
    }

    void SETT() {
        dbg_println("sett");
        SR.T = 1;
    }

    void SHAL(uint16 rn) {
        dbg_println("shal r{}", rn);
        SR.T = R[rn] >> 31u;
        R[rn] <<= 1u;
    }

    void SHAR(uint16 rn) {
        dbg_println("shar r{}", rn);
        SR.T = R[rn] & 1u;
        R[rn] = static_cast<sint32>(R[rn]) >> 1;
    }

    void SHLL(uint16 rn) {
        dbg_println("shll r{}", rn);
        SR.T = R[rn] >> 31u;
        R[rn] <<= 1u;
    }

    void SHLL2(uint16 rn) {
        dbg_println("shll2 r{}", rn);
        R[rn] <<= 2u;
    }

    void SHLL8(uint16 rn) {
        dbg_println("shll8 r{}", rn);
        R[rn] <<= 8u;
    }

    void SHLL16(uint16 rn) {
        dbg_println("shll16 r{}", rn);
        R[rn] <<= 16u;
    }

    void SHLR(uint16 rn) {
        dbg_println("shlr r{}", rn);
        SR.T = R[rn] & 1u;
        R[rn] >>= 1u;
    }

    void SHLR2(uint16 rn) {
        dbg_println("shlr2 r{}", rn);
        R[rn] >>= 2u;
    }

    void SHLR8(uint16 rn) {
        dbg_println("shlr8 r{}", rn);
        R[rn] >>= 8u;
    }

    void SHLR16(uint16 rn) {
        dbg_println("shlr16 r{}", rn);
        R[rn] >>= 16u;
    }

    void SLEEP() {
        dbg_println("sleep");
        PC -= 2;
        // TODO: wait for exception
    }

    void STCSR(uint16 rn) {
        dbg_println("stc sr, r{}", rn);
        R[rn] = SR.u32;
    }

    void STCGBR(uint16 rn) {
        dbg_println("stc gbr, r{}", rn);
        R[rn] = GBR;
    }

    void STCVBR(uint16 rn) {
        dbg_println("stc vbr, r{}", rn);
        R[rn] = VBR;
    }

    void STSMACH(uint16 rn) {
        dbg_println("sts mach, r{}", rn);
        R[rn] = MAC.H;
    }

    void STCMSR(uint16 rn) {
        dbg_println("stc.l sr, @-r{}", rn);
        R[rn] -= 4;
        MemWriteLong(R[rn], SR.u32);
    }

    void STCMGBR(uint16 rn) {
        dbg_println("stc.l gbr, @-r{}", rn);
        R[rn] -= 4;
        MemWriteLong(R[rn], GBR);
    }

    void STCMVBR(uint16 rn) {
        dbg_println("stc.l vbr, @-r{}", rn);
        R[rn] -= 4;
        MemWriteLong(R[rn], VBR);
    }

    void STSMACL(uint16 rn) {
        dbg_println("sts macl, r{}", rn);
        R[rn] = MAC.L;
    }

    void STSPR(uint16 rn) {
        dbg_println("sts pr, r{}", rn);
        R[rn] = PR;
    }

    void STSMMACH(uint16 rn) {
        dbg_println("sts.l mach, @-r{}", rn);
        R[rn] -= 4;
        MemWriteLong(R[rn], MAC.H);
    }

    void STSMMACL(uint16 rn) {
        dbg_println("sts.l macl, @-r{}", rn);
        R[rn] -= 4;
        MemWriteLong(R[rn], MAC.L);
    }

    void STSMPR(uint16 rn) {
        dbg_println("sts.l pr, @-r{}", rn);
        R[rn] -= 4;
        MemWriteLong(R[rn], PR);
    }

    void SUB(uint16 rm, uint16 rn) {
        dbg_println("sub r{}, r{}", rm, rn);
        R[rn] -= R[rm];
    }

    void SUBC(uint16 rm, uint16 rn) {
        dbg_println("subc r{}, r{}", rm, rn);
        uint32 tmp1 = R[rn] - R[rm];
        uint32 tmp0 = R[rn];
        R[rn] = tmp1 - SR.T;
        SR.T = (tmp0 < tmp1) || (tmp1 < R[rn]);
    }

    void SUBV(uint16 rm, uint16 rn) {
        dbg_println("subv r{}, r{}", rm, rn);

        bool dst = static_cast<sint32>(R[rn]) < 0;
        bool src = static_cast<sint32>(R[rm]) < 0;

        R[rn] -= R[rm];

        bool ans = static_cast<sint32>(R[rn]) < 0;
        ans ^= dst;
        SR.T = (src != dst) & ans;
    }

    void SWAPB(uint16 rm, uint16 rn) {
        dbg_println("swap.b r{}, r{}", rm, rn);

        uint32 tmp0 = R[rm] & 0xFFFF0000;
        uint32 tmp1 = (R[rm] & 0xFF) << 8u;
        R[rn] = ((R[rm] >> 8u) & 0xFF) | tmp1 | tmp0;
    }

    void SWAPW(uint16 rm, uint16 rn) {
        dbg_println("swap.w r{}, r{}", rm, rn);

        uint32 tmp = R[rm] >> 16u;
        R[rn] = (R[rm] << 16u) | tmp;
    }

    void TAS(uint16 rn) {
        dbg_println("tas.b @r{}", rn);
        dbg_println("WARNING: bus lock not implemented!");

        // TODO: enable bus lock on this read
        uint8 tmp = MemReadByte(R[rn]);
        SR.T = tmp == 0;
        // TODO: disable bus lock on this write
        MemWriteByte(R[rn], tmp | 0x80);
    }

    void TST(uint16 rm, uint16 rn) {
        dbg_println("tst r{}, r{}", rm, rn);
        SR.T = (R[rn] & R[rm]) == 0;
    }

    void TSTI(uint16 imm) {
        dbg_println("tst #0x{:X}, r0", imm);
        SR.T = (R[0] & imm) == 0;
    }

    void TSTM(uint16 imm) {
        dbg_println("tst.b #0x{:X}, @(r0,gbr)", imm);
        uint8 tmp = MemReadByte(GBR + R[0]);
        SR.T = (tmp & imm) == 0;
    }

    void TRAPA(uint16 imm) {
        dbg_println("trapa #0x{:X}", imm);
        R[15] -= 4;
        MemWriteLong(R[15], SR.u32);
        R[15] -= 4;
        MemWriteLong(R[15], PC - 2);
        PC = MemReadLong(VBR + (imm << 2)) + 4;
    }

    void XOR(uint16 rm, uint16 rn) {
        dbg_println("xor r{}, r{}", rm, rn);
        R[rn] ^= R[rm];
    }

    void XORI(uint16 imm) {
        dbg_println("xor #0x{:X}, r0", imm);
        R[0] ^= imm;
    }

    void XORM(uint16 imm) {
        dbg_println("xor.b #0x{:X}, @(r0,gbr)", imm);
        uint8 tmp = MemReadByte(GBR + R[0]);
        tmp ^= imm;
        MemWriteByte(GBR + R[0], tmp);
    }

    void XTRCT(uint16 rm, uint16 rn) {
        dbg_println("xtrct r{}, r{}", rm, rn);
        R[rn] = (R[rn] >> 16u) | (R[rm] << 16u);
    }
};

} // namespace satemu
