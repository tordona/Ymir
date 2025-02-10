#include <satemu/hw/smpc/smpc.hpp>

#include <satemu/sys/saturn.hpp>

#include <satemu/util/arith_ops.hpp>
#include <satemu/util/bit_ops.hpp>
#include <satemu/util/date_time.hpp>
#include <satemu/util/inline.hpp>

#include <cassert>
#include <filesystem>

namespace satemu::smpc {

SMPC::SMPC(core::Scheduler &scheduler, Saturn &saturn)
    : m_saturn(saturn)
    , m_scheduler(scheduler) {

    SMEM.fill(0);
    m_STE = false;

    m_resetState = false;

    ReadPersistentData();

    m_commandEvent = m_scheduler.RegisterEvent(
        core::events::SMPCCommand, this, [](core::EventContext &eventContext, void *userContext, uint64 cyclesLate) {
            auto &smpc = *static_cast<SMPC *>(userContext);
            smpc.ProcessCommand();
        });

    Reset(true);
}

SMPC::~SMPC() {
    WritePersistentData();
}

void SMPC::Reset(bool hard) {
    m_resetDisable = true;

    IREG.fill(0x00);
    OREG.fill(0x00);
    COMREG = Command::None;
    SR.u8 = 0x80;
    SF = false;

    PDR1 = 0;
    PDR2 = 0;
    DDR1 = 0;
    DDR2 = 0;

    m_busValue = 0x00;

    m_rtc.Reset(hard);
    m_rtc.SetSysClockRatio(false, false);

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
    default: regsLog.debug("unhandled SMPC read from {:02X}", address); return m_busValue;
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
                rootLog.trace("INTBACK break request");
                m_intbackInProgress = false;
                SR.NPE = 0;
                SR.PDL = 0;
            } else if (continueFlag) {
                rootLog.trace("INTBACK continue request");
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
    default: regsLog.debug("unhandled SMPC write to {:02X} = {:02X}", address, value); break;
    }
}

void SMPC::UpdateResetNMI() {
    if (!m_resetDisable && m_resetState) {
        m_saturn.SH2.master.SetNMI();
    }
}

void SMPC::ReadPersistentData() {
    // TODO(SMPC): configurable path
    std::filesystem::path smpcPersistentDataPath = "smpc.bin";

    // TODO: replace std iostream with custom I/O class with managed endianness
    std::ifstream in{smpcPersistentDataPath, std::ios::binary};
    if (!in) {
        return;
    }

    int version = in.get();
    if (version != kPersistentDataVersion || version < 0) {
        return;
    }
    in.seekg(3, std::ios::cur); // skip 3 reserved bytes

    std::array<uint8, 4> smem{};
    bool ste{};

    in.read((char *)&smem[0], sizeof(smem));
    in.read((char *)&ste, sizeof(ste));
    if (!in) {
        return;
    }
    SMEM = smem;
    m_STE = ste;

    m_rtc.ReadPersistentData(in);
}

