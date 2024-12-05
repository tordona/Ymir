#pragma once

#include "smpc_defs.hpp"

#include <satemu/util/bit_ops.hpp>

#include <array>
#include <cassert>

#include <fmt/format.h>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::scu {

class SCU;

} // namespace satemu::scu

namespace satemu::scsp {

class SCSP;

} // namespace satemu::scsp

// -----------------------------------------------------------------------------

namespace satemu::smpc {

class SMPC {
public:
    SMPC(scu::SCU &scu, scsp::SCSP &scsp);

    void Reset(bool hard);

    uint8 Read(uint32 address);
    void Write(uint32 address, uint8 value);

private:
    std::array<uint8, 7> IREG;
    std::array<uint8, 32> OREG;

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

    uint8 ReadOREG(uint8 offset) const;
    uint8 ReadSR() const;
    uint8 ReadSF() const;

    void WriteIREG(uint8 offset, uint8 value);
    void WriteCOMREG(uint8 value);
    void WriteSF(uint8 value);

    // -------------------------------------------------------------------------
    // Commands

    void SNDON();
    void SNDOFF();
    void RESENAB();
    void RESDISA();
    void INTBACK();
};

} // namespace satemu::smpc
