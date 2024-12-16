#include <satemu/hw/cdblock/cdblock.hpp>

#include <satemu/hw/scu/scu.hpp>

#include <satemu/util/data_ops.hpp>

namespace satemu::cdblock {

CDBlock::CDBlock(scu::SCU &scu)
    : m_scu(scu) {
    Reset(true);
}

void CDBlock::Reset(bool hard) {
    m_CR[0] = 0x0043; // ' C'
    m_CR[1] = 0x4442; // 'DB'
    m_CR[2] = 0x4C4F; // 'LO'
    m_CR[3] = 0x434B; // 'CK'

    m_status.statusCode = kStatusCodePause;
    m_status.frameAddress = 0x1FFFFFF;
    m_status.flagsRepeat = 0xFF;
    m_status.controlADR = 0xFF;
    m_status.track = 0xFF;
    m_status.index = 0xFF;

    m_readSpeed = 1;

    m_getSectorLength = 2048;
    m_putSectorLength = 2048;

    m_discAuthStatus = 0;
    m_mpegAuthStatus = 0;

    m_HIRQ = 0x0BC1; // 0x0BE1;
    m_HIRQMASK = 0;

    m_transferType = TransferType::None;
    m_transferPos = 0;
    m_transferLength = 0;
    m_transferCount = 0x1FFFFFF;

    for (auto &filter : m_filters) {
        filter.Reset();
    }
    m_cdDeviceConnection = media::Filter::kDisconnected;

    m_processingCommand = false;
    m_currCommandCycles = 0;
    m_targetCommandCycles = 0;

    m_readyForPeriodicReports = false;
    m_currPeriodicReportCycles = 0;
    m_targetPeriodicReportCycles = kPeriodicCyclesNotPlaying;
}

void CDBlock::LoadDisc(media::Disc &&disc) {
    m_disc.Swap(std::move(disc));
    // TODO: update status

    // Try building filesystem structure
    if (m_fs.Read(m_disc)) {
        fmt::println("CDBlock: Filesystem built successfully");
    } else {
        fmt::println("CDBlock: Failed to build filesystem");
    }
}

void CDBlock::EjectDisc() {
    m_disc = {};
    // TODO: update status
}

void CDBlock::OpenTray() {
    // TODO: implement
}

void CDBlock::CloseTray() {
    // TODO: implement
}

void CDBlock::Advance(uint64 cycles) {
    if (m_targetCommandCycles > 0) {
        m_currCommandCycles += cycles;
        if (m_currCommandCycles >= m_targetCommandCycles) {
            ProcessCommand();
            m_targetCommandCycles = 0;
        }
    }

    m_currPeriodicReportCycles += cycles * 3;
    if (m_currPeriodicReportCycles >= m_targetPeriodicReportCycles) {
        m_currPeriodicReportCycles -= m_targetPeriodicReportCycles;
        if (m_readyForPeriodicReports && !m_processingCommand) {
            m_status.statusCode |= kStatusFlagPeriodic;
            ReportCDStatus();
            SetInterrupt(kHIRQ_SCDQ);
        }
    }
}

void CDBlock::SetInterrupt(uint16 bits) {
    m_HIRQ |= bits;
    UpdateInterrupts();
}

void CDBlock::UpdateInterrupts() {
    // fmt::println("CDBlock: HIRQ = {:04X}  mask = {:04X}  active = {:04X}", m_HIRQ, m_HIRQMASK, m_HIRQ & m_HIRQMASK);
    if (m_HIRQ & m_HIRQMASK) {
        m_scu.TriggerExternalInterrupt0();
    }
}

void CDBlock::ReportCDStatus() {
    ReportCDStatus(m_status.statusCode);
}

void CDBlock::ReportCDStatus(uint8 statusCode) {
    m_CR[0] = (statusCode << 8u) | m_status.flagsRepeat;
    m_CR[1] = (m_status.controlADR << 8u) | m_status.track;
    m_CR[2] = (m_status.index << 8u) | ((m_status.frameAddress >> 16u) & 0xFF);
    m_CR[3] = m_status.frameAddress;
}

void CDBlock::SetupTransfer(TransferType type) {
    m_transferType = type;
    m_transferPos = 0;
    m_transferLength = 0;
    m_transferCount = 0x1FFFFFF;

    switch (type) {
    case TransferType::None: fmt::println("CDBlock: Ending transfer"); break;
    case TransferType::TOC:
        fmt::println("CDBlock: Starting TOC transfer");
        m_transferLength = sizeof(media::Session::toc) / sizeof(uint16);
        m_transferCount = 0;
        break;
    default: assert(false); break; // this should never happen
    }
}

uint16 CDBlock::DoTransfer() {
    uint16 value = 0;
    switch (m_transferType) {
    case TransferType::None: value = 0; break;
    case TransferType::TOC: {
        const bool evenWord = (m_transferPos & 2) == 0;
        const std::size_t tocIndex = m_transferPos / sizeof(uint32);
        value = m_disc.sessions[0].toc[tocIndex] >> (evenWord * 16u);
        break;
    }
    default:
        assert(false); // this should never happen
        value = 0;
        break;
    }

    m_transferPos += sizeof(uint16);
    m_transferCount += sizeof(uint16);
    if (m_transferPos >= m_transferLength) {
        m_transferType = TransferType::None;
        m_transferPos = 0;
        m_transferLength = 0;
    }
    return value;
}

void CDBlock::DisconnectFilterInput(uint8 filterNumber) {
    for (auto &filter : m_filters) {
        if (filter.falseOutput == filterNumber) {
            filter.falseOutput = media::Filter::kDisconnected;
        }
    }
}

void CDBlock::SetupCommand() {
    m_targetCommandCycles = 50;
}

void CDBlock::ProcessCommand() {
    const uint8 cmd = m_CR[0] >> 8u;
    fmt::println("CDBlock: processing command {:02X}  {:04X} {:04X} {:04X} {:04X}", cmd, m_CR[0], m_CR[1], m_CR[2],
                 m_CR[3]);

    switch (cmd) {
    case 0x00: CmdGetStatus(); break;
    case 0x01: CmdGetHardwareInfo(); break;
    case 0x02: CmdGetTOC(); break;
    case 0x03: CmdGetSessionInfo(); break;
    case 0x04: CmdInitializeCDSystem(); break;
    // case 0x05: CmdOpenTray(); break;
    case 0x06: CmdEndDataTransfer(); break;
    // case 0x10: CmdPlayDisc(); break;
    // case 0x11: CmdSeekDisc(); break;
    // case 0x12: CmdScanDisc(); break;
    // case 0x20: CmdGetSubcodeQ_RW(); break;
    case 0x30: CmdSetCDDeviceConnection(); break;
    // case 0x31: CmdGetCDDeviceConnection(); break;
    // case 0x32: CmdGetLastBufferDest(); break;
    // case 0x40: CmdSetFilterRange(); break;
    // case 0x41: CmdGetFilterRange(); break;
    // case 0x42: CmdSetFilterSubheaderConditions(); break;
    // case 0x43: CmdGetFilterSubheaderConditions(); break;
    // case 0x44: CmdSetFilterMode(); break;
    // case 0x45: CmdGetFilterMode(); break;
    // case 0x46: CmdSetFilterConnection(); break;
    // case 0x47: CmdGetFilterConnection(); break;
    case 0x48: CmdResetSelector(); break;
    // case 0x50: CmdGetBufferSize(); break;
    // case 0x51: CmdGetSectorNumber(); break;
    // case 0x52: CmdCalculateActualSize(); break;
    // case 0x53: CmdGetActualSize(); break;
    // case 0x54: CmdGetSectorInfo(); break;
    // case 0x55: CmdExecuteFADSearch(); break;
    // case 0x56: CmdGetFADSearchResults(); break;
    case 0x60: CmdSetSectorLength(); break;
    // case 0x61: CmdGetSectorData(); break;
    // case 0x62: CmdDeleteSectorData(); break;
    // case 0x63: CmdGetThenDeleteSectorData(); break;
    // case 0x64: CmdPutSectorData(); break;
    // case 0x65: CmdCopySectorData(); break;
    // case 0x66: CmdMoveSectorData(); break;
    case 0x67: CmdGetCopyError(); break;
    case 0x70: CmdChangeDirectory(); break;
    // case 0x71: CmdReadDirectory(); break;
    // case 0x72: CmdGetFileSystemScope(); break;
    // case 0x73: CmdGetFileInfo(); break;
    // case 0x74: CmdReadFile(); break;
    case 0x75: CmdAbortFile(); break;

    // case 0x90: CmdMpegGetStatus(); break;
    // case 0x91: CmdMpegGetInterrupt(); break;
    // case 0x92: CmdMpegSetInterruptMask(); break;
    case 0x93:
        CmdMpegInit();
        break;
        // case 0x94: CmdMpegSetMode(); break;
        // case 0x95: CmdMpegPlay(); break;
        // case 0x96: CmdMpegSetDecodingMethod(); break;
        // case 0x9A: CmdMpegSetConnection(); break;
        // case 0x9B: CmdMpegGetConnection(); break;
        // case 0x9D: CmdMpegSetStream(); break;
        // case 0x9E: CmdMpegGetStream(); break;
        // case 0xA0: CmdMpegDisplay(); break;
        // case 0xA1: CmdMpegSetWindow(); break;
        // case 0xA2: CmdMpegSetBorderColor(); break;
        // case 0xA3: CmdMpegSetFade(); break;
        // case 0xA4: CmdMpegSetVideoEffects(); break;
        // case 0xAF: CmdMpegSetLSI(); break;

    case 0xE0: CmdAuthenticateDevice(); break;
    case 0xE1:
        CmdIsDeviceAuthenticated();
        break;
        // case 0xE2: CmdGetMpegROM(); break;

    default: fmt::println("CDBlock: unimplemented command {:02X}", cmd); break;
    }
}

void CDBlock::CmdGetStatus() {
    fmt::println("CDBlock: -> Get status");

    // Input structure:
    // 0x00     <blank>
    // <blank>
    // <blank>
    // <blank>

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdGetHardwareInfo() {
    fmt::println("CDBlock: -> Get hardware info");

    // Input structure:
    // 0x01     <blank>
    // <blank>
    // <blank>
    // <blank>

    // Output structure:
    // status code      <blank>
    // hardware flags   hardware version
    // <blank>          MPEG version (0 if unauthenticated)
    // drive version    drive revision
    m_CR[0] = m_status.statusCode << 8u;
    m_CR[1] = 0x0002;
    m_CR[2] = 0x0000;
    m_CR[3] = 0x0600;

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdGetTOC() {
    fmt::println("CDBlock: -> Get TOC");

    // Input structure:
    // 0x02     <blank>
    // <blank>
    // <blank>
    // <blank>

    SetupTransfer(TransferType::TOC);

    // Output structure:
    // status code   <blank>
    // TOC size in words
    // <blank>
    // <blank>
    m_CR[0] = m_status.statusCode << 8u;
    m_CR[1] = sizeof(media::Session::toc) / sizeof(uint16);
    m_CR[2] = 0x0000;
    m_CR[3] = 0x0000;

    // TODO: make busy for a brief moment
    m_status.statusCode = kStatusCodePause;

    SetInterrupt(kHIRQ_CMOK | kHIRQ_DRDY);
}

void CDBlock::CmdGetSessionInfo() {
    fmt::println("CDBlock: -> Get session info");

    // Input structure:
    // 0x03     session data type (00 = all, others = specific session)
    // <blank>
    // <blank>
    // <blank>

    // Output structure:
    // status code        <blank>
    // <blank>
    // session num/count  lba bits 23-16
    // lba bits 15-0

    const uint8 sessionNum = bit::extract<0, 7>(m_CR[0]);
    if (sessionNum == 0) {
        // Get information about all sessions
        m_CR[2] = (m_disc.sessions.size() << 8u); // TODO: session LBA?
        m_CR[3] = 0x0000;
    } else if (sessionNum <= m_disc.sessions.size()) {
        // Get information about a specific session
        m_CR[2] = (sessionNum << 8u) | bit::extract<16, 23>(m_disc.sessions[sessionNum - 1].toc[101]);
        m_CR[3] = bit::extract<0, 15>(m_disc.sessions[sessionNum - 1].toc[101]);
    } else {
        // Return FFFFFFFF for nonexistent sessions
        m_CR[2] = 0xFFFF;
        m_CR[3] = 0xFFFF;
    }

    m_CR[0] = m_status.statusCode << 8u;
    m_CR[1] = 0x0000;

    // TODO: make busy for a brief moment
    m_status.statusCode = kStatusCodePause;

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdInitializeCDSystem() {
    fmt::println("CDBlock: -> Initialize CD system");

    // Input structure:
    // 0x04           initialization flags
    // standby time
    // <blank>
    // ECC            retry count
    const bool softReset = bit::extract<0>(m_CR[0]);
    // const bool decodeSubcodeRW = bit::extract<1>(m_CR[0]);
    // const bool ignoreMode2Subheader = bit::extract<2>(m_CR[0]);
    // const bool retryForm2Read = bit::extract<3>(m_CR[0]);
    const uint8 readSpeed = bit::extract<4, 5>(m_CR[0]); // 0=max (2x), 1=1x, 2=2x, 3=invalid
    // const bool keepSettings = bit::extract<7>(m_CR[0]);
    // const uint16 standbyTime = m_CR[1];
    // const uint8 ecc = bit::extract<8, 15>(m_CR[3]);
    // const uint8 retryCount = bit::extract<0, 7>(m_CR[3]);

    if (softReset) {
        fmt::println("CDBlock: Soft reset");
        // NOTE: switch to Busy, then Pause if disc is present
        m_status.statusCode = kStatusCodeNoDisc;
        // TODO: reset state and configuration
    }

    m_readSpeed = readSpeed == 1 ? 1 : 2;
    fmt::println("CDBlock: Read speed: {}x", m_readSpeed);

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdOpenTray() {}

void CDBlock::CmdEndDataTransfer() {
    fmt::println("CDBlock: -> End data transfer");

    // Input structure:
    // 0x06     <blank>
    // <blank>
    // <blank>
    // <blank>

    // TODO: stop any ongoing transfer
    // - trigger kHIRQ_EHST on Get Sector Data, Get Then Delete Sector or Put Sector

    // Output structure:
    // status code      transferred word count bits 23-16
    // transferred word count bits 15-0
    // <blank>
    // <blank>
    const uint32 wordCount = m_transferCount / sizeof(uint16);
    m_CR[0] = (m_status.statusCode << 8u) | (wordCount >> 16u);
    m_CR[1] = wordCount;
    m_CR[2] = 0x0000;
    m_CR[3] = 0x0000;

    m_transferCount = 0x1FFFFFF;

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdPlayDisc() {}

void CDBlock::CmdSeekDisc() {}

void CDBlock::CmdScanDisc() {}

void CDBlock::CmdGetSubcodeQ_RW() {}

void CDBlock::CmdSetCDDeviceConnection() {
    fmt::println("CDBlock: -> Set CD device connection");

    // Input structure:
    // 0x30           <blank>
    // <blank>
    // filter number  <blank>
    // <blank>
    const uint8 filterNumber = bit::extract<8, 15>(m_CR[2]);

    if (filterNumber < m_filters.size()) {
        // Connect CD to specified filter
        m_cdDeviceConnection = filterNumber;
        DisconnectFilterInput(filterNumber);
    } else if (filterNumber == media::Filter::kDisconnected) {
        // Disconnect CD
        m_cdDeviceConnection = media::Filter::kDisconnected;
    }

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdGetCDDeviceConnection() {}

void CDBlock::CmdGetLastBufferDest() {}

void CDBlock::CmdSetFilterRange() {}

void CDBlock::CmdGetFilterRange() {}

void CDBlock::CmdSetFilterSubheaderConditions() {}

void CDBlock::CmdGetFilterSubheaderConditions() {}

void CDBlock::CmdSetFilterMode() {}

void CDBlock::CmdGetFilterMode() {}

void CDBlock::CmdSetFilterConnection() {}

void CDBlock::CmdGetFilterConnection() {}

void CDBlock::CmdResetSelector() {
    fmt::println("CDBlock: -> Reset selector");

    // Input structure:
    // 0x48           reset flags
    // <blank>
    // buffer number  <blank>
    // <blank>
    const uint8 resetFlags = bit::extract<0, 7>(m_CR[0]);

    if (resetFlags == 0) {
        const uint8 bufferNumber = bit::extract<8, 15>(m_CR[2]);
        // TODO: clear everything in the specified buffer only

        fmt::println("CDBlock: clearing all data for buffer {}", bufferNumber);
    } else {
        const bool clearBufferData = bit::extract<2>(resetFlags);
        const bool clearSectionOutputs = bit::extract<3>(resetFlags);
        const bool clearFilterConditions = bit::extract<4>(resetFlags);
        const bool clearFilterInputs = bit::extract<5>(resetFlags);
        const bool clearFilterTrueOutputs = bit::extract<6>(resetFlags);
        const bool clearFilterFalseOutputs = bit::extract<7>(resetFlags);

        if (clearBufferData) {
            fmt::println("CDBlock: clearing all buffer data");
            // TODO: clear
        }
        if (clearSectionOutputs) {
            fmt::println("CDBlock: clearing all section output connectors");
            // TODO: clear
        }
        if (clearFilterConditions) {
            fmt::println("CDBlock: clearing all filter conditions");
            for (auto &filter : m_filters) {
                filter.ResetConditions();
            }
        }
        if (clearFilterInputs) {
            fmt::println("CDBlock: clearing all filter input connectors");
            // TODO: is this correct?
            for (auto &filter : m_filters) {
                filter.falseOutput = media::Filter::kDisconnected;
            }
            m_cdDeviceConnection = media::Filter::kDisconnected;
        }
        if (clearFilterTrueOutputs) {
            fmt::println("CDBlock: clearing all true filter output connectors");
            for (int i = 0; auto &filter : m_filters) {
                filter.trueOutput = i;
                i++;
            }
        }
        if (clearFilterFalseOutputs) {
            fmt::println("CDBlock: clearing all false filter output connectors");
            for (auto &filter : m_filters) {
                filter.falseOutput = media::Filter::kDisconnected;
            }
        }
    }

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdGetBufferSize() {}

void CDBlock::CmdGetSectorNumber() {}

void CDBlock::CmdCalculateActualSize() {}

void CDBlock::CmdGetActualSize() {}

void CDBlock::CmdGetSectorInfo() {}

void CDBlock::CmdExecuteFADSearch() {}

void CDBlock::CmdGetFADSearchResults() {}

void CDBlock::CmdSetSectorLength() {
    fmt::println("CDBlock: -> Set sector length");

    // Input structure:
    // 0x60               get sector length
    // put sector length  <blank>
    // <blank>
    // <blank>
    const uint8 getSectorLength = bit::extract<0, 7>(m_CR[0]);
    const uint8 putSectorLength = bit::extract<8, 15>(m_CR[1]);

    static constexpr uint32 kSectorLengths[] = {
        2048, // user data
        2336, // + subheader (checksum, ECC)
        2340, // + header (sector offset and mode)
        2352, // + sync bytes
    };

    if (getSectorLength < 4) {
        m_getSectorLength = kSectorLengths[getSectorLength];
    }
    if (putSectorLength < 4) {
        m_putSectorLength = kSectorLengths[putSectorLength];
    }
    fmt::println("CDBlock: Sector lengths: get={} put={}", m_getSectorLength, m_putSectorLength);

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdGetSectorData() {}

void CDBlock::CmdDeleteSectorData() {}

void CDBlock::CmdGetThenDeleteSectorData() {}

void CDBlock::CmdPutSectorData() {}

void CDBlock::CmdCopySectorData() {}

void CDBlock::CmdMoveSectorData() {}

void CDBlock::CmdGetCopyError() {
    fmt::println("CDBlock: -> Get copy error");

    // Input structure:
    // 0x67     <blank>
    // <blank>
    // <blank>
    // <blank>

    // Output structure:
    // status code      error code
    // <blank>
    // <blank>
    // <blank>
    m_CR[0] = (m_status.statusCode << 8u) | 0x00; // TODO: async copy/move error code
    m_CR[1] = 0x0000;
    m_CR[2] = 0x0000;
    m_CR[3] = 0x0000;

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdChangeDirectory() {
    fmt::println("CDBlock: -> Change directory");

    // Input structure:
    // 0x70     <blank>
    // <blank>
    // filter number   file ID bits 23-16
    // file ID bits 15-0

    const uint8 filterNum = bit::extract<8, 15>(m_CR[2]);
    const uint32 fileID = (bit::extract<0, 7>(m_CR[2]) << 16u) | m_CR[3];

    // Output structure: standard CD status data
    if (filterNum < m_filters.size()) {
        // TODO: read from file system
        if (m_fs.ChangeDirectory(fileID, m_filters[filterNum])) {
            // succeeded
            ReportCDStatus();
        } else {
            // failed
            ReportCDStatus(kStatusReject);
        }
    } else if (filterNum == 0xFF) {
        ReportCDStatus(kStatusReject);
    } else {
        ReportCDStatus();
    }

    SetInterrupt(kHIRQ_CMOK | kHIRQ_EFLS);
}

void CDBlock::CmdReadDirectory() {}

void CDBlock::CmdGetFileSystemScope() {}

void CDBlock::CmdGetFileInfo() {}

void CDBlock::CmdReadFile() {}

void CDBlock::CmdAbortFile() {
    fmt::println("CDBlock: -> Abort file");

    // Input structure:
    // 0x75     <blank>
    // <blank>
    // <blank>
    // <blank>

    // TODO: abort file transfer

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_EFLS);
}

void CDBlock::CmdMpegGetStatus() {}

void CDBlock::CmdMpegGetInterrupt() {}

void CDBlock::CmdMpegSetInterruptMask() {}

void CDBlock::CmdMpegInit() {
    fmt::println("CDBlock: -> MPEG init");

    // Input structure:
    // 0x93     <blank>
    // <blank>
    // <blank>
    // <blank>

    // TODO: initialize MPEG stuff

    // Output structure:
    // status code (FF=unauthenticated)  <blank>
    // <blank>
    // <blank>
    // <blank>
    m_CR[0] = 0xFF00;
    m_CR[1] = 0;
    m_CR[2] = 0;
    m_CR[3] = 0;

    SetInterrupt(kHIRQ_CMOK | kHIRQ_MPED | kHIRQ_MPST);
}

void CDBlock::CmdMpegSetMode() {}

void CDBlock::CmdMpegPlay() {}

void CDBlock::CmdMpegSetDecodingMethod() {}

void CDBlock::CmdMpegSetConnection() {}

void CDBlock::CmdMpegGetConnection() {}

void CDBlock::CmdMpegSetStream() {}

void CDBlock::CmdMpegGetStream() {}

void CDBlock::CmdMpegDisplay() {}

void CDBlock::CmdMpegSetWindow() {}

void CDBlock::CmdMpegSetBorderColor() {}

void CDBlock::CmdMpegSetFade() {}

void CDBlock::CmdMpegSetVideoEffects() {}

void CDBlock::CmdMpegSetLSI() {}

void CDBlock::CmdAuthenticateDevice() {
    fmt::println("CDBlock: -> Authenticate device");

    // Input structure:
    // 0xE1    <blank>
    // authentication type (0x0000=CD, 0x0001=MPEG)
    // <blank>
    // <blank>

    const uint16 authType = m_CR[1];

    switch (authType) {
    case 0x0000:
        fmt::println("CDBlock: CD authentication");
        m_discAuthStatus = 4; // always authenticated ;)
        SetInterrupt(kHIRQ_EFLS | kHIRQ_CSCT);
        break;
    case 0x0001:
        fmt::println("CDBlock: MPEG authentication");
        m_mpegAuthStatus = 2;
        SetInterrupt(kHIRQ_MPED);
        break;
    default: fmt::println("CDBlock: unexpected authentication type {}", authType); break;
    }

    // TODO: make busy for a brief moment
    m_status.statusCode = kStatusCodePause;

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdIsDeviceAuthenticated() {
    fmt::println("CDBlock: -> Is device authenticated");

    // Input structure:
    // 0xE2    <blank>
    // authentication type (0x0000=CD, 0x0001=MPEG)
    // <blank>
    // <blank>

    const uint16 authType = m_CR[1];

    // Output structure:
    // status code  <blank>
    // authentication status
    // <blank>
    // <unknown>
    m_CR[0] = (m_status.statusCode << 8u);
    m_CR[1] = authType == 0x0000 ? m_discAuthStatus : m_mpegAuthStatus;
    m_CR[2] = 0;
    m_CR[3] = 0;

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdGetMpegROM() {}

} // namespace satemu::cdblock