void SMPC::WritePersistentData() {
    // TODO(SMPC): configurable path
    std::filesystem::path smpcPersistentDataPath = "smpc.bin";

    // TODO: replace std iostream with custom I/O class with managed endianness
    std::ofstream out{smpcPersistentDataPath, std::ios::binary};
    if (!out) {
        return;
    }

    out.put(kPersistentDataVersion);
    out.put(0x00); // reserved for future expansion
    out.put(0x00); // reserved for future expansion
    out.put(0x00); // reserved for future expansion
    out.write((const char *)&SMEM[0], sizeof(SMEM));
    out.write((const char *)&m_STE, sizeof(m_STE));

    m_rtc.WritePersistentData(out);
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

    if (COMREG == Command::SYSRES || COMREG == Command::CKCHG352 || COMREG == Command::CKCHG320) {
        // TODO: these should take ~100ms (about 2.8 million SH-2 cycles) to complete
        // Doing a shorter delay here to make it snappier
        m_scheduler.ScheduleFromNow(m_commandEvent, 200000);
    } else {
        // TODO: CDON and CDOFF execute in 40 microseconds; all other commands take 30 microseconds to complete
        m_scheduler.ScheduleFromNow(m_commandEvent, 30);
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

void SMPC::ProcessCommand() {
    switch (COMREG) {
    case Command::MSHON: MSHON(); break;
    case Command::SSHON: SSHON(); break;
    case Command::SSHOFF: SSHOFF(); break;
    case Command::SNDON: SNDON(); break;
    case Command::SNDOFF: SNDOFF(); break;
    case Command::SYSRES: SYSRES(); break;
    case Command::CKCHG352: CKCHG352(); break;
    case Command::CKCHG320: CKCHG320(); break;
    case Command::NMIREQ: NMIREQ(); break;
    case Command::RESENAB: RESENAB(); break;
    case Command::RESDISA: RESDISA(); break;
    case Command::INTBACK: INTBACK(); break;
    case Command::SETSMEM: SETSMEM(); break;
    case Command::SETTIME: SETTIME(); break;
    default: rootLog.debug("unhandled SMPC command {:02X}", static_cast<uint8>(COMREG)); break;
    }
}

void SMPC::MSHON() {
    rootLog.debug("Processing MSHON");

    // TODO: is this supposed to do something...?

    SF = false; // done processing

    OREG[31] = 0x00;
}

void SMPC::SSHON() {
    rootLog.debug("Processing SSHON");

    // Turn on and reset slave SH-2
    m_saturn.SH2.slaveEnabled = true;
    m_saturn.SH2.slave.Reset(true);

    SF = false; // done processing

    OREG[31] = 0x02;
}

void SMPC::SSHOFF() {
    rootLog.debug("Processing SSHOFF");

    // Turn off slave SH-2
    m_saturn.SH2.slaveEnabled = false;

    SF = false; // done processing

    OREG[31] = 0x03;
}

void SMPC::SNDON() {
    rootLog.debug("Processing SNDON");

    m_saturn.SCSP.SetCPUEnabled(true);

    SF = false; // done processing

    OREG[31] = 0x06;
}

void SMPC::SNDOFF() {
    rootLog.debug("Processing SNDOFF");

    m_saturn.SCSP.SetCPUEnabled(false);

    SF = false; // done processing

    OREG[31] = 0x07;
}

void SMPC::SYSRES() {
    rootLog.debug("Processing SYSRES");

    m_saturn.Reset(false);

    SF = false; // done processing

    OREG[31] = 0x0D;
}

void SMPC::CKCHG352() {
    rootLog.debug("Processing CKCHG352");

    ClockChange(true);

    SF = false; // done processing

    OREG[31] = 0x0E;
}
void SMPC::CKCHG320() {
    rootLog.debug("Processing CKCHG320");

    ClockChange(false);

    SF = false; // done processing

    OREG[31] = 0x0F;
}

void SMPC::NMIREQ() {
    rootLog.debug("Processing NMIREQ");

    m_saturn.SH2.master.SetNMI();

    SF = false; // done processing

    OREG[31] = 0x19;
}

void SMPC::RESENAB() {
    rootLog.debug("Processing RESENAB");

    bool prevState = m_resetDisable;
    m_resetDisable = false;
    if (prevState != m_resetDisable) {
        UpdateResetNMI();
    }

    SF = false; // done processing

    OREG[31] = 0x19;
}

void SMPC::RESDISA() {
    rootLog.debug("Processing RESDISA");

    bool prevState = m_resetDisable;
    m_resetDisable = true;
    if (prevState != m_resetDisable) {
        UpdateResetNMI();
    }

    SF = false; // done processing

    OREG[31] = 0x1A;
}

void SMPC::INTBACK() {
    rootLog.trace("Processing INTBACK {:02X} {:02X} {:02X}", IREG[0], IREG[1], IREG[2]);

    m_getPeripheralData = bit::extract<3>(IREG[1]);

    if (m_intbackInProgress) {
        if (m_getPeripheralData) {
            WriteINTBACKPeripheralReport();
        } else {
            WriteINTBACKStatusReport();
        }
    } else {
        m_intbackInProgress = true;

        // m_optimize = bit::extract<1>(IREG[1]);
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

    SF = false; // done processing

    m_saturn.SCU.TriggerSystemManager();
}

void SMPC::WriteINTBACKStatusReport() {
    rootLog.trace("INTBACK status report");

    SR.bit7 = 0;                  // fixed 0
    SR.PDL = 1;                   // fixed 1 for status report
    SR.NPE = m_getPeripheralData; // 0=no peripheral data, 1=has peripheral data
    SR.RESB = 0;                  // reset button state (0=off, 1=on)
    SR.P1MDn = m_port1mode;
    SR.P2MDn = m_port2mode;

    OREG[0] = m_STE << 7; // TODO: bit 6: RESD - reset disable flag

    if (m_rtc.GetMode() == rtc::RTC::Mode::Emulated) {
        m_rtc.UpdateSysClock(m_scheduler.CurrentCount());
    }
    const auto dt = m_rtc.GetDateTime();

    OREG[1] = util::to_bcd(dt.year / 100);  // Year 1000s, Year 100s (BCD)
    OREG[2] = util::to_bcd(dt.year % 100);  // Year 10s, Year 1s (BCD)
    OREG[3] = (dt.weekday << 4) | dt.month; // Day of week (0=sun), Month (hex, 1=jan)
    OREG[4] = util::to_bcd(dt.day);         // Day (BCD)
    OREG[5] = util::to_bcd(dt.hour);        // Hour (BCD)
    OREG[6] = util::to_bcd(dt.minute);      // Minute (BCD)
    OREG[7] = util::to_bcd(dt.second);      // Second (BCD)

    // the date/time below refers to this project's very first commit
    /*OREG[1] = 0x20; // Year 1000s, Year 100s (BCD)
    OREG[2] = 0x24; // Year 10s, Year 1s (BCD)
    OREG[3] = 0x0B; // Day of week (0=sun), Month (hex, 1=jan)
    OREG[4] = 0x17; // Day (BCD)
    OREG[5] = 0x17; // Hour (BCD)
    OREG[6] = 0x01; // Minute (BCD)
    OREG[7] = 0x20; // Second (BCD)*/

    // TODO: read cartridge code from cartridge
    // TODO: allow setting or auto-detecting area code
    OREG[8] = 0x00; // Cartridge code (CTG1-0) == 0b00
    OREG[9] = m_areaCode;

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
    rootLog.trace("INTBACK peripheral report - first? {}", m_firstPeripheralReport);

    // TODO: read from the report generated by the device

    SR.bit7 = 1;                      // fixed 1
    SR.PDL = m_firstPeripheralReport; // 1=first data, 2=second+ data
    SR.NPE = 0;                       // 0=no remaining data, 1=more data
    SR.RESB = 0;                      // reset button state (0=off, 1=on)
    SR.P1MDn = m_port1mode;           // port 1 mode \  0=15 byte, 1=255 byte
    SR.P2MDn = m_port2mode;           // port 2 mode /  2=unused,  3=0 byte

    const uint8 btnHi = bit::extract<8, 15>(m_buttons);
    const uint8 btnLo = (bit::extract<3, 7>(m_buttons) << 3) | 0x7;
    OREG.fill(0x00);
    OREG[0] = 0xF1;  // 7-4 = F=no multitap/device directly connected; 3-0 = 1 device
    OREG[1] = 0x02;  // 7-4 = 0=standard pad; 3-0 = 2 data bytes
    OREG[2] = btnHi; // 7-0 = left, right, down, up, start, A, C, B  \ button state
    OREG[3] = btnLo; // 7-3 = R, X, Y, Z, L; 2-0 = nothing           / is inverted!

    // Port 2 = no connected device
    OREG[4] = 0xF0;

    m_emittedPort1Status = true; // HACK to keep track of the current report position
    m_firstPeripheralReport = false;
    m_intbackInProgress = false; // TODO: should clear the flag only if all reports have been generated
}

void SMPC::SETSMEM() {
    rootLog.debug("Processing SETSMEM {:02X} {:02X} {:02X} {:02X}", IREG[0], IREG[1], IREG[2], IREG[3]);

    SMEM[0] = IREG[0];
    SMEM[1] = IREG[1];
    SMEM[2] = IREG[2];
    SMEM[3] = IREG[3];
    m_STE = true;
    WritePersistentData();

    SF = false; // done processing

    OREG[31] = 0x17;
}

void SMPC::SETTIME() {
    rootLog.debug("Processing SETTIME");

    util::datetime::DateTime dt{};
    dt.year = util::from_bcd((IREG[0] << 8u) + IREG[1]);
    dt.weekday = bit::extract<4, 7>(IREG[2]);
    dt.month = bit::extract<0, 3>(IREG[2]);
    dt.day = util::from_bcd(IREG[3]);
    dt.hour = util::from_bcd(IREG[4]);
    dt.minute = util::from_bcd(IREG[5]);
    dt.second = util::from_bcd(IREG[6]);

    rootLog.debug("Setting time to {}/{:02d}/{:02d} {:02d}:{:02d}:{:02d}", dt.year, dt.month, dt.day, dt.hour,
                  dt.minute, dt.second);

    m_rtc.SetDateTime(dt);
    WritePersistentData();

    SF = false; // done processing

    OREG[31] = 0x16;
}

void SMPC::ClockChange(bool fast) {
    m_saturn.VDP.Reset(false);
    m_saturn.SCU.Reset(false);
    m_saturn.SCSP.Reset(false);

    // TODO: clear VDP VRAMs

    m_saturn.SH2.slaveEnabled = false;

    m_saturn.SH2.master.SetNMI();

    m_saturn.SetClockRatios(fast);

    // TODO: PAL flag
    m_rtc.SetSysClockRatio(fast, false);
}

} // namespace satemu::smpc
