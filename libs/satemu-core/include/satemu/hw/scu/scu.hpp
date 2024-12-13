#pragma once

#include "scu_defs.hpp"

#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/hw_defs.hpp>
#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/vdp/vdp.hpp>

#include <satemu/util/data_ops.hpp>

#include <fmt/format.h>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::sh2 {

class SH2;

} // namespace satemu::sh2

// -----------------------------------------------------------------------------

namespace satemu::scu {

// SCU memory map
//
// TODO? Address range            Mirror size       Description
// TODO  0x200'0000..0x3FF'FFFF   -                 A-Bus CS0
// TODO  0x400'0000..0x4FF'FFFF   -                 A-Bus CS1
// TODO  0x500'0000..0x57F'FFFF   -                 A-Bus Dummy
//       0x580'0000..0x58F'FFFF   -                 A-Bus CS2 (includes CD-ROM registers)
// TODO  0x590'0000..0x59F'FFFF   -                 Lockup when read
//       0x5A0'0000..0x5AF'FFFF   0x40000/0x80000   68000 Work RAM
//       0x5B0'0000..0x5BF'FFFF   0x1000            SCSP registers
//       0x5C0'0000..0x5C7'FFFF   0x80000           VDP1 VRAM
//       0x5C8'0000..0x5CF'FFFF   0x40000           VDP1 Framebuffer (backbuffer only)
//       0x5D0'0000..0x5D7'FFFF   0x18 (no mirror)  VDP1 Registers
// TODO  0x5D8'0000..0x5DF'FFFF   -                 Lockup when read
//       0x5E0'0000..0x5EF'FFFF   0x80000           VDP2 VRAM
//       0x5F0'0000..0x5F7'FFFF   0x1000            VDP2 CRAM
//       0x5F8'0000..0x5FB'FFFF   0x200             VDP2 registers
// TODO  0x5FC'0000..0x5FD'FFFF   -                 Reads 0x000E0000
//       0x5FE'0000..0x5FE'FFFF   0x100             SCU registers
// TODO  0x5FF'0000..0x5FF'FFFF   0x100             Unknown registers
//
// Notes
// - Unless otherwise specified, all regions are mirrored across the designated area
// - Addresses 0x200'0000..0x58F'FFFF comprise the SCU A-Bus
// - Addresses 0x5A0'0000..0x5FB'FFFF comprise the SCU B-Bus
// - [TODO] A-Bus and B-Bus reads are always 32-bit (split into two 16-bit reads internally)
// - [TODO] A-Bus and B-Bus 32-bit writes are split into two 16-bit writes internally
// - 68000 Work RAM
//   - [TODO] Area size depends on MEM4MB bit setting:
//       0=only first 256 KiB are used/mirrored
//       1=all 512 KiB are used/mirrored
// - VDP2 CRAM
//   - [TODO] Byte writes write garbage to the odd/even byte counterpart
//   - Byte reads work normally
class SCU {
public:
    SCU(vdp::VDP &vdp, scsp::SCSP &scsp, cdblock::CDBlock &cdblock, sh2::SH2 &sh2);

    void Reset(bool hard);

    void Advance(uint64 cycles);

    template <mem_primitive T>
    T Read(uint32 address) {
        using namespace util;

        /****/ if (AddressInRange<0x580'0000, 0x58F'FFFF>(address)) {
            if ((address & 0x7FFF) < 0x1000 && address < 0x5891000) {
                // CD Block registers are mirrored every 64 bytes in a 4 KiB block.
                // These 4 KiB blocks are mapped every 32 KiB, up to 0x25891000.
                return m_CDBlock.ReadReg<T>(address & 0x3F);
            } else {
                return m_CDBlock.ReadData<T>(address & 0xFFFFF);
            }

        } else if (AddressInRange<0x5A0'0000, 0x5AF'FFFF>(address)) {
            if constexpr (std::is_same_v<T, uint32>) {
                uint32 value = m_SCSP.ReadWRAM<uint16>((address + 0) & 0x7FFFF);
                value = (value << 16u) | m_SCSP.ReadWRAM<uint16>((address + 2) & 0x7FFFF);
                return value;
            } else {
                return m_SCSP.ReadWRAM<T>(address & 0x7FFFF);
            }
        } else if (AddressInRange<0x5B0'0000, 0x5BF'FFFF>(address)) {
            if constexpr (std::is_same_v<T, uint32>) {
                uint32 value = m_SCSP.ReadReg<uint16>((address + 0) & 0xFFF);
                value = (value << 16u) | m_SCSP.ReadReg<uint16>((address + 2) & 0xFFF);
                return value;
            } else {
                return m_SCSP.ReadReg<T>(address & 0xFFF);
            }

        } else if (AddressInRange<0x5C0'0000, 0x5C7'FFFF>(address)) {
            return m_VDP.VDP1ReadVRAM<T>(address & 0x7FFFF);
        } else if (AddressInRange<0x5C8'0000, 0x5CF'FFFF>(address)) {
            return m_VDP.VDP1ReadFB<T>(address & 0x3FFFF);
        } else if (AddressInRange<0x5D0'0000, 0x5D7'FFFF>(address)) {
            return m_VDP.VDP1ReadReg<T>(address & 0x7FFFF);

        } else if (AddressInRange<0x5E0'0000, 0x5EF'FFFF>(address)) {
            return m_VDP.VDP2ReadVRAM<T>(address & 0x7FFFF);
        } else if (AddressInRange<0x5F0'0000, 0x5F7'FFFF>(address)) {
            return m_VDP.VDP2ReadCRAM<T>(address & 0xFFF);
        } else if (AddressInRange<0x5F8'0000, 0x5FB'FFFF>(address)) {
            return m_VDP.VDP2ReadReg<T>(address & 0x1FF);

        } else if (AddressInRange<0x5FE'0000, 0x5FE'FFFF>(address)) {
            return ReadReg<T>(address & 0xFF);
        }

        fmt::println("unhandled {}-bit SCU read from {:05X}", sizeof(T) * 8, address);
        return 0;
    }

