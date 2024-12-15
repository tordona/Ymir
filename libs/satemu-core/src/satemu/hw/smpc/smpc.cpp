#include <satemu/hw/smpc/smpc.hpp>

#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/scu/scu.hpp>

namespace satemu::smpc {

SMPC::SMPC(scu::SCU &scu, scsp::SCSP &scsp)
    : m_SCU(scu)
    , m_SCSP(scsp) {
    Reset(true);
}

void SMPC::Reset(bool hard) {
    IREG.fill(0x00);
    OREG.fill(0x00);
    COMREG = Command::None;
    SR.u8 = 0x80;
    SF = false;

    m_busValue = 0x00;
}

uint8 SMPC::Read(uint32 address) {
    switch (address) {
    case 0x21 ... 0x5F: return ReadOREG((address - 0x20) >> 1);
    case 0x61: return ReadSR();
    case 0x63: return ReadSF();
    default: fmt::println("unhandled SMPC read from {:02X}", address); return m_busValue;
    }
}

void SMPC::Write(uint32 address, uint8 value) {
    m_busValue = value;
    switch (address) {
    case 0x01 ... 0x0D: WriteIREG(address >> 1, value); break;
    case 0x1F: WriteCOMREG(value); break;
    case 0x63: WriteSF(value); break;
    default: fmt::println("unhandled SMPC write to {:02X} = {:02X}", address, value); break;
    }
}

uint8 SMPC::ReadOREG(uint8 offset) const {
    return OREG[offset & 31];
}

uint8 SMPC::ReadSR() const {
    return SR.u8;
}

uint8 SMPC::ReadSF() const {
    return SF;
}

void SMPC::WriteIREG(uint8 offset, uint8 value) {
    assert(offset < 7);
    IREG[offset] = value;
}

void SMPC::WriteCOMREG(uint8 value) {
    COMREG = static_cast<Command>(value);

    // TODO: should delay execution
    switch (COMREG) {
    case Command::SNDON: SNDON(); break;
    case Command::SNDOFF: SNDOFF(); break;
    case Command::RESENAB: RESENAB(); break;
    case Command::RESDISA: RESDISA(); break;
    case Command::INTBACK: INTBACK(); break;
    default: fmt::println("unhandled SMPC command {:02X}", static_cast<uint8>(COMREG)); break;
    }
}

void SMPC::WriteSF(uint8 value) {
    SF = true;
}

void SMPC::SNDON() {
    // fmt::println("SMPC: processing SNDON");

    m_SCSP.SetCPUEnabled(true);

    SF = 0; // done processing

    OREG[31] = 0x06;
}

void SMPC::SNDOFF() {
    // fmt::println("SMPC: processing SNDOFF");

    m_SCSP.SetCPUEnabled(false);

    SF = 0; // done processing

    OREG[31] = 0x07;
}

void SMPC::RESENAB() {
    // fmt::println("SMPC: processing RESENAB");
    // TODO: enable reset NMI

    SF = 0; // done processing

    OREG[31] = 0x19;
}

void SMPC::RESDISA() {
    // fmt::println("SMPC: processing RESDISA");
    // TODO: disable reset NMI

    SF = 0; // done processing

    OREG[31] = 0x1A;
}

void SMPC::INTBACK() {
    // fmt::println("SMPC: processing INTBACK {:02X} {:02X} {:02X}", IREG[0], IREG[1], IREG[2]);
    // TODO: implement properly

    const bool getSMPCStatus = IREG[0];
    // const bool optimize = bit::extract<1>(IREG[1]);
    const bool getPeripheralData = bit::extract<3>(IREG[1]);
    const uint8 port1mode = bit::extract<4, 5>(IREG[1]);
    const uint8 port2mode = bit::extract<6, 7>(IREG[1]);
    if (IREG[2] != 0xF0) {
        // TODO: log invalid INTBACK command
        // TODO: does SMPC reject the command in this case?
    }

    SF = 0; // done processing

    if (getSMPCStatus) {
        SR.bit7 = 0; // fixed 0
        SR.PDL = 1;  // fixed 1
        SR.NPE = 0;  // 0=no remaining data, 1=more data
        SR.RESB = 0; // reset button state (0=off, 1=on)

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
    } else if (getPeripheralData) {
        SR.bit7 = 1;          // fixed 1
        SR.PDL = 1;           // 1=first data, 2=second+ data
        SR.NPE = 0;           // 0=no remaining data, 1=more data
        SR.RESB = 0;          // reset button state (0=off, 1=on)
        SR.P1MDn = port1mode; // port 1 mode: 0=15 byte, 1=255 byte
        SR.P2MDn = port2mode; // port 2 mode: 2=unused,  3=0 byte

        OREG.fill(0xFF);
        OREG[0] = 0xF1; // 7-4 = F=no multitap/device directly connected; 3-0 = 1 device
        OREG[1] = 0x02; // 7-4 = 0=standard pad; 3-0 = 2 data bytes
        OREG[2] = 0xFF; // individual bits 7-0: left, right, down, up, start, A, C, B
        OREG[3] = 0xFF; // individual bits 7-3: R, X, Y, Z, L; button state is inverted
    }
    m_SCU.TriggerSystemManager();
}

} // namespace satemu::smpc
