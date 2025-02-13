#pragma once

#include "scu_defs.hpp"

#include <satemu/core/scheduler.hpp>

#include <satemu/hw/hw_defs.hpp>

#include <satemu/sys/backup_ram.hpp>

#include <satemu/util/data_ops.hpp>
#include <satemu/util/debug_print.hpp>

#include <iosfwd>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu {

struct Saturn;

} // namespace satemu

namespace satemu::sh2 {

class SH2Block;
class SH2Bus;

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
    static constexpr dbg::Category dspLog{rootLog, "DSP"};
    static constexpr dbg::Category debugLog{rootLog, "Debug"};

public:
    SCU(core::Scheduler &scheduler, sh2::SH2Block &sh2);

    void Reset(bool hard);

    template <bool debug>
    void Advance(uint64 cycles);

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

    // -------------------------------------------------------------------------
    // RAM/register dumps

    void DumpDSPProgramRAM(std::ostream &out);
    void DumpDSPDataRAM(std::ostream &out);
    void DumpDSPRegs(std::ostream &out);

private:
    sh2::SH2Block &m_SH2;

    core::Scheduler &m_scheduler;
    core::EventID m_timer1Event;

    template <bool debug>
    static void OnTimer1Event(core::EventContext &eventContext, void *userContext, uint64 cyclesLate);

    // TODO: move to Backup RAM Cartridge implementation
    bup::BackupMemory m_externalBackupRAM;

    std::string m_debugOutput;

    // -------------------------------------------------------------------------
    // Memory accessors

    friend struct ::satemu::Saturn;
    void MapMemory(sh2::SH2Bus &bus);

    template <mem_primitive T>
    T ReadCartridge(uint32 address);

    template <mem_primitive T>
    void WriteCartridge(uint32 address, T value);

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

    template <bool debug>
    void TickTimer1();

    // -------------------------------------------------------------------------
    // SCU registers

    bool m_WRAMSizeSelect; // false=2x2Mbit, true=2x4Mbit

    template <mem_primitive T>
    T ReadReg(uint32 address);

    void WriteRegByte(uint32 address, uint8 value);
    void WriteRegWord(uint32 address, uint16 value);
    void WriteRegLong(uint32 address, uint32 value);

    void UpdateInterruptLevel(bool acknowledge);
};

} // namespace satemu::scu