    template <mem_primitive T>
    void Write(uint32 address, T value) {
        using namespace util;

        /****/ if (AddressInRange<0x580'0000, 0x58F'FFFF>(address)) {
            if ((address & 0x7FFF) < 0x1000 && address < 0x5891000) {
                // CD Block registers are mirrored every 64 bytes in a 4 KiB block.
                // These 4 KiB blocks are mapped every 32 KiB, up to 0x25891000.
                m_CDBlock.WriteReg<T>(address & 0x3F, value);
            } else {
                m_CDBlock.WriteData<T>(address & 0xFFFFF, value);
            }

        } else if (AddressInRange<0x5A0'0000, 0x5AF'FFFF>(address)) {
            if constexpr (std::is_same_v<T, uint32>) {
                m_SCSP.WriteWRAM<uint16>((address + 0) & 0x7FFFF, value >> 16u);
                m_SCSP.WriteWRAM<uint16>((address + 2) & 0x7FFFF, value >> 0u);
            } else {
                m_SCSP.WriteWRAM<T>(address & 0x7FFFF, value);
            }
        } else if (AddressInRange<0x5B0'0000, 0x5BF'FFFF>(address)) {
            if constexpr (std::is_same_v<T, uint32>) {
                m_SCSP.WriteReg<uint16>((address + 0) & 0xFFF, value >> 16u);
                m_SCSP.WriteReg<uint16>((address + 2) & 0xFFF, value >> 0u);
            } else {
                m_SCSP.WriteReg<T>(address & 0xFFF, value);
            }

        } else if (AddressInRange<0x5C0'0000, 0x5C7'FFFF>(address)) {
            m_VDP.VDP1WriteVRAM<T>(address & 0x7FFFF, value);
        } else if (AddressInRange<0x5C8'0000, 0x5CF'FFFF>(address)) {
            m_VDP.VDP1WriteFB<T>(address & 0x3FFFF, value);
        } else if (AddressInRange<0x5D0'0000, 0x5D7'FFFF>(address)) {
            m_VDP.VDP1WriteReg<T>(address & 0x7FFFF, value);

        } else if (AddressInRange<0x5E0'0000, 0x5EF'FFFF>(address)) {
            m_VDP.VDP2WriteVRAM<T>(address & 0x7FFFF, value);
        } else if (AddressInRange<0x5F0'0000, 0x5F7'FFFF>(address)) {
            m_VDP.VDP2WriteCRAM<T>(address & 0xFFF, value);
        } else if (AddressInRange<0x5F8'0000, 0x5FB'FFFF>(address)) {
            m_VDP.VDP2WriteReg<T>(address & 0x1FF, value);

        } else if (AddressInRange<0x5FE'0000, 0x5FE'FFFF>(address)) {
            WriteReg<T>(address & 0xFF, value);
        } else {
            fmt::println("unhandled {}-bit SCU write to {:05X} = {:X}", sizeof(T) * 8, address, value);
        }
    }

    // -------------------------------------------------------------------------
    // External interrupt triggers

    void TriggerHBlankIN();
    void TriggerVBlankIN();
    void TriggerVBlankOUT();
    void TriggerSystemManager();
    void TriggerSoundRequest(bool level);
    void TriggerExternalInterrupt0();

    void AcknowledgeExternalInterrupt();

private:
    vdp::VDP &m_VDP;
    scsp::SCSP &m_SCSP;
    cdblock::CDBlock &m_CDBlock;
    sh2::SH2 &m_SH2;

    // -------------------------------------------------------------------------
    // Interrupts

    InterruptMask m_intrMask;
    InterruptStatus m_intrStatus;

    // -------------------------------------------------------------------------
    // DSP

    struct DSPState {
        DSPState() {
            Reset();
        }

        void Reset() {
            programRAM.fill(0);
            for (auto &bank : dataRAM) {
                bank.fill(0);
            }

            programExecuting = false;
            programPaused = false;
            programEnded = false;
            programStep = false;

            programAddress = 0;
            dataAddress = 0;

            transfer0 = false;

            sign = false;
            zero = false;
            carry = false;
            overflow = false;

            dataAccessAddresses.fill(0);
            multiplierOut = 0;
        }

