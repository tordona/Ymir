#include <satemu/satemu.hpp>

#include <fmt/format.h>

#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

static_assert(std::endian::native == std::endian::little, "big-endian platforms are not supported at this moment");

// -----------------------------------------------------------------------------
// unreachable.hpp

namespace util {

[[noreturn]] inline void unreachable() {
#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
    __builtin_unreachable();
#elif defined(_MSC_VER)
    __assume(0);
#endif
}

} // namespace util

// -----------------------------------------------------------------------------
// types.hpp

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

using sint8 = int8_t;
using sint16 = int16_t;
using sint32 = int32_t;
using sint64 = int64_t;

// -----------------------------------------------------------------------------
// signextend.hpp

template <unsigned B, std::integral T>
constexpr auto SignExtend(T x) {
    using ST = std::make_signed_t<T>;
    struct {
        ST x : B;
    } s{static_cast<ST>(x)};
    return s.x;
}

// -----------------------------------------------------------------------------
// bit_ops.hpp

namespace bit {

// Extracts a range of bits from the value.
// start and end are both inclusive.
template <size_t start, size_t end = start, typename T>
constexpr T extract(T value) {
    static_assert(start < sizeof(T) * 8, "start out of range");
    static_assert(end < sizeof(T) * 8, "end out of range");
    static_assert(end >= start, "end cannot be before start");

    using UT = std::make_unsigned_t<T>;

    constexpr size_t length = end - start + 1;
    constexpr UT mask = static_cast<UT>(~(~0 << length));
    return (value >> start) & mask;
}

} // namespace bit

// -----------------------------------------------------------------------------
// size_ops.hpp

constexpr size_t operator""_KiB(size_t sz) {
    return sz * 1024;
}

// -----------------------------------------------------------------------------
// smpc.hpp

class SMPC {
public:
    SMPC() {
        Reset(true);
    }

    void Reset(bool hard) {
        IREG.fill(0x00);
        OREG.fill(0x00);
        COMREG = Command::None;
        SR.u8 = 0x80;
        SF = false;

        m_busValue = 0x00;
    }

    uint8 Read(uint32 address) {
        switch (address) {
        case 0x21 ... 0x5F: return ReadOREG((address - 0x20) >> 1);
        case 0x61: return ReadSR();
        case 0x63: return ReadSF();
        default: fmt::println("unhandled SMPC read from {:02X}", address); return m_busValue;
        }
    }

    void Write(uint32 address, uint8 value) {
        m_busValue = value;
        switch (address) {
        case 0x01 ... 0x0D: WriteIREG(address >> 1, value); break;
        case 0x1F: WriteCOMREG(value); break;
        case 0x63: WriteSF(value); break;
        default: fmt::println("unhandled SMPC write to {:02X} = {:02X}", address, value); break;
        }
    }

private:
    std::array<uint8, 7> IREG;
    std::array<uint8, 32> OREG;

    enum class Command : uint8 {
        // Resetable system management commands
        MSHON = 0x00,    // Master SH-2 ON
        SSHON = 0x02,    // Slave SH-2 ON
        SSHOFF = 0x03,   // Slave SH-2 OFF
        SNDON = 0x06,    // Sound CPU ON (MC68EC000)
        SNDOFF = 0x07,   // Sound CPU OFF (MC68EC000)
        CDON = 0x08,     // CD ON
        CDOFF = 0x09,    // CD OFF
        SYSRES = 0x0D,   // Entire System Reset
        CKCHG352 = 0x0E, // Clock Change 352 Mode
        CKCHG320 = 0x0F, // Clock Change 320 Mode
        NMIREQ = 0x18,   // NMI Request
        RESENAB = 0x19,  // Reset Enable
        RESDISA = 0x1A,  // Reset Disable

        // Non-resetable system management commands
        INTBACK = 0x10, // Interrupt Back (SMPC Status Acquisition)
        SETSMEM = 0x17, // SMPC Memory Setting

        // RTC commands
        SETTIME = 0x16, // Time Setting

        None = 0xFF,
    };

    Command COMREG;

    // bits   r/w  code     description
    //    7   R    -        ??
    //    6   R    PDL      Peripheral Data Location bit (0=2nd+, 1=1st)
    //    5   R    NPE      Remaining Peripheral Existence bit (0=no remaining data, 1=more remaining data)
    //    4   R    RESB     Reset button status (0=released, 1=pressed)
    //  3-2   R    P2MD0-1  Port 2 Mode
    //                        00: 15-byte mode
    //                        01: 255-byte mode
    //                        10: Unused
    //                        11: 0-byte mode
    //  1-0   R    P1MD0-1  Port 1 Mode
    //                        00: 15-byte mode
    //                        01: 255-byte mode
    //                        10: Unused
    //                        11: 0-byte mode
    union SR_t {
        uint8 u8;
        struct {
            uint8 P1MDn : 2;
            uint8 P2MDn : 2;
            uint8 RESB : 1;
            uint8 NPE : 1;
            uint8 PDL : 1;
            uint8 bit7 : 1;
        };
    } SR;

    bool SF;

    uint8 m_busValue;

