#pragma once

#include "scu_defs.hpp"

#include <satemu/core/scheduler.hpp>

#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/hw_defs.hpp>
#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/vdp/vdp.hpp>

#include <satemu/util/data_ops.hpp>
#include <satemu/util/debug_print.hpp>

#include <mio/mmap.hpp> // HACK: should be used in a binary reader/writer object

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::sh2 {

class SH2Block;

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
// - A-Bus and B-Bus reads are always 32-bit (split into two 16-bit reads internally)
// - A-Bus and B-Bus 32-bit writes are split into two 16-bit writes internally
// - 68000 Work RAM
//   - [TODO] Area size depends on MEM4MB bit setting:
//       0=only first 256 KiB are used/mirrored
//       1=all 512 KiB are used/mirrored
// - VDP2 CRAM
//   - [TODO] Byte writes write garbage to the odd/even byte counterpart
//   - Byte reads work normally
class SCU {
    static constexpr dbg::Category rootLog{"SCU"};
    static constexpr dbg::Category regsLog{rootLog, "Regs"};
    static constexpr dbg::Category dmaLog{rootLog, "DMA"};
    static constexpr dbg::Category debugLog{rootLog, "Debug"};

public:
    SCU(core::Scheduler &scheduler, vdp::VDP &vdp, scsp::SCSP &scsp, cdblock::CDBlock &cdblock, sh2::SH2Block &sh2);

    void Reset(bool hard);

    void Advance(uint64 cycles);

    template <mem_primitive T>
    T Read(uint32 address) {
        using namespace util;

        /****/ if (util::AddressInRange<0x200'0000, 0x58F'FFFF>(address)) {
            return ReadABus<T>(address);
        } else if (util::AddressInRange<0x5A0'0000, 0x5FB'FFFF>(address)) {
            return ReadBBus<T>(address);
        } else if (util::AddressInRange<0x5C0'0000, 0x5FF'FFFF>(address)) {
            return ReadReg<T>(address & 0xFF);
        } else {
            regsLog.debug("unexpected {}-bit read from {:05X}", sizeof(T) * 8, address);
            return 0;
        }
    }

    template <mem_primitive T>
    void Write(uint32 address, T value) {
        using namespace util;

        /****/ if (util::AddressInRange<0x200'0000, 0x58F'FFFF>(address)) {
            WriteABus<T>(address, value);
        } else if (util::AddressInRange<0x5A0'0000, 0x5FB'FFFF>(address)) {
            WriteBBus<T>(address, value);
        } else if (util::AddressInRange<0x5C0'0000, 0x5FF'FFFF>(address)) {
            WriteReg<T>(address & 0xFF, value);
        } else {
            regsLog.debug("unexpected {}-bit write to {:05X} = {:X}", sizeof(T) * 8, address, value);
        }
    }

    // -------------------------------------------------------------------------
    // External interrupt triggers

    void TriggerVBlankIN();
    void TriggerVBlankOUT();
    void TriggerHBlankIN();
    void TriggerTimer0();
    void TriggerTimer1();
    void TriggerDSPEnd();
    void TriggerSoundRequest(bool level);
    void TriggerSystemManager();
    void TriggerDMAEnd(uint32 level);
    void TriggerSpriteDrawEnd();

    void TriggerExternalInterrupt0();
    void AcknowledgeExternalInterrupt();

private:
    vdp::VDP &m_VDP;
    scsp::SCSP &m_SCSP;
    cdblock::CDBlock &m_CDBlock;
    sh2::SH2Block &m_SH2;

    core::Scheduler &m_scheduler;
    core::EventID m_timer1Event;

    // TODO: don't hardcode this
    // TODO: use an abstraction
    // TODO: move to its own class
    // std::array<uint8, kInternalBackupRAMSize> internalBackupRAM;
    mio::mmap_sink m_externalBackupRAM;

    std::string m_debugOutput;

    // -------------------------------------------------------------------------
    // A-Bus and B-Bus accessors

    template <mem_primitive T>
    T ReadABus(uint32 address) {
        using namespace util;

        if constexpr (std::is_same_v<T, uint32>) {
            // 32-bit reads are split into two 16-bit reads
            uint32 value = ReadABus<uint16>(address + 0) << 16u;
            value |= ReadABus<uint16>(address + 2) << 0u;
            return value;

        } else if (AddressInRange<0x400'0000, 0x4FF'FFFF>(address)) {
            // HACK: emulate 32 Mbit backup RAM cartridge
            if (address >= 0x4FF'FFFC) {
                // Return ID for 32 Mbit Backup RAM cartridge
                if constexpr (std::is_same_v<T, uint8>) {
                    if ((address & 1) == 0) {
                        return 0xFF;
                    } else {
                        return 0x24;
                    }
                } else {
                    return 0xFF24;
                }
            } else {
                return util::ReadBE<T>((const uint8 *)&m_externalBackupRAM.data()[address & 0x7FFFFF]);
            }
        } else if (AddressInRange<0x580'0000, 0x58F'FFFF>(address)) {
            if ((address & 0x7FFF) < 0x1000) {
                // CD Block registers are mirrored every 64 bytes in a 4 KiB block.
                // These 4 KiB blocks are mapped every 32 KiB.
                return m_CDBlock.ReadReg<T>(address & 0x3F);
            }
        }
        regsLog.debug("unhandled {}-bit SCU A-Bus read from {:05X}", sizeof(T) * 8, address);
        return 0;
    }

    template <mem_primitive T>
    T ReadBBus(uint32 address) {
        using namespace util;

        if constexpr (std::is_same_v<T, uint32>) {
            // 32-bit reads are split into two 16-bit reads
            uint32 value = ReadBBus<uint16>(address + 0) << 16u;
            value |= ReadBBus<uint16>(address + 2) << 0u;
            return value;

        } else if (AddressInRange<0x5A0'0000, 0x5AF'FFFF>(address)) {
            return m_SCSP.ReadWRAM<T>(address & 0x7FFFF);
        } else if (AddressInRange<0x5B0'0000, 0x5BF'FFFF>(address)) {
            return m_SCSP.ReadReg<T>(address & 0xFFF);

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

        } else {
            regsLog.debug("unhandled {}-bit SCU B-Bus read from {:05X}", sizeof(T) * 8, address);
            return 0;
        }
    }

    template <mem_primitive T>
    void WriteABus(uint32 address, T value) {
        using namespace util;

        if constexpr (std::is_same_v<T, uint32>) {
            // 32-bit writes are split into two 16-bit writes
            WriteABus<uint16>(address + 0, value >> 16u);
            WriteABus<uint16>(address + 2, value >> 0u);

        } else if (address == 0x210'0001) [[unlikely]] {
            if constexpr (std::is_same_v<T, uint8>) {
                // mednafen debug port
                if (value == '\n') {
                    debugLog.debug("{}", m_debugOutput);
                    m_debugOutput.clear();
                } else if (value != '\r') {
                    m_debugOutput.push_back(value);
                }
            }
        } else if (AddressInRange<0x400'0000, 0x4FF'FFFF>(address)) {
            // HACK: emulate 32 Mbit backup RAM cartridge
            util::WriteBE<T>((uint8 *)&m_externalBackupRAM.data()[address & 0x7FFFFF], value);
        } else if (AddressInRange<0x580'0000, 0x58F'FFFF>(address)) {
            if ((address & 0x7FFF) < 0x1000) {
                // CD Block registers are mirrored every 64 bytes in a 4 KiB block.
                // These 4 KiB blocks are mapped every 32 KiB.
                m_CDBlock.WriteReg<T>(address & 0x3F, value);
            }

        } else {
            regsLog.debug("unhandled {}-bit SCU A-Bus write to {:05X} = {:X}", sizeof(T) * 8, address, value);
        }
    }

    template <mem_primitive T>
    void WriteBBus(uint32 address, T value) {
        using namespace util;

        if constexpr (std::is_same_v<T, uint32>) {
            // 32-bit writes are split into two 16-bit writes
            WriteBBus<uint16>(address + 0, value >> 16u);
            WriteBBus<uint16>(address + 2, value >> 0u);
        } else if (AddressInRange<0x5A0'0000, 0x5AF'FFFF>(address)) {
            m_SCSP.WriteWRAM<T>(address & 0x7FFFF, value);
        } else if (AddressInRange<0x5B0'0000, 0x5BF'FFFF>(address)) {
            m_SCSP.WriteReg<T>(address & 0xFFF, value);

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

        } else {
            regsLog.debug("unhandled {}-bit SCU B-Bus write to {:05X} = {:X}", sizeof(T) * 8, address, value);
        }
    }

    // -------------------------------------------------------------------------
    // Interrupts

    InterruptMask m_intrMask;
    InterruptStatus m_intrStatus;
    bool m_abusIntrAck;

    // -------------------------------------------------------------------------
    // DMA

    enum class DMATrigger {
        VBlankIN = 0,
        VBlankOUT = 1,
        HBlankIN = 2,
        Timer0 = 3,
        Timer1 = 4,
        SoundRequest = 5,
        SpriteDrawEnd = 6,
        Immediate = 7,
    };

    struct DMAChannel {
        DMAChannel() {
            Reset();
        }

        void Reset() {
            srcAddr = 0;   // initial value undefined
            dstAddr = 0;   // initial value undefined
            xferCount = 0; // initial value undefined
            srcAddrInc = 4;
            dstAddrInc = 2;
            updateSrcAddr = false;
            updateDstAddr = false;
            enabled = false;
            active = false;
            indirect = false;
            trigger = DMATrigger::Immediate;

            start = false;
            currSrcAddr = 0;
            currDstAddr = 0;
            currXferCount = 0;

            currIndirectSrc = 0;
            endIndirect = false;
        }

        uint32 srcAddr;     // DnR - Read address
        uint32 dstAddr;     // DnW - Write address
        uint32 xferCount;   // DnC - Transfer byte count (up to 1 MiB for level 0, 4 KiB for levels 1 and 2)
        uint32 srcAddrInc;  // DnAD.DnRA - Read address increment (0=0, 1=+4 bytes)
        uint32 dstAddrInc;  // DnAD.DnWA - Write address increment (+0,2,4,8,16,32,64,128 bytes)
        bool updateSrcAddr; // DnRUP - Update read address after transfer
        bool updateDstAddr; // DnWUP - Update write address after transfer
        bool enabled;       // DxEN - Enable
        bool active;        // Transfer active (triggered by trigger condition)
        bool indirect;      // DxMOD - Mode (false=direct, true=indirect)
        DMATrigger trigger; // DxFT2-0 - DMA Starting Factor

        bool start;            // Start transfer on next cycle
        uint32 currSrcAddr;    // Current read address
        uint32 currDstAddr;    // Current write address
        uint32 currXferCount;  // Current transfer count (stops when == xferCount)
        uint32 currSrcAddrInc; // Current read address increment
        uint32 currDstAddrInc; // Current write address increment

        uint32 currIndirectSrc; // Indirect data transfer source address
        bool endIndirect;       // Whether the end flag was sent on the current indirect transfer
    };

    std::array<DMAChannel, 3> m_dmaChannels;
    uint8 m_activeDMAChannelLevel;

    void RunDMA();

    void RecalcDMAChannel();

    void TriggerDMATransfer(DMATrigger trigger);

    // -------------------------------------------------------------------------
    // DSP

    struct DSPState {
        DSPState() {
            Reset(true);
        }

        void Reset(bool hard) {
            if (hard) {
                programRAM.fill(0);
                for (auto &bank : dataRAM) {
                    bank.fill(0);
                }
            }

            programExecuting = false;
            programPaused = false;
            programEnded = false;
            programStep = false;

            PC = 0;
            dataAddress = 0;

            nextPC = ~0;
            jmpCounter = 0;

            sign = false;
            zero = false;
            carry = false;
            overflow = false;

            CT.fill(0);
            incCT.fill(false);

            ALU.u64 = 0;
            AC.u64 = 0;
            P.u64 = 0;
            RX = 0;
            RY = 0;

            loopTop = 0;
            loopCount = 0;

            dmaRun = false;
            dmaToD0 = false;
            dmaHold = false;
            dmaCount = 0;
            dmaSrc = 0;
            dmaDst = 0;
            dmaReadAddr = 0;
            dmaWriteAddr = 0;
            dmaAddrInc = 0;
        }

        std::array<uint32, 256> programRAM;
        std::array<std::array<uint32, 64>, 4> dataRAM;

        bool programExecuting;
        bool programPaused;
        bool programEnded;
        bool programStep;

        uint8 PC; // program address
        uint8 dataAddress;

        uint32 nextPC;    // jump target
        uint8 jmpCounter; // when it reaches zero, perform the jump

        bool sign;
        bool zero;
        bool carry;
        bool overflow;

        // DSP data address
        std::array<uint8, 4> CT;
        std::array<bool, 4> incCT; // whether CT must be incremented after this iteration

        // ALU operation output
        union {
            uint64 u64 : 48;
            sint64 s64 : 48;
            struct {
                uint32 L;
                uint16 H;
            };
        } ALU;

        // ALU operation input 1
        union {
            uint64 u64 : 48;
            sint64 s64 : 48;
            struct {
                uint32 L;
                uint16 H;
            };
        } AC;

        // ALU operation input 2
        // Multiplication output
        union {
            uint64 u64 : 48;
            sint64 s64 : 48;
            struct {
                uint32 L;
                uint16 H;
            };
        } P;

        sint32 RX; // Multiplication input 1
        sint32 RY; // Multiplication input 2

        uint8 loopTop;    // TOP
        uint16 loopCount; // LOP

        bool dmaRun;         // DMA transfer in progress (T0)
        bool dmaToD0;        // DMA transfer direction: false=D0 to DSP, true=DSP to D0
        bool dmaHold;        // DMA transfer hold address
        uint8 dmaCount;      // DMA transfer length
        uint8 dmaSrc;        // DMA source register (CT0-3 or program RAM)
        uint8 dmaDst;        // DMA destination register (CT0-3 or program RAM)
        uint32 dmaReadAddr;  // DMA read address (RA0)
        uint32 dmaWriteAddr; // DMA write address (WA0)
        uint32 dmaAddrInc;   // DMA address increment

        void WriteProgram(uint32 value) {
            // Cannot write while program is executing
            if (programExecuting) {
                return;
            }

            programRAM[PC++] = value;
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
    void RunDSPDMA(uint64 cycles);

    // DSP memory/register accessors

    // X-Bus, Y-Bus and D1-Bus reads from [s]
    uint32 DSPReadSource(uint8 index);

    // D1-Bus writes to [d]
    void DSPWriteD1Bus(uint8 index, uint32 value);

    // Immediate writes to [d]
    void DSPWriteImm(uint8 index, uint32 value);

    // Checks if the current DSP flags pass the given condition
    bool DSPCondCheck(uint8 cond) const;

    // Prepares a delayed jump to the given target address
    void DSPDelayedJump(uint8 target);

    // DSP command interpreters

    void DSPCmd_Operation(uint32 command);
    void DSPCmd_LoadImm(uint32 command);
    void DSPCmd_Special(uint32 command);
    void DSPCmd_Special_DMA(uint32 command);
    void DSPCmd_Special_Jump(uint32 command);
    void DSPCmd_Special_LoopBottom(uint32 command);
    void DSPCmd_Special_End(uint32 command);

    // -------------------------------------------------------------------------
    // Timers

    // Timer 0 counts up at every HBlank IN
    // Resets to 0 at VBlank OUT of the first line before the display area
    // Raises interrupt when counter == compare
    uint16 m_timer0Counter;
    uint16 m_timer0Compare;

    // Timer 1 reloads at HBlank IN
    // Counts down every 7 MHz (4 cycles) when enabled
    // Raises interrupt when counter == 0 depending on mode:
    // - false: every line
    // - true: only if Timer 0 counter matched on previous line
    uint16 m_timer1Reload; // 2 fractional bits
    bool m_timer1Enable;
    bool m_timer1Mode;

    void TickTimer1();

    // -------------------------------------------------------------------------
    // SCU registers

    bool m_WRAMSizeSelect; // false=2x2Mbit, true=2x4Mbit

    template <mem_primitive T>
    T ReadReg(uint32 address) {
        if constexpr (std::is_same_v<T, uint8>) {
            const uint32 value = ReadReg<uint32>(address & ~3u);
            return value >> (((address & 3u) ^ 3u) * 8u);
        } else if constexpr (std::is_same_v<T, uint16>) {
            const uint32 value = ReadReg<uint32>(address & ~3u);
            return value >> ((((address >> 1u) & 1u) ^ 1u) * 16u);
        }

        switch (address) {
        case 0x00: // Level 0 DMA Read Address
        case 0x20: // Level 1 DMA Read Address
        case 0x40: // Level 2 DMA Read Address
            if constexpr (std::is_same_v<T, uint32>) {
                auto &ch = m_dmaChannels[address >> 5u];
                return ch.srcAddr;
            } else {
                return 0;
            }
        case 0x04: // Level 0 DMA Write Address
        case 0x24: // Level 1 DMA Write Address
        case 0x44: // Level 2 DMA Write Address
            if constexpr (std::is_same_v<T, uint32>) {
                auto &ch = m_dmaChannels[address >> 5u];
                return ch.dstAddr;
            } else {
                return 0;
            }
        case 0x08: // Level 0 DMA Transfer Number
        case 0x28: // Level 1 DMA Transfer Number
        case 0x48: // Level 2 DMA Transfer Number
            if constexpr (std::is_same_v<T, uint32>) {
                auto &ch = m_dmaChannels[address >> 5u];
                return ch.xferCount;
            } else {
                return 0;
            }
        case 0x0C: // Level 0 DMA Increment (write-only)
        case 0x2C: // Level 1 DMA Increment (write-only)
        case 0x4C: // Level 2 DMA Increment (write-only)
            return 0;
        case 0x10: // Level 0 DMA Enable (write-only)
        case 0x30: // Level 1 DMA Enable (write-only)
        case 0x50: // Level 2 DMA Enable (write-only)
            return 0;
        case 0x14: // Level 0 DMA Mode (write-only)
        case 0x34: // Level 1 DMA Mode (write-only)
        case 0x54: // Level 2 DMA Mode (write-only)
            return 0;

        case 0x60: // DMA Force Stop (write-only)
            return 0;
        case 0x7C: // DMA Status
        {
            uint32 value = 0;
            // bit::deposit_into<0>(value, m_dspState.dmaRun); // TODO: is this correct?
            // bit::deposit_into<1>(value, m_dspState.dmaStandby?);
            bit::deposit_into<4>(value, m_dmaChannels[0].active);
            // TODO: bit 5: DMA0 standby
            bit::deposit_into<8>(value, m_dmaChannels[1].active);
            // TODO: bit 9: DMA1 standby
            bit::deposit_into<12>(value, m_dmaChannels[2].active);
            // TODO: bit 13: DMA2 standby
            bit::deposit_into<16>(value,
                                  m_dmaChannels[0].active && (m_dmaChannels[1].active || m_dmaChannels[2].active));
            bit::deposit_into<17>(value, m_dmaChannels[1].active && m_dmaChannels[2].active);
            // TODO: bit 20: DMA accessing A-Bus
            // TODO: bit 21: DMA accessing B-Bus
            // TODO: bit 22: DMA accessing DSP-Bus
            return value;
        }

        case 0x80: // DSP Program Control Port
            if constexpr (std::is_same_v<T, uint32>) {
                uint32 value = 0;
                bit::deposit_into<0, 7>(value, m_dspState.PC);
                bit::deposit_into<16>(value, m_dspState.programExecuting);
                bit::deposit_into<18>(value, m_dspState.programEnded);
                bit::deposit_into<19>(value, m_dspState.overflow);
                bit::deposit_into<20>(value, m_dspState.carry);
                bit::deposit_into<21>(value, m_dspState.zero);
                bit::deposit_into<22>(value, m_dspState.sign);
                bit::deposit_into<23>(value, m_dspState.dmaRun);
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

        case 0x90: // Timer 0 Compare (write-only)
            return 0;
        case 0x94: // Timer 1 Set Data (write-only)
            return 0;
        case 0x98: // Timer 1 Mode (write-only)
            return 0;

        case 0xA0: // Interrupt Mask
            return m_intrMask.u32;
        case 0xA4: // Interrupt Status
            return m_intrStatus.u32;
        case 0xA8: // A-Bus Interrupt Acknowledge
            return m_abusIntrAck;

        case 0xB0: // A-Bus Set (part 1) (write-only)
            return 0;
        case 0xB4: // A-Bus Set (part 2) (write-only)
            return 0;
        case 0xB8: // A-Bus Refresh (write-only)
            return 0;

        case 0xC4: // SCU SDRAM Select
            return m_WRAMSizeSelect;
        case 0xC8: // SCU Version
            return 0x4;

        default: //
            regsLog.debug("unhandled {}-bit SCU register read from {:02X}", sizeof(T) * 8, address);
            return 0;
        }
    }

    template <mem_primitive T>
    void WriteReg(uint32 address, T value) {
        // TODO: handle 8-bit and 16-bit register writes if needed

        switch (address) {
        case 0x00: // Level 0 DMA Read Address
        case 0x20: // Level 1 DMA Read Address
        case 0x40: // Level 2 DMA Read Address
            if constexpr (std::is_same_v<T, uint32>) {
                auto &ch = m_dmaChannels[address >> 5u];
                ch.srcAddr = bit::extract<0, 26>(value);
            }
            break;
        case 0x04: // Level 0 DMA Write Address
        case 0x24: // Level 1 DMA Write Address
        case 0x44: // Level 2 DMA Write Address
            if constexpr (std::is_same_v<T, uint32>) {
                auto &ch = m_dmaChannels[address >> 5u];
                ch.dstAddr = bit::extract<0, 26>(value);
            }
            break;
        case 0x08: // Level 0 DMA Transfer Number
            if constexpr (std::is_same_v<T, uint32>) {
                auto &ch = m_dmaChannels[address >> 5u];
                ch.xferCount = bit::extract<0, 19>(value);
            }
            break;
        case 0x28: // Level 1 DMA Transfer Number
        case 0x48: // Level 2 DMA Transfer Number
            if constexpr (std::is_same_v<T, uint32>) {
                auto &ch = m_dmaChannels[address >> 5u];
                ch.xferCount = bit::extract<0, 11>(value);
            }
            break;
        case 0x0C: // Level 0 DMA Increment
        case 0x2C: // Level 1 DMA Increment
        case 0x4C: // Level 2 DMA Increment
            if constexpr (std::is_same_v<T, uint32>) {
                auto &ch = m_dmaChannels[address >> 5u];
                ch.srcAddrInc = bit::extract<8>(value) * 4u;
                ch.dstAddrInc = (1u << bit::extract<0, 2>(value)) & ~1u;
            }
            break;
        case 0x10: // Level 0 DMA Enable
        case 0x30: // Level 1 DMA Enable
        case 0x50: // Level 2 DMA Enable
            if constexpr (std::is_same_v<T, uint32>) {
                const uint32 index = address >> 5u;
                auto &ch = m_dmaChannels[index];
                ch.enabled = bit::extract<8>(value);
                if (ch.enabled) {
                    dmaLog.trace("DMA{} enabled - {:08X} (+{:02X}) -> {:08X} (+{:02X})", index, ch.srcAddr,
                                 ch.srcAddrInc, ch.dstAddr, ch.dstAddrInc);
                }
                if (ch.enabled && ch.trigger == DMATrigger::Immediate && bit::extract<0>(value)) {
                    if (ch.active) {
                        dmaLog.trace("DMA{} triggering immediate transfer while another transfer is in progress",
                                     index);
                        // Finish previous transfer
                        RunDMA();
                    }
                    ch.start = true;
                    RecalcDMAChannel();
                    RunDMA(); // HACK: run immediate DMA transfers immediately and instantly
                }
            }
            break;
        case 0x14: // Level 0 DMA Mode
        case 0x34: // Level 1 DMA Mode
        case 0x54: // Level 2 DMA Mode
            if constexpr (std::is_same_v<T, uint32>) {
                auto &ch = m_dmaChannels[address >> 5u];
                ch.indirect = bit::extract<24>(value);
                ch.updateSrcAddr = bit::extract<16>(value);
                ch.updateDstAddr = bit::extract<8>(value);
                ch.trigger = static_cast<DMATrigger>(bit::extract<0, 2>(value));
            }
            break;

        case 0x60: // DMA Force Stop
            if (bit::extract<0>(value)) {
                for (auto &ch : m_dmaChannels) {
                    ch.active = false;
                }
                m_activeDMAChannelLevel = m_dmaChannels.size();
            }
            break;
        case 0x7C: // DMA Status (read-only)
            break;

        case 0x80: // DSP Program Control Port
            if constexpr (std::is_same_v<T, uint32>) {
                if (bit::extract<15>(value)) {
                    m_dspState.PC = bit::extract<0, 7>(value);
                }
                if (bit::extract<25>(value)) {
                    m_dspState.programPaused = true;
                } else if (bit::extract<26>(value)) {
                    m_dspState.programPaused = false;
                } else if (!m_dspState.programExecuting) {
                    m_dspState.programExecuting = bit::extract<16>(value);
                    m_dspState.programStep = bit::extract<17>(value);
                    m_dspState.programEnded = false;
                }
            }
            break;
        case 0x84: // DSP Program RAM Data Port
            if constexpr (std::is_same_v<T, uint32>) {
                m_dspState.WriteProgram(value);
            }
            break;
        case 0x88: // DSP Data RAM Address Port
            if constexpr (std::is_same_v<T, uint32>) {
                m_dspState.dataAddress = bit::extract<0, 7>(value);
            }
            break;
        case 0x8C: // DSP Data RAM Data Port
            if constexpr (std::is_same_v<T, uint32>) {
                m_dspState.WriteData(value);
            }
            break;

        case 0x90: // Timer 0 Compare
            if constexpr (std::is_same_v<T, uint32>) {
                m_timer0Compare = bit::extract<0, 9>(value);
            }
            break;
        case 0x94: // Timer 1 Set Data
            if constexpr (std::is_same_v<T, uint32>) {
                m_timer1Reload = bit::extract<0, 8>(value) << 2u;
            }
            break;
        case 0x98: // Timer 1 Mode
            if constexpr (std::is_same_v<T, uint32>) {
                m_timer1Enable = bit::extract<0>(value);
                m_timer1Mode = bit::extract<8>(value);
            }
            break;

        case 0xA0: // Interrupt Mask
            m_intrMask.u32 = value & 0x0000BFFF;
            break;
        case 0xA4: // Interrupt Status
            m_intrStatus.u32 &= value;
            break;
        case 0xA8: // A-Bus Interrupt Acknowledge
            m_abusIntrAck = bit::extract<0>(value);
            UpdateInterruptLevel(false);
            break;

        case 0xB0: // A-Bus Set (part 1)
            // ignored for now
            break;
        case 0xB4: // A-Bus Set (part 2)
            // ignored for now
            break;
        case 0xB8: // A-Bus Refresh
            // ignored for now
            break;

        case 0xC4: // SCU SDRAM Select
            m_WRAMSizeSelect = value;
            break;
        case 0xC8: // SCU Version (read-only)
            break;

        default:
            regsLog.debug("unhandled {}-bit SCU register write to {:02X} = {:X}", sizeof(T) * 8, address, value);
            break;
        }
    }

    void UpdateInterruptLevel(bool acknowledge);
};

} // namespace satemu::scu
