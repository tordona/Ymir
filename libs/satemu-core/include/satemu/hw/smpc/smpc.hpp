#pragma once

#include "smpc_defs.hpp"

#include "rtc.hpp"

#include <satemu/core/scheduler.hpp>

#include <satemu/util/debug_print.hpp>

#include <array>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu {

struct Saturn;

} // namespace satemu

// -----------------------------------------------------------------------------

namespace satemu::smpc {

class SMPC {
    static constexpr dbg::Category rootLog{"SMPC"};
    static constexpr dbg::Category regsLog{rootLog, "Regs"};

public:
    SMPC(sys::System &system, core::Scheduler &scheduler, Saturn &saturn);
    ~SMPC();

    void Reset(bool hard);

    uint8 Read(uint32 address);
    void Write(uint32 address, uint8 value);

    void SetResetButtonState(bool pressed) {
        bool prevState = m_resetState;
        m_resetState = pressed;
        if (prevState != m_resetState) {
            UpdateResetNMI();
        }
    }

    void SetAreaCode(uint8 areaCode) {
        rootLog.debug("Setting area code to {:X}", areaCode);
        m_areaCode = areaCode;
    }

    // HACK(SMPC): simple controller pad support
    // buttons is a bitmask with inverted state (0=pressed, 1=released)
    // use the kButton* constants to set/clear bits
    uint16 &Buttons() {
        return m_buttons;
    }

private:
    std::array<uint8, 7> IREG;
    std::array<uint8, 32> OREG;

    std::array<uint8, 4> SMEM;

    bool m_STE; // false = forces system configuration on boot up

    bool m_resetDisable; // RESD flag, masks the Reset state
    bool m_resetState;   // State of the console's Reset button

    void UpdateResetNMI();

    // Area code:
    //   0x1: (J) Japan
    //   0x2: (T) Asia NTSC
    //   0x4: (U) North America
    //   0x5: (B) Central/South America NTSC
    //   0x6: (K) Korea
    //   0xA: (A) Asia PAL
    //   0xC: (E) Europe PAL
    //   0xD: (L) Central/South America PAL
    // 0x0 and 0xF are prohibited; all others are reserved
    uint8 m_areaCode;

    Saturn &m_saturn;
    core::Scheduler &m_scheduler;
    core::EventID m_commandEvent;

    // -------------------------------------------------------------------------
    // Persistent data

    static constexpr uint8 kPersistentDataVersion = 0x01;

    void ReadPersistentData();
    void WritePersistentData();

    // -------------------------------------------------------------------------
    // Registers

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
    union RegSR {
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

    uint8 PDR1;
    uint8 PDR2;
    uint8 DDR1;
    uint8 DDR2;

    uint8 m_busValue;

    uint8 ReadOREG(uint8 offset) const;
    uint8 ReadSR() const;
    uint8 ReadSF() const;

    void WriteIREG(uint8 offset, uint8 value);
    void WriteCOMREG(uint8 value);
    void WriteSF(uint8 value);
    void WriteIOSEL(uint8 value);
    void WriteEXLE(uint8 value);

    uint8 ReadPDR1() const;
    void WritePDR1(uint8 value);
    void WriteDDR1(uint8 value);

    uint8 ReadPDR2() const;
    void WritePDR2(uint8 value);
    void WriteDDR2(uint8 value);

    // -------------------------------------------------------------------------
    // RTC

    rtc::RTC m_rtc;

    // -------------------------------------------------------------------------
    // Input, parallel I/O and INTBACK

    // TODO: support multiple controllers, multi-tap, different types of devices, etc.
    // for now, emulate just one standard Saturn pad (not analog)
    //   15-12: right left down up
    //   11-8:  start A C B
    //    7-4:  R X Y Z
    //    3-0:  L <one> <zero> <zero>
    uint16 m_buttons = 0xFFFC;

    // Parallel I/O SMPC-controlled (false) or SH-2 direct mode (true)
    bool m_pioMode1;
    bool m_pioMode2;

    // External latch enable flags
    bool m_extLatchEnable1;
    bool m_extLatchEnable2;

    // INTBACK request parameters
    bool m_getPeripheralData;
    uint8 m_port1mode;
    uint8 m_port2mode;

    // INTBACK output control
    bool m_intbackInProgress;
    bool m_firstPeripheralReport;

    // HACK: simulate port 1 having a standard Saturn pad
    bool m_emittedPort1Status; // have we emitted port 1 status? (valid only if peripheral data requested)
    // TODO: read port 1 data properly into an array
    // - keep track of how many bytes have been read so that subsequent report requests can continue from that point
    // TODO: read port 2 data
    // TODO: support multitap
    // TODO: support controllers with longer reports
    // - does the report split into two parts, or does it have to be in a single report?

    void WriteINTBACKStatusReport();
    void WriteINTBACKPeripheralReport();

    // -------------------------------------------------------------------------
    // Commands

    void ProcessCommand();

    void MSHON();
    void SSHON();
    void SSHOFF();
    void SNDON();
    void SNDOFF();
    void SYSRES();
    void CKCHG352();
    void CKCHG320();
    void NMIREQ();
    void RESENAB();
    void RESDISA();
    void INTBACK();
    void SETSMEM();
    void SETTIME();

    void ClockChange(sys::ClockSpeed clockSpeed);
};

} // namespace satemu::smpc