    uint8 ReadOREG(uint8 offset) const {
        return OREG[offset & 31];
    }

    uint8 ReadSR() const {
        return SR.u8;
    }

    uint8 ReadSF() const {
        return SF;
    }

    void WriteIREG(uint8 offset, uint8 value) {
        assert(offset < 7);
        IREG[offset] = value;
    }

    void WriteCOMREG(uint8 value) {
        COMREG = static_cast<Command>(value);

        // TODO: should delay execution
        switch (COMREG) {
        case Command::INTBACK:
            fmt::println("INTBACK command received: {:02X} {:02X} {:02X}", IREG[0], IREG[1], IREG[2]);
            INTBACK();
            break;
        default: fmt::println("unhandled SMPC command {:02X}", static_cast<uint8>(COMREG)); break;
        }
    }

    void WriteSF(uint8 value) {
        SF = true;
    }

    // -------------------------------------------------------------------------
    // Commands

    void INTBACK() {
        // const bool getSMPCStatus = IREG[0];
        // const bool optimize = bit::extract<1>(IREG[1]);
        // const bool getPeripheralData = bit::extract<3>(IREG[1]);
        // const uint8 port1mode = bit::extract<4, 5>(IREG[1]);
        // const uint8 port2mode = bit::extract<6, 7>(IREG[1]);
        // IREG[2] == 0xF0;

        SR.bit7 = 0; // fixed 0
        SR.PDL = 1;  // fixed 1
        SR.NPE = 0;  // 0=no remaining data, 1=more data
        SR.RESB = 0; // reset button state (0=off, 1=on)

        SF = 0; // done processing

        OREG[0] = 0x80; // STE set, RESD clear

        OREG[1] = 0x20; // Year 1000s, Year 100s (BCD)
        OREG[2] = 0x24; // Year 10s, Year 1s (BCD)
        OREG[3] = 0x3B; // Day of week (0=sun), Month (hex, 1=jan)
        OREG[4] = 0x20; // Day (BCD)
        OREG[5] = 0x12; // Hour (BCD)
        OREG[6] = 0x34; // Minute (BCD)
        OREG[7] = 0x56; // Second (BCD)

        OREG[8] = 0x00; // Cartridge code (CTG1-0) == 0b00
        OREG[9] = 0x04; // Area code (0x04=NA)

        OREG[10] = 0b00111110; // System status 1 (DOTSEL, MSHNMI, SYSRES, SNDRES)
        OREG[11] = 0b00000010; // System status 2 (CDRES)

        OREG[12] = 0x00; // SMEM 1 Saved Data
        OREG[13] = 0x00; // SMEM 2 Saved Data
        OREG[14] = 0x00; // SMEM 3 Saved Data
        OREG[15] = 0x00; // SMEM 4 Saved Data

        OREG[31] = 0x00;
    }
};

// -----------------------------------------------------------------------------
// sh2_bus.hpp

constexpr size_t kIPLSize = 512_KiB;
constexpr size_t kWRAMLowSize = 1024_KiB;
constexpr size_t kWRAMHighSize = 1024_KiB;

template <typename T>
concept mem_access_type = std::same_as<T, uint8> || std::same_as<T, uint16> || std::same_as<T, uint32>;

