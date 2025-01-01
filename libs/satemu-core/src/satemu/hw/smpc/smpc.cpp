#include <satemu/hw/smpc/smpc.hpp>

#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/scu/scu.hpp>
#include <satemu/hw/sh2/sh2_block.hpp>
#include <satemu/hw/vdp/vdp.hpp>

#include <satemu/util/inline.hpp>

namespace satemu::smpc {

SMPC::SMPC(sh2::SH2Block &sh2, vdp::VDP &vdp, scu::SCU &scu, scsp::SCSP &scsp)
    : m_SH2(sh2)
    , m_VDP(vdp)
    , m_SCU(scu)
    , m_SCSP(scsp) {
    Reset(true);
}

void SMPC::Reset(bool hard) {
    IREG.fill(0x00);
    OREG.fill(0x00);
    COMREG = Command::None;
    SR.u8 = 0x80;
    SF = false;

    PDR1 = 0;
    PDR2 = 0;
    DDR1 = 0;
    DDR2 = 0;

    // TODO(SMPC): SMEM should be persisted
    SMEM.fill(0);

    m_busValue = 0x00;

    m_pioMode1 = false;
    m_pioMode2 = false;

    m_extLatchEnable1 = false;
    m_extLatchEnable2 = false;

    m_getPeripheralData = false;
    m_port1mode = 0;
    m_port2mode = 0;

    m_intbackInProgress = false;
    m_firstPeripheralReport = false;
    m_emittedPort1Status = false;
}

uint8 SMPC::Read(uint32 address) {
    switch (address) {
    case 0x21 ... 0x5F: return ReadOREG((address - 0x20) >> 1);
    case 0x61: return ReadSR();
    case 0x63: return ReadSF();
    case 0x75: return ReadPDR1();
    case 0x77: return ReadPDR2();
    case 0x7D: return 0; // IOSEL is write-only
    case 0x7F: return 0; // EXLE is write-only
    default: fmt::println("unhandled SMPC read from {:02X}", address); return m_busValue;
    }
}

void SMPC::Write(uint32 address, uint8 value) {
    m_busValue = value;
    switch (address) {
    case 0x01:
        WriteIREG(0x00, value);
        if (m_intbackInProgress) {
            // Handle INTBACK continue/break requests
            const bool continueFlag = bit::extract<7>(IREG[0]);
            const bool breakFlag = bit::extract<6>(IREG[0]);
            if (breakFlag) {
                // fmt::println("SMPC: INTBACK break request");
                m_intbackInProgress = false;
                SR.NPE = 0;
                SR.PDL = 0;
            } else if (continueFlag) {
                // fmt::println("SMPC: INTBACK continue request");
                INTBACK();
            }
        }
        break;
    case 0x03 ... 0x0D: WriteIREG(address >> 1u, value); break;
    case 0x1F: WriteCOMREG(value); break;
    case 0x63: WriteSF(value); break;
    case 0x75: WritePDR1(value); break;
    case 0x77: WritePDR2(value); break;
    case 0x79: WriteDDR1(value); break;
    case 0x7B: WriteDDR2(value); break;
    case 0x7D: WriteIOSEL(value); break;
    case 0x7F: WriteEXLE(value); break;
    default: fmt::println("unhandled SMPC write to {:02X} = {:02X}", address, value); break;
    }
}

FORCE_INLINE uint8 SMPC::ReadOREG(uint8 offset) const {
    return OREG[offset & 31];
}

FORCE_INLINE uint8 SMPC::ReadSR() const {
    return SR.u8;
}

FORCE_INLINE uint8 SMPC::ReadSF() const {
    return SF;
}

FORCE_INLINE void SMPC::WriteIREG(uint8 offset, uint8 value) {
    assert(offset < 7);
    IREG[offset] = value;
}

FORCE_INLINE void SMPC::WriteCOMREG(uint8 value) {
    COMREG = static_cast<Command>(value);

    // TODO: should delay execution
    switch (COMREG) {
    case Command::MSHON: MSHON(); break;
    case Command::SSHON: SSHON(); break;
    case Command::SSHOFF: SSHOFF(); break;
    case Command::SNDON: SNDON(); break;
    case Command::SNDOFF: SNDOFF(); break;
    case Command::CKCHG352: CKCHG352(); break;
    case Command::CKCHG320: CKCHG320(); break;
    case Command::RESENAB: RESENAB(); break;
    case Command::RESDISA: RESDISA(); break;
    case Command::INTBACK: INTBACK(); break;
    case Command::SETSMEM: SETSMEM(); break;
    case Command::SETTIME: SETTIME(); break;
    default: fmt::println("unhandled SMPC command {:02X}", static_cast<uint8>(COMREG)); break;
    }
}

FORCE_INLINE void SMPC::WriteSF(uint8 value) {
    SF = true;
}

FORCE_INLINE void SMPC::WriteIOSEL(uint8 value) {
    m_pioMode1 = bit::extract<0>(value);
    m_pioMode2 = bit::extract<1>(value);
}

void SMPC::WriteEXLE(uint8 value) {
    m_extLatchEnable1 = bit::extract<0>(value);
    m_extLatchEnable2 = bit::extract<1>(value);
}

FORCE_INLINE uint8 SMPC::ReadPDR1() const {
    return PDR1;
}

FORCE_INLINE void SMPC::WritePDR1(uint8 value) {
    // TODO: process port 1 peripheral properly

    // HACK: read from our fixed standard Saturn pad
    switch (DDR1 & 0x7F) {
    case 0x40: // TH control mode
        if (value & 0x40) {
            PDR1 = 0x70 | bit::extract<0, 3>(m_buttons);
        } else {
            PDR1 = 0x30 | bit::extract<12, 15>(m_buttons);
        }
        break;
    case 0x60: // TH/TR control mode
        switch (value & 0x60) {
        case 0x00: // R X Y Z
            PDR1 = 0x10 | bit::extract<4, 7>(m_buttons);
            break;
        case 0x20: // right left down up
            PDR1 = 0x30 | bit::extract<12, 15>(m_buttons);
            break;
        case 0x40: // start A C B
            PDR1 = 0x50 | bit::extract<8, 11>(m_buttons);
            break;
        case 0x60: // L 1 0 0
            PDR1 = 0x70 | bit::extract<0, 3>(m_buttons);
            break;
        }
    }
    // fmt::println("SMPC: {:02X} {:02X} -> {:02X}  buttons {:04X}", DDR1, value, PDR1, m_buttons);
}

FORCE_INLINE void SMPC::WriteDDR1(uint8 value) {
    DDR1 = value;
}

FORCE_INLINE uint8 SMPC::ReadPDR2() const {
    return PDR2;
}

FORCE_INLINE void SMPC::WritePDR2(uint8 value) {
    // TODO: process port 2 peripheral
}

FORCE_INLINE void SMPC::WriteDDR2(uint8 value) {
    DDR2 = value;
}

void SMPC::MSHON() {
    // fmt::println("SMPC: processing MSHON");

    // TODO: is this supposed to do something...?

    SF = 0; // done processing

    OREG[31] = 0x00;
}

void SMPC::SSHON() {
    // fmt::println("SMPC: processing SSHON");

    // Turn on and reset slave SH-2
    m_SH2.slaveEnabled = true;
    m_SH2.Reset(true);

    SF = 0; // done processing

    OREG[31] = 0x02;
}

void SMPC::SSHOFF() {
    // fmt::println("SMPC: processing SSHOFF");

    // Turn off slave SH-2
    m_SH2.slaveEnabled = false;

    SF = 0; // done processing

    OREG[31] = 0x03;
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

void SMPC::CKCHG352() {
    // fmt::println("SMPC: processing CKCHG352");

    ClockChange(true);

    SF = 0; // done processing

    OREG[31] = 0x0E;
}
void SMPC::CKCHG320() {
    // fmt::println("SMPC: processing CKCHG320");

    ClockChange(false);

    SF = 0; // done processing

    OREG[31] = 0x0F;
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

    if (m_intbackInProgress) {
        WriteINTBACKPeripheralReport();
    } else {
        m_intbackInProgress = true;

        // m_optimize = bit::extract<1>(IREG[1]);
        m_getPeripheralData = bit::extract<3>(IREG[1]);
        m_port1mode = bit::extract<4, 5>(IREG[1]);
        m_port2mode = bit::extract<6, 7>(IREG[1]);
        if (IREG[2] != 0xF0) {
            // TODO: log invalid INTBACK command
            // TODO: does SMPC reject the command in this case?
        }

        m_emittedPort1Status = false;
        m_firstPeripheralReport = true;

        const bool getSMPCStatus = IREG[0] == 0x01;
        if (getSMPCStatus) {
            WriteINTBACKStatusReport();
        } else if (m_getPeripheralData) {
            WriteINTBACKPeripheralReport();
        }
    }

    SF = 0; // done processing

    m_SCU.TriggerSystemManager();
}

void SMPC::WriteINTBACKStatusReport() {
    // fmt::println("SMPC: INTBACK status report");

    SR.bit7 = 0;                  // fixed 0
    SR.PDL = 1;                   // fixed 1 for status report
    SR.NPE = m_getPeripheralData; // 0=no peripheral data, 1=has peripheral data
    SR.RESB = 0;                  // reset button state (0=off, 1=on)
    SR.P1MDn = m_port1mode;
    SR.P2MDn = m_port2mode;

    // TODO: simulate full system reset (STE clear)
    OREG[0] = 0x80; // STE set, RESD clear

    // TODO: read from RTC
    // the date/time below refers to this project's very first commit
    OREG[1] = 0x20; // Year 1000s, Year 100s (BCD)
    OREG[2] = 0x24; // Year 10s, Year 1s (BCD)
    OREG[3] = 0x0B; // Day of week (0=sun), Month (hex, 1=jan)
    OREG[4] = 0x17; // Day (BCD)
    OREG[5] = 0x17; // Hour (BCD)
    OREG[6] = 0x01; // Minute (BCD)
    OREG[7] = 0x20; // Second (BCD)

    // TODO: read cartridge code from cartridge
    // TODO: allow setting or auto-detecting area code
    OREG[8] = 0x00; // Cartridge code (CTG1-0) == 0b00
    OREG[9] = 0x04; // Area code (0x01=JP, 0x04=NA)

    // TODO: update flags accordingly
    OREG[10] = 0b00110100; // System status 1 (TODO: 6=DOTSEL, 3=MSHNMI, 1=SYSRES, 0=SNDRES)
    OREG[11] = 0b00000000; // System status 2 (TODO: 6=CDRES)

    OREG[12] = SMEM[0]; // SMEM 1 Saved Data
    OREG[13] = SMEM[1]; // SMEM 2 Saved Data
    OREG[14] = SMEM[2]; // SMEM 3 Saved Data
    OREG[15] = SMEM[3]; // SMEM 4 Saved Data

    OREG[31] = 0x10;

    m_intbackInProgress = m_getPeripheralData;
}

void SMPC::WriteINTBACKPeripheralReport() {
    // fmt::println("SMPC: INTBACK peripheral report - first? {}", m_firstPeripheralReport);

    // TODO: read from the report generated by the device

    SR.bit7 = 1;                      // fixed 1
    SR.PDL = m_firstPeripheralReport; // 1=first data, 2=second+ data
    SR.NPE = 0;                       // 0=no remaining data, 1=more data
    SR.RESB = 0;                      // reset button state (0=off, 1=on)
    SR.P1MDn = m_port1mode;           // port 1 mode \  0=15 byte, 1=255 byte
    SR.P2MDn = m_port2mode;           // port 2 mode /  2=unused,  3=0 byte

    OREG.fill(0x00);
    OREG[0] = 0xF1;                               // 7-4 = F=no multitap/device directly connected; 3-0 = 1 device
    OREG[1] = 0x02;                               // 7-4 = 0=standard pad; 3-0 = 2 data bytes
    OREG[2] = bit::extract<8, 15>(m_buttons);     // 7-0 = left, right, down, up, start, A, C, B  \ button state
    OREG[3] = bit::extract<3, 7>(m_buttons) << 3; // 7-3 = R, X, Y, Z, L; 2-0 = nothing           / is inverted!

    // Port 2 = no connected device
    OREG[4] = 0xF0;

    m_emittedPort1Status = true; // HACK to keep track of the current report position
    m_firstPeripheralReport = false;
    m_intbackInProgress = false; // TODO: should clear the flag only if all reports have been generated
}

void SMPC::SETSMEM() {
    fmt::println("SMPC: processing SETSMEM {:02X} {:02X} {:02X} {:02X}", IREG[0], IREG[1], IREG[2], IREG[3]);

    SMEM[0] = IREG[0];
    SMEM[1] = IREG[1];
    SMEM[2] = IREG[2];
    SMEM[3] = IREG[3];
    // TODO: persist

    SF = 0; // done processing

    OREG[31] = 0x17;
}

void SMPC::SETTIME() {
    fmt::println("SMPC: processing SETTIME year={:02X}{:02X} day/month={:02X} day={:02X} time={:02X}:{:02X}:{:02X}",
                 IREG[0], IREG[1], IREG[2], IREG[3], IREG[4], IREG[5], IREG[6]);

    // const uint8 bcdYear100s = IREG[0];
    // const uint8 bcdYear1s = IREG[1];
    // const uint8 day = bit::extract<4, 7>(IREG[2]);
    // const uint8 month = bit::extract<0, 3>(IREG[2]);
    // const uint8 bcdDay = IREG[3];
    // const uint8 bcdHour = IREG[4];
    // const uint8 bcdMinute = IREG[5];
    // const uint8 bcdSecond = IREG[6];

    // TODO: update time

    SF = 0; // done processing

    OREG[31] = 0x16;
}

void SMPC::ClockChange(bool fast) {
    m_VDP.Reset(false);
    m_SCU.Reset(false);
    m_SCSP.Reset(false);

    // TODO: clear VDP VRAMs

    // TODO: stop slave SH2

    m_SH2.master.SetNMI();

    // TODO: update system clock
}

} // namespace satemu::smpc
