#pragma once

#include "smpc_defs.hpp"

#include <array>
#include <cassert>

#include <fmt/format.h>

namespace satemu::smpc {

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
        case Command::RESENAB:
            fmt::println("RESENAB command received");
            RESENAB();
            break;
        case Command::RESDISA:
            fmt::println("RESDISA command received");
            RESDISA();
            break;
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

    void RESENAB() {
        // TODO: enable reset NMI

        SF = 0; // done processing

        OREG[31] = 0x19;
    }

    void RESDISA() {
        // TODO: disable reset NMI

        SF = 0; // done processing

        OREG[31] = 0x1A;
    }

    void INTBACK() {
        // TODO: implement

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

} // namespace satemu::smpc