// SH-2 memory map
// https://wiki.yabause.org/index.php5?title=SH-2CPU
//
// Address range            Mirror size       Description
// 0x00000000..0x000FFFFF   0x80000           Boot ROM / IPL
// 0x00100000..0x0017FFFF   0x80              SMPC registers
// 0x00180000..0x001FFFFF   0x10000           Backup RAM
// 0x00200000..0x002FFFFF   0x100000          Work RAM Low
// 0x00300000..0x003FFFFF   -                 Open bus? (reads random data, mostly 0x00)
// 0x00400000..0x007FFFFF   -                 Reads 0x0000
// 0x00800000..0x00FFFFFF   -                 Reads 0x0000 0x0001 0x0002 0x0003 0x0004 0x0005 0x0006 0x0007
// 0x01000000..0x017FFFFF   -                 Reads 0xFFFF; writes go to slave SH-2 FRT  (MINIT area)
// 0x01800000..0x01FFFFFF   -                 Reads 0xFFFF; writes go to master SH-2 FRT (SINIT area)
// 0x02000000..0x03FFFFFF   -                 A-Bus CS0
// 0x04000000..0x04FFFFFF   -                 A-Bus CS1
// 0x05000000..0x057FFFFF   -                 A-Bus Dummy
// 0x05800000..0x058FFFFF   -                 A-Bus CS2 (includes CD-ROM registers)
// 0x05900000..0x059FFFFF   -                 Lockup when read
// 0x05A00000..0x05AFFFFF   0x40000/0x80000   68000 Work RAM
// 0x05B00000..0x05BFFFFF   0x1000            SCSP registers
// 0x05C00000..0x05C7FFFF   0x80000           VDP1 VRAM
// 0x05C80000..0x05CFFFFF   0x40000           VDP1 Framebuffer (backbuffer only)
// 0x05D00000..0x05D7FFFF   0x18 (no mirror)  VDP1 Registers
// 0x05D80000..0x05DFFFFF   -                 Lockup when read
// 0x05E00000..0x05EFFFFF   0x80000           VDP2 VRAM
// 0x05F00000..0x05F7FFFF   0x1000            VDP2 CRAM
// 0x05F80000..0x05FBFFFF   0x200             VDP2 registers
// 0x05FC0000..0x05FDFFFF   -                 Reads 0x000E0000
// 0x05FE0000..0x05FEFFFF   0x100             SCU registers
// 0x05FF0000..0x05FFFFFF   0x100             Unknown registers
// 0x06000000..0x07FFFFFF   0x100000          Work RAM High
//
// Notes
// - Unless otherwise specified, all regions are mirrored across the designated area
// - Backup RAM
//   - Only odd bytes mapped
//   - Reads from even bytes return 0xFF
//   - Writes to even bytes map to correspoding odd byte
// - 68000 Work RAM
//   - Area size depends on MEM4MB bit setting:
//       0=only first 256 KiB are used/mirrored
//       1=all 512 KiB are used/mirrored
// - VDP2 CRAM
//   - Byte writes write garbage to the odd/even byte counterpart
//   - Byte reads work normally
class SH2Bus {
public:
    SH2Bus(SMPC &smpc)
        : m_SMPC(smpc) {
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

    template <mem_access_type T>
    T Read(uint32 address) {
        if constexpr (std::is_same_v<T, uint8>) {
            return ReadByte(address);
        } else if constexpr (std::is_same_v<T, uint16>) {
            return ReadWord(address);
        } else if constexpr (std::is_same_v<T, uint32>) {
            return ReadLong(address);
        }
    }

    template <mem_access_type T>
    void Write(uint32 address, T value) {
        if constexpr (std::is_same_v<T, uint8>) {
            WriteByte(address, value);
        } else if constexpr (std::is_same_v<T, uint16>) {
            WriteWord(address, value);
        } else if constexpr (std::is_same_v<T, uint32>) {
            WriteLong(address, value);
        }
    }

    uint8 ReadByte(uint32 baseAddress) {
        uint32 address = baseAddress & 0x7FFFFFF;

        if (address <= 0x000FFFFF) {
            address &= 0x7FFFF;
            return m_IPL[address];
        } else if (address - 0x100000 <= 0x0007FFFF) {
            address &= 0x7F;
            return m_SMPC.Read(address | 1);
        } else if (address - 0x200000 <= 0x000FFFFF) {
            address &= 0xFFFFF;
            return m_WRAMLow[address];
        } else if (address - 0x5FE0000 <= 0x0000FFFF) {
            address &= 0xFF;
            return SCUReadByte(address);
        } else if (address - 0x6000000 <= 0x01FFFFFF) {
            address &= 0xFFFFF;
            return m_WRAMHigh[address];
        } else {
            fmt::println("unhandled SH2 bus 8-bit read from {:08X}", baseAddress);
            return 0;
        }
    }

    uint16 ReadWord(uint32 baseAddress) {
        uint32 address = baseAddress & 0x7FFFFFE;

        if (address <= 0x000FFFFF) {
            address &= 0x7FFFF;
            return (m_IPL[address + 0] << 8u) | m_IPL[address + 1];
        } else if (address - 0x100000 <= 0x0007FFFF) {
            address &= 0x7F;
            return 0xFF00 | m_SMPC.Read(address | 1);
        } else if (address - 0x200000 <= 0x000FFFFF) {
            address &= 0xFFFFF;
            return (m_WRAMLow[address + 0] << 8u) | m_WRAMLow[address + 1];
        } else if (address - 0x5FE0000 <= 0x0000FFFF) {
            address &= 0xFF;
            return SCUReadWord(address);
        } else if (address - 0x6000000 <= 0x01FFFFFF) {
            address &= 0xFFFFF;
            return (m_WRAMHigh[address + 0] << 8u) | m_WRAMHigh[address + 1];
        } else {
            fmt::println("unhandled SH2 bus 16-bit read from {:08X}", baseAddress);
            return 0;
        }
    }

    uint32 ReadLong(uint32 baseAddress) {
        uint32 address = baseAddress & 0x7FFFFFC;

        if (address <= 0x000FFFFF) {
            address &= 0x7FFFF;
            return (m_IPL[address + 0] << 24u) | (m_IPL[address + 1] << 16u) | (m_IPL[address + 2] << 8u) |
                   m_IPL[address + 3];
        } else if (address - 0x200000 <= 0x000FFFFF) {
            address &= 0xFFFFF;
            return (m_WRAMLow[address + 0] << 24u) | (m_WRAMLow[address + 1] << 16u) | (m_WRAMLow[address + 2] << 8u) |
                   m_WRAMLow[address + 3];
        } else if (address - 0x5FE0000 <= 0x0000FFFF) {
            address &= 0xFF;
            return SCUReadLong(address);
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
        uint32 address = baseAddress & 0x7FFFFFF;

        if (address - 0x100000 <= 0x0007FFFF && (address & 1)) {
            if (address & 1) {
                address &= 0x7F;
                m_SMPC.Write(address, value);
            }
        } else if (address - 0x200000 <= 0x000FFFFF) {
            address &= 0xFFFFF;
            m_WRAMLow[address] = value;
        } else if (address - 0x5FE0000 <= 0x0000FFFF) {
            address &= 0xFF;
            SCUWriteByte(address, value);
        } else if (address - 0x6000000 <= 0x01FFFFFF) {
            address &= 0xFFFFF;
            m_WRAMHigh[address] = value;
        } else {
            fmt::println("unhandled SH2 bus 8-bit write to {:08X} = {:02X}", baseAddress, value);
        }
    }

    void WriteWord(uint32 baseAddress, uint16 value) {
        uint32 address = baseAddress & 0x7FFFFFE;

        if (address - 0x100000 <= 0x0007FFFF) {
            address &= 0x7F;
            m_SMPC.Write(address, value);
        } else if (address - 0x200000 <= 0x000FFFFF) {
            address &= 0xFFFFF;
            m_WRAMLow[address + 0] = value >> 8u;
            m_WRAMLow[address + 1] = value >> 0u;
        } else if (address - 0x5FE0000 <= 0x0000FFFF) {
            address &= 0xFF;
            SCUWriteWord(address, value);
        } else if (address - 0x6000000 <= 0x01FFFFFF) {
            address &= 0xFFFFF;
            m_WRAMHigh[address + 0] = value >> 8u;
            m_WRAMHigh[address + 1] = value >> 0u;
        } else {
            fmt::println("unhandled SH2 bus 16-bit write to {:08X} = {:04X}", baseAddress, value);
        }
    }

    void WriteLong(uint32 baseAddress, uint32 value) {
        uint32 address = baseAddress & 0x7FFFFFC;

        if (address - 0x200000 <= 0x000FFFFF) {
            address &= 0xFFFFF;
            m_WRAMLow[address + 0] = value >> 24u;
            m_WRAMLow[address + 1] = value >> 16u;
            m_WRAMLow[address + 2] = value >> 8u;
            m_WRAMLow[address + 3] = value >> 0u;
        } else if (address - 0x5FE0000 <= 0x0000FFFF) {
            address &= 0xFF;
            SCUWriteLong(address, value);
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

    SMPC &m_SMPC;

    uint8 SCUReadByte(uint32 address) {
        fmt::println("unhandled SCU 8-bit read from {:08X}", address);
        return 0;
    }

    uint16 SCUReadWord(uint32 address) {
        fmt::println("unhandled SCU 16-bit read from {:08X}", address);
        return 0;
    }

    uint32 SCUReadLong(uint32 address) {
        fmt::println("unhandled SCU 32-bit read from {:08X}", address);
        return 0;
    }

    void SCUWriteByte(uint32 address, uint8 value) {
        fmt::println("unhandled SCU 8-bit write to {:08X} = {:02X}", address, value);
    }

    void SCUWriteWord(uint32 address, uint16 value) {
        fmt::println("unhandled SCU 16-bit write to {:08X} = {:04X}", address, value);
    }

    void SCUWriteLong(uint32 address, uint32 value) {
        fmt::println("unhandled SCU 32-bit write to {:08X} = {:08X}", address, value);
    }
};

// -----------------------------------------------------------------------------
// sh2.hpp

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
            uint32 H;
            uint32 L;
        };
    } MAC;

    SH2Bus &m_bus;

    uint64 dbg_count = 0;
    static constexpr uint64 dbg_minCount = 9302150; // 9547530;

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
            return m_bus.Read<T>(address);
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
                if ((address & 0x100) == 0 && (address & 0x10001000) != 0x10001000) {
                    return OpenBusSeqRead<T>(address);
                } else {
                    return OnChipRegRead<T>(address & 0x1FF);
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
            m_bus.Write<T>(address, value);
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

    // Returns 00 00 00 01 00 02 00 03 00 04 00 05 00 06 00 07
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
                    STCSR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x03: // 0000 mmmm 0000 0011   BSRF Rm
                    if constexpr (delaySlot) {
                        // TODO: illegal instruction
                        dbg_println("illegal delay slot instruction");
                    } else {
                        BSRF((instr >> 8u) & 0xF);
                    }
                    break;
                case 0x0A: // 0000 nnnn 0000 1010   STS MACH, Rn
                    STSMACH((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x12: // 0000 nnnn 0001 0010   STC GBR, Rn
                    STCGBR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x1A: // 0000 nnnn 0001 1010   STS MACL, Rn
                    STSMACL((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x22: // 0000 nnnn 0010 0010   STC VBR, Rn
                    STCVBR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x23: // 0000 mmmm 0010 0011   BRAF Rm
                    if constexpr (delaySlot) {
                        // TODO: illegal instruction
                        dbg_println("illegal delay slot instruction");
                    } else {
                        BRAF((instr >> 8u) & 0xF);
                    }
                    break;
                case 0x29: // 0000 nnnn 0010 1001   MOVT Rn
                    MOVT((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x2A: // 0000 nnnn 0010 1010   STS PR, Rn
                    STSPR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                default:
                    switch (instr & 0xF) {
                    case 0x4: // 0000 nnnn mmmm 0100   MOV.B Rm, @(R0,Rn)
                        MOVBS0((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                    case 0x5: // 0000 nnnn mmmm 0101   MOV.W Rm, @(R0,Rn)
                        MOVWS0((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                    case 0x6: // 0000 nnnn mmmm 0110   MOV.L Rm, @(R0,Rn)
                        MOVLS0((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                        // TODO: case 0x7: // 0000 nnnn mmmm 0111   MUL.L Rm, Rn
                    case 0xC: // 0000 nnnn mmmm 1100   MOV.B @(R0,Rm), Rn
                        MOVBL0((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                    case 0xD: // 0000 nnnn mmmm 1101   MOV.W @(R0,Rm), Rn
                        MOVWL0((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                    case 0xE: // 0000 nnnn mmmm 1110   MOV.L @(R0,Rm), Rn
                        MOVLL0((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                        if constexpr (!delaySlot) {
                            PC += 2;
                        }
                        break;
                        // TODO: case 0xF: // 0000 nnnn mmmm 1111   MAC.L @Rm, @Rn+

                    default: dbg_println("unhandled 0000 instruction"); break;
                    }
                    break;
                }
                break;
            }
            break;
        case 0x1: // 0001 nnnn mmmm dddd   MOV.L Rm, @(disp,Rn)
            MOVLS4((instr >> 4u) & 0xF, instr & 0xF, (instr >> 8u) & 0xF);
            if constexpr (!delaySlot) {
                PC += 2;
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
            // TODO: case 0xE: // 0010 nnnn mmmm 1110   MULU.W Rm, Rn
            // TODO: case 0xF: // 0010 nnnn mmmm 1111   MULS.W Rm, Rn
            default: dbg_println("unhandled 0010 instruction"); break;
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
            case 0x2: // 0011 nnnn mmmm 0010   CMP/HS Rm, Rn
                CMPHS((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x3: // 0011 nnnn mmmm 0011   CMP/GE Rm, Rn
                CMPGE((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
                // TODO: case 0x4: // 0011 nnnn mmmm 0100   DIV1 Rm, Rn
                // TODO: case 0x5: // 0011 nnnn mmmm 0101   DMULU.L Rm, Rn
            case 0x6: // 0011 nnnn mmmm 0110   CMP/HI Rm, Rn
                CMPHI((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x7: // 0011 nnnn mmmm 0111   CMP/GT Rm, Rn
                CMPGT((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x8: // 0011 nnnn mmmm 1000   SUB Rm, Rn
                SUB((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x9: // 0011 nnnn mmmm 1001   SUBC Rm, Rn
                SUBC((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xA: // 0011 nnnn mmmm 1010   SUBV Rm, Rn
                SUBV((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;

                // There's no case 0xB

            case 0xC: // 0011 nnnn mmmm 1100   ADD Rm, Rn
                ADD((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            // TODO: case 0xD: // 0011 nnnn mmmm 1101   DMULS.L Rm, Rn
            case 0xE: // 0011 nnnn mmmm 1110   ADDC Rm, Rn
                ADDC((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xF: // 0011 nnnn mmmm 1110   ADDV Rm, Rn
                ADDV((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
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
                // TODO: implement
                dbg_println("unhandled MAC.W instruction");
            } else {
                switch (instr & 0xFF) {
                case 0x00: // 0100 nnnn 0000 0000   SHLL Rn
                    SHLL((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x01: // 0100 nnnn 0000 0001   SHLR Rn
                    SHLR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x02: // 0100 nnnn 0000 0010   STS.L MACH, @-Rn
                    STSMMACH((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x03: // 0100 nnnn 0000 0010   STC.L SR, @-Rn
                    STCMSR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x04: // 0100 nnnn 0000 0100   ROTL Rn
                    ROTL((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x05: // 0100 nnnn 0000 0101   ROTR Rn
                    ROTR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x06: // 0100 mmmm 0000 0110   LDS.L @Rm+, MACH
                    LDSMMACH((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x07: // 0100 mmmm 0000 0111   LDC.L @Rm+, SR
                    LDCMSR((instr >> 8u) & 0xF);
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
                case 0x09: // 0100 nnnn 0000 1001   SHLR2 Rn
                    SHLR2((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x0A: // 0100 mmmm 0000 1010   LDS Rm, MACH
                    LDSMACH((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x0B: // 0110 mmmm 0000 1110   JSR @Rm
                    if constexpr (delaySlot) {
                        // TODO: illegal instruction
                        dbg_println("illegal delay slot instruction");
                    } else {
                        JSR((instr >> 8u) & 0xF);
                    }
                    break;

                    // There's no case 0x0C or 0x0D

                case 0x0E: // 0110 mmmm 0000 1110   LDC Rm, SR
                    LDCSR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;

                    // There's no case 0x0F

                case 0x10: // 0100 nnnn 0001 0000   DT Rn
                    DT((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x11: // 0100 nnnn 0001 0001   CMP/PZ Rn
                    CMPPZ((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x12: // 0100 nnnn 0001 0010   STS.L MACL, @-Rn
                    STSMMACL((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x13: // 0100 nnnn 0001 0011   STC.L GBR, @-Rn
                    STCMGBR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;

                    // There's no case 0x14

                case 0x15: // 0100 nnnn 0001 0101   CMP/PL Rn
                    CMPPL((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x16: // 0100 mmmm 0001 0110   LDS.L @Rm+, MACL
                    LDSMMACL((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x17: // 0100 mmmm 0001 0111   LDC.L @Rm+, GBR
                    LDCMGBR((instr >> 8u) & 0xF);
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
                case 0x19: // 0100 nnnn 0001 1001   SHLR8 Rn
                    SHLR8((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x1A: // 0100 mmmm 0001 1010   LDS Rm, MACL
                    LDSMACL((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x1B: // 0100 nnnn 0001 1011   TAS.B @Rn
                    TAS((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;

                    // There's no case 0x1C or 0x1D

                case 0x1E: // 0110 mmmm 0001 1110   LDC Rm, GBR
                    LDCGBR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;

                    // There's no case 0x1F

                case 0x20: // 0100 nnnn 0010 0000   SHAL Rn
                    SHAL((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x21: // 0100 nnnn 0010 0001   SHAR Rn
                    SHAR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x22: // 0100 nnnn 0010 0010   STS.L PR, @-Rn
                    STSMPR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x23: // 0100 nnnn 0010 0011   STC.L VBR, @-Rn
                    STCMVBR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x24: // 0100 nnnn 0010 0100   ROTCL Rn
                    ROTCL((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x25: // 0100 nnnn 0010 0101   ROTCR Rn
                    ROTCR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x26: // 0100 mmmm 0010 0110   LDS.L @Rm+, PR
                    LDSMPR((instr >> 8u) & 0xF);
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x27: // 0100 mmmm 0010 0111   LDC.L @Rm+, VBR
                    LDCMVBR((instr >> 8u) & 0xF);
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
                case 0x29: // 0100 nnnn 0010 1001   SHLR16 Rn
                    SHLR16((instr >> 8u) & 0xF);
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
                case 0x2B: // 0100 mmmm 0010 1011   JMP @Rm
                    if constexpr (delaySlot) {
                        // TODO: illegal instruction
                        dbg_println("illegal delay slot instruction");
                    } else {
                        JMP((instr >> 8u) & 0xF);
                    }
                    break;

                    // There's no case 0x2C or 0x2D

                case 0x2E: // 0110 mmmm 0010 1110   LDC Rm, VBR
                    LDCVBR((instr >> 8u) & 0xF);
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
            MOVLL4((instr >> 4) & 0xF, instr & 0xF, (instr >> 8) & 0xF);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x6:
            switch (instr & 0xF) {
            case 0x0: // 0110 nnnn mmmm 0000   MOV.B @Rm, Rn
                MOVBL((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x1: // 0110 nnnn mmmm 0001   MOV.W @Rm, Rn
                MOVWL((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x2: // 0110 nnnn mmmm 0010   MOV.L @Rm, Rn
                MOVLL((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x3: // 0110 nnnn mmmm 0010   MOV Rm, Rn
                MOV((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x4: // 0110 nnnn mmmm 0110   MOV.B @Rm+, Rn
                MOVBP((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x5: // 0110 nnnn mmmm 0110   MOV.W @Rm+, Rn
                MOVWP((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
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
            case 0x7: // 0110 nnnn mmmm 0111   NOT Rm, Rn
                NOT((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x8: // 0110 nnnn mmmm 1000   SWAP.B Rm, Rn
                SWAPB((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x9: // 0110 nnnn mmmm 1001   SWAP.W Rm, Rn
                SWAPW((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xA: // 0110 nnnn mmmm 1010   NEGC Rm, Rn
                NEGC((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xB: // 0110 nnnn mmmm 1011   NEG Rm, Rn
                NEG((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
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
            case 0xE: // 0110 nnnn mmmm 1110   EXTS.B Rm, Rn
                EXTSB((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xF: // 0110 nnnn mmmm 1111   EXTS.W Rm, Rn
                EXTSW((instr >> 4u) & 0xF, (instr >> 8u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
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
            case 0x0: // 1000 0000 nnnn dddd   MOV.B R0, @(disp,Rn)
                MOVBS4(instr & 0xF, (instr >> 4u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x1: // 1000 0001 nnnn dddd   MOV.W R0, @(disp,Rn)
                MOVWS4(instr & 0xF, (instr >> 4u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;

                // There's no case 0x2 or 0x3

            case 0x4: // 1000 0100 mmmm dddd   MOV.B @(disp,Rm), R0
                MOVBL4(instr & 0xF, (instr >> 4u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x5: // 1000 0101 mmmm dddd   MOV.W @(disp,Rm), R0
                MOVWL4(instr & 0xF, (instr >> 4u) & 0xF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;

                // There's no case 0x6 or 0x7

            case 0x8: // 1000 1000 iiii iiii   CMP/EQ #imm, R0
                CMPIM(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x9: // 1000 1001 dddd dddd   BT <label>
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    dbg_println("illegal delay slot instruction");
                } else {
                    BT(instr & 0xFF);
                }
                break;

                // There's no case 0xA

            case 0xB: // 1000 1011 dddd dddd   BF <label>
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    dbg_println("illegal delay slot instruction");
                } else {
                    BF(instr & 0xFF);
                }
                break;

                // There's no case 0xC

            case 0xD: // 1000 1101 dddd dddd   BT/S <label>
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    dbg_println("illegal delay slot instruction");
                } else {
                    BTS(instr & 0xFF);
                }
                break;

                // There's no case 0xE

            case 0xF: // 1000 1111 dddd dddd   BF/S <label>
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    dbg_println("illegal delay slot instruction");
                } else {
                    BFS(instr & 0xFF);
                }
                break;
            default: dbg_println("unhandled 1000 instruction"); break;
            }
            break;
        case 0x9: // 1001 nnnn dddd dddd   MOV.W @(disp,PC), Rn
            MOVWI(instr & 0xFF, (instr >> 8) & 0xF);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xA: // 1011 dddd dddd dddd   BRA <label>
            if constexpr (delaySlot) {
                // TODO: illegal instruction
                dbg_println("illegal delay slot instruction");
            } else {
                BRA(instr & 0xFFF);
            }
            break;
        case 0xB: // 1011 dddd dddd dddd   BSR <label>
            if constexpr (delaySlot) {
                // TODO: illegal instruction
                dbg_println("illegal delay slot instruction");
            } else {
                BSR(instr & 0xFFF);
            }
            break;
        case 0xC:
            switch ((instr >> 8u) & 0xF) {
            case 0x0: // 1100 0000 dddd dddd   MOV.B R0, @(disp,GBR)
                MOVBSG(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x1: // 1100 0001 dddd dddd   MOV.W R0, @(disp,GBR)
                MOVWSG(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x2: // 1100 0010 dddd dddd   MOV.L R0, @(disp,GBR)
                MOVLSG(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x3: // 1100 0011 iiii iiii   TRAPA #imm
                if constexpr (delaySlot) {
                    // TODO: illegal instruction
                    dbg_println("illegal delay slot instruction");
                } else {
                    TRAPA(instr & 0xFF);
                }
                break;
            case 0x4: // 1100 0100 dddd dddd   MOV.B @(disp,GBR), R0
                MOVBLG(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x5: // 1100 0101 dddd dddd   MOV.W @(disp,GBR), R0
                MOVWLG(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x6: // 1100 0110 dddd dddd   MOV.L @(disp,GBR), R0
                MOVLLG(instr & 0xFF);
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
            case 0x8: // 1100 1000 iiii iiii   TST #imm, R0
                TSTI(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x9: // 1100 1001 iiii iiii   AND #imm, R0
                ANDI(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xA: // 1100 1010 iiii iiii   XOR #imm, R0
                XORI(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xB: // 1100 1011 iiii iiii   OR #imm, R0
                ORI(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xC: // 1100 1100 iiii iiii   TST.B #imm, @(R0,GBR)
                TSTM(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xD: // 1100 1001 iiii iiii   AND #imm, @(R0,GBR)
                ANDM(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xE: // 1100 1001 iiii iiii   XOR #imm, @(R0,GBR)
                XORM(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0xF: // 1100 1001 iiii iiii   OR #imm, @(R0,GBR)
                ORM(instr & 0xFF);
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            default: dbg_println("unhandled 1100 instruction"); break;
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

            // There's no case 0xF

        default: dbg_println("unhandled instruction"); break;
        }
    }

    void ADD(uint16 rm, uint16 rn) {
        dbg_println("add r{}, r{}", rm, rn);
        R[rn] += R[rm];
    }

    void ADDI(uint16 imm, uint16 rn) {
        sint32 simm = SignExtend<8>(imm);
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
        sint32 sdisp = (SignExtend<8>(disp) << 1) + 4;
        dbg_println("bf 0x{:08X}", PC + sdisp);

        if (!SR.T) {
            PC += sdisp;
        } else {
            PC += 2;
        }
    }

    void BFS(uint16 disp) {
        sint32 sdisp = (SignExtend<8>(disp) << 1) + 4;
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
        sint32 sdisp = (SignExtend<12>(disp) << 1) + 4;
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
        sint32 sdisp = (SignExtend<12>(disp) << 1) + 4;
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
        sint32 sdisp = (SignExtend<8>(disp) << 1) + 4;
        dbg_println("bt 0x{:08X}", PC + sdisp);

        if (SR.T) {
            PC += sdisp;
        } else {
            PC += 2;
        }
    }

    void BTS(uint16 disp) {
        sint32 sdisp = (SignExtend<8>(disp) << 1) + 4;
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
        SR.T = static_cast<sint16>(R[rn]) >= static_cast<sint16>(R[rm]);
    }

    void CMPGT(uint16 rm, uint16 rn) {
        dbg_println("cmp/gt r{}, r{}", rm, rn);
        SR.T = static_cast<sint16>(R[rn]) > static_cast<sint16>(R[rm]);
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
        sint32 simm = SignExtend<8>(imm);
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
        SR.M = static_cast<sint16>(R[rm]) < 0;
        SR.Q = static_cast<sint16>(R[rn]) < 0;
        SR.T = SR.M != SR.Q;
    }

    void DIV0U() {
        dbg_println("div0u");
        SR.M = 0;
        SR.Q = 0;
        SR.T = 0;
    }

    void DT(uint16 rn) {
        dbg_println("dt r{}", rn);
        R[rn]--;
        SR.T = R[rn] == 0;
    }

    void EXTSB(uint16 rm, uint16 rn) {
        dbg_println("exts.b r{}, r{}", rm, rn);
        R[rn] = SignExtend<8>(R[rm]);
    }

    void EXTSW(uint16 rm, uint16 rn) {
        dbg_println("exts.w r{}, r{}", rm, rn);
        R[rn] = SignExtend<16>(R[rm]);
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

    void MOVA(uint16 disp) {
        disp = (disp << 2u) + 4;
        dbg_println("mova @(0x{:X},pc), r0", (PC & ~3) + disp);
        R[0] = (PC & ~3) + disp;
    }

    void MOVBL(uint16 rm, uint16 rn) {
        dbg_println("mov.b @r{}, r{}", rm, rn);
        R[rn] = SignExtend<8>(MemReadByte(R[rm]));
    }

    void MOVWL(uint16 rm, uint16 rn) {
        dbg_println("mov.w @r{}, r{}", rm, rn);
        R[rn] = SignExtend<16>(MemReadWord(R[rm]));
    }

    void MOVLL(uint16 rm, uint16 rn) {
        dbg_println("mov.l @r{}, r{}", rm, rn);
        R[rn] = MemReadLong(R[rm]);
    }

    void MOVBL0(uint16 rm, uint16 rn) {
        dbg_println("mov.b @(r0,r{}), r{}", rm, rn);
        R[rn] = SignExtend<8>(MemReadByte(R[rm] + R[0]));
    }

    void MOVWL0(uint16 rm, uint16 rn) {
        dbg_println("mov.w @(r0,r{}), r{})", rm, rn);
        R[rn] = SignExtend<16>(MemReadWord(R[rm] + R[0]));
    }

    void MOVLL0(uint16 rm, uint16 rn) {
        dbg_println("mov.l @(r0,r{}), r{})", rm, rn);
        R[rn] = MemReadLong(R[rm] + R[0]);
    }

    void MOVBL4(uint16 rm, uint16 disp) {
        dbg_println("mov.b @(0x{:X},r{}), r0", disp, rm);
        R[0] = SignExtend<8>(MemReadByte(R[rm] + disp));
    }

    void MOVWL4(uint16 rm, uint16 disp) {
        disp <<= 1u;
        dbg_println("mov.w @(0x{:X},r{}), r0", disp, rm);
        R[0] = SignExtend<16>(MemReadWord(R[rm] + disp));
    }

    void MOVLL4(uint16 rm, uint16 disp, uint16 rn) {
        disp <<= 2u;
        dbg_println("mov.l @(0x{:X},r{}), r{}", disp, rm, rn);
        R[rn] = MemReadLong(R[rm] + disp);
    }

    void MOVBLG(uint16 disp) {
        dbg_println("mov.b @(0x{:X},gbr), r0", disp);
        R[0] = SignExtend<8>(MemReadByte(GBR + disp));
    }

    void MOVWLG(uint16 disp) {
        disp <<= 1u;
        dbg_println("mov.w @(0x{:X},gbr), r0", disp);
        R[0] = SignExtend<16>(MemReadWord(GBR + disp));
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
        MemWriteByte(R[rn] - 4, R[rm]);
        R[rn] -= 4;
    }

    void MOVBP(uint16 rm, uint16 rn) {
        dbg_println("mov.b @r{}+, r{}", rm, rn);
        R[rn] = SignExtend<8>(MemReadByte(R[rm]));
        if (rn != rm) {
            R[rm] += 1;
        }
    }

    void MOVWP(uint16 rm, uint16 rn) {
        dbg_println("mov.w @r{}+, r{}", rm, rn);
        R[rn] = SignExtend<16>(MemReadWord(R[rm]));
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
        sint32 simm = SignExtend<8>(imm);
        dbg_println("mov #{}0x{:X}, r{}", (simm < 0 ? "-" : ""), abs(simm), rn);
        R[rn] = simm;
    }

    void MOVWI(uint16 disp, uint16 rn) {
        disp <<= 1u;
        dbg_println("mov.w @(0x{:08X},pc), r{}", PC + 4 + disp, rn);
        R[rn] = SignExtend<16>(MemReadWord(PC + 4 + disp));
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

class Saturn {
public:
    Saturn()
        : m_sh2bus(m_SMPC)
        , m_masterSH2(m_sh2bus, true) {
        Reset(true);
    }

    void Reset(bool hard) {
        m_sh2bus.Reset(hard);
        m_masterSH2.Reset(hard);
        m_SMPC.Reset(hard);
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
    SMPC m_SMPC;
};

// -----------------------------------------------------------------------------

std::vector<uint8_t> loadFile(std::filesystem::path romPath) {
    fmt::print("Loading file {}... ", romPath.string());

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

    auto rom = loadFile(argv[1]);
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