        std::array<uint32, 256> programRAM;
        std::array<std::array<uint32, 64>, 4> dataRAM;

        bool programExecuting;
        bool programPaused;
        bool programEnded;
        bool programStep;

        uint8 programAddress;
        uint8 dataAddress;

        bool transfer0;

        bool sign;
        bool zero;
        bool carry;
        bool overflow;

        std::array<uint8, 4> dataAccessAddresses; // CT0-3
        uint64 multiplierOut;                     // MULTIPLIER

        void WriteProgram(uint32 value) {
            // Cannot write while program is executing
            if (programExecuting) {
                return;
            }

            programRAM[programAddress++] = value;
        }

        uint32 ReadData() {
            // Cannot read while program is executing
            if (programExecuting) {
                return 0;
            }

            const uint8 bank = bit::extract<6, 7>(dataAddress);
            const uint8 offset = bit::extract<0, 5>(dataAddress);
            dataAddress++;
            return dataRAM[bank][offset];
        }

        void WriteData(uint32 value) {
            // Cannot write while program is executing
            if (programExecuting) {
                return;
            }

            const uint8 bank = bit::extract<6, 7>(dataAddress);
            const uint8 offset = bit::extract<0, 5>(dataAddress);
            dataAddress++;
            dataRAM[bank][offset] = value;
        }
    } m_dspState;

    void RunDSP(uint64 cycles);

    // -------------------------------------------------------------------------
    // SCU registers

    template <mem_primitive T>
    T ReadReg(uint32 address) {
        switch (address) {
        case 0x80: // DSP Program Control Port
            if constexpr (std::is_same_v<T, uint32>) {
                uint32 value = 0;
                bit::deposit_into<0, 7>(value, m_dspState.programAddress);
                bit::deposit_into<16>(value, m_dspState.programExecuting);
                bit::deposit_into<18>(value, m_dspState.programEnded);
                bit::deposit_into<19>(value, m_dspState.overflow);
                bit::deposit_into<20>(value, m_dspState.carry);
                bit::deposit_into<21>(value, m_dspState.zero);
                bit::deposit_into<22>(value, m_dspState.sign);
                bit::deposit_into<23>(value, m_dspState.transfer0);
                return value;
            } else {
                return 0;
            }
        case 0x84: // DSP Program RAM Data Port (write-only)
            return 0;
        case 0x88: // DSP Data RAM Address Port (write-only)
            return 0;
        case 0x8C: // DSP Data RAM Data Port
            if constexpr (std::is_same_v<T, uint32>) {
                return m_dspState.ReadData();
            } else {
                return 0;
            }

        case 0xA0: // Interrupt Mask
            return m_intrMask.u32;
        case 0xA4: // Interrupt Status
            return m_intrStatus.u32;
        case 0xA8: // A-Bus Interrupt Acknowledge
            // TODO: not yet sure how this works
            return 0;

        default: //
            fmt::println("unhandled {}-bit SCU register read from {:02X}", sizeof(T) * 8, address);
            return 0;
        }
    }

    template <mem_primitive T>
    void WriteReg(uint32 address, T value) {
        switch (address) {
        case 0x80: // DSP Program Control Port
            if constexpr (std::is_same_v<T, uint32>) {
                if (bit::extract<15>(value)) {
                    m_dspState.programAddress = bit::extract<0, 7>(value);
                }
                if (bit::extract<25>(value)) {
                    m_dspState.programPaused = true;
                } else if (bit::extract<26>(value)) {
                    m_dspState.programPaused = false;
                } else {
                    m_dspState.programExecuting = bit::extract<16>(value);
                    m_dspState.programStep = bit::extract<17>(value);
                }
            }
            break;
        case 0x84: // DSP Program RAM Data Port (write-only)
            if constexpr (std::is_same_v<T, uint32>) {
                m_dspState.WriteProgram(value);
            }
            break;
        case 0x88: // DSP Data RAM Address Port (write-only)
            if constexpr (std::is_same_v<T, uint32>) {
                m_dspState.dataAddress = bit::extract<0, 7>(value);
            }
            break;
        case 0x8C: // DSP Data RAM Data Port
            if constexpr (std::is_same_v<T, uint32>) {
                m_dspState.WriteData(value);
            }
            break;

        case 0xA0: // Interrupt Mask
            m_intrMask.u32 = value & 0x0000BFFF;
            break;
        case 0xA4: // Interrupt Status
            m_intrStatus.u32 &= value;
            break;
        case 0xA8: // A-Bus Interrupt Acknowledge
            // TODO: not yet sure how this works
            break;

        default:
            fmt::println("unhandled {}-bit SCU register write to {:02X} = {:X}", sizeof(T) * 8, address, value);
            break;
        }
    }

    void UpdateInterruptLevel(bool acknowledge);
};

} // namespace satemu::scu
