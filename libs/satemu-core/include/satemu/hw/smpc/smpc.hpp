#pragma once

#include "smpc_defs.hpp"

#include <satemu/util/bit_ops.hpp>

#include <array>
#include <cassert>

#include <fmt/format.h>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::sh2 {

class SH2Block;

} // namespace satemu::sh2

namespace satemu::vdp {

class VDP;

} // namespace satemu::vdp

namespace satemu::scu {

class SCU;

} // namespace satemu::scu

namespace satemu::scsp {

class SCSP;

} // namespace satemu::scsp

// -----------------------------------------------------------------------------

namespace satemu::smpc {

inline constexpr uint16 kButtonRight = (1u << 15u);
inline constexpr uint16 kButtonLeft = (1u << 14u);
inline constexpr uint16 kButtonDown = (1u << 13u);
inline constexpr uint16 kButtonUp = (1u << 12u);
inline constexpr uint16 kButtonStart = (1u << 11u);
inline constexpr uint16 kButtonA = (1u << 10u);
inline constexpr uint16 kButtonC = (1u << 9u);
inline constexpr uint16 kButtonB = (1u << 8u);
inline constexpr uint16 kButtonR = (1u << 7u);
inline constexpr uint16 kButtonX = (1u << 6u);
inline constexpr uint16 kButtonY = (1u << 5u);
inline constexpr uint16 kButtonZ = (1u << 4u);
inline constexpr uint16 kButtonL = (1u << 3u);

class SMPC {
public:
    SMPC(sh2::SH2Block &sh2, vdp::VDP &vdp, scu::SCU &scu, scsp::SCSP &scsp);

    void Reset(bool hard);

    uint8 Read(uint32 address);
    void Write(uint32 address, uint8 value);

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

    sh2::SH2Block &m_SH2;
    vdp::VDP &m_VDP;
    scu::SCU &m_SCU;
    scsp::SCSP &m_SCSP;

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

    void MSHON();
    void SSHOFF();
    void SNDON();
    void SNDOFF();
    void CKCHG352();
    void CKCHG320();
    void RESENAB();
    void RESDISA();
    void INTBACK();
    void SETSMEM();
    void SETTIME();

    void ClockChange(bool fast);
};

} // namespace satemu::smpc
