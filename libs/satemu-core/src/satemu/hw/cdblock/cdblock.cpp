#include <satemu/hw/cdblock/cdblock.hpp>

#include <satemu/hw/scu/scu.hpp>

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
    m_status.frameAddress = 0xFFFFFF;
    m_status.flags = 0xF;
    m_status.repeatCount = 0xF;
    m_status.controlADR = 0xFF;
    m_status.track = 0xFF;
    m_status.index = 0xFF;

    m_readyForPeriodicReports = false;

    m_currDriveCycles = 0;
    m_targetDriveCycles = kDriveCyclesNotPlaying;

    m_playStartParam = 0;
    m_playEndParam = 0;
    m_playRepeatParam = 0;

    m_playStartPos = 0;
    m_playEndPos = 0;

    m_readSpeed = 1;

    m_discAuthStatus = 0;
    m_mpegAuthStatus = 0;

    m_HIRQ = 0x0BC1; // 0x0BE1;
    m_HIRQMASK = 0;

    m_transferType = TransferType::None;
    m_transferPos = 0;
    m_transferLength = 0;
    m_transferCount = 0x1FFFFFF;

    m_bufferManager.Reset();
    m_partitionManager.Reset();

    for (int i = 0; auto &filter : m_filters) {
        filter.Reset();
        filter.trueOutput = i;
        i++;
    }
    m_cdDeviceConnection = media::Filter::kDisconnected;
    m_lastCDWritePartition = ~0;

    m_getSectorLength = 2048;
    m_putSectorLength = 2048;

    m_processingCommand = false;
    m_currCommandCycles = 0;
    m_targetCommandCycles = 0;
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

    m_currDriveCycles += cycles * 3;
    if (m_currDriveCycles >= m_targetDriveCycles) {
        m_currDriveCycles -= m_targetDriveCycles;
        ProcessDriveState();

        if (m_readyForPeriodicReports && !m_processingCommand) {
            // HACK to ensure the system detects the absence of a disc properly
            if (m_disc.sessions.empty()) {
                m_status.statusCode = kStatusCodeNoDisc;
                m_targetDriveCycles = kDriveCyclesNotPlaying;
            }
            m_status.statusCode |= kStatusFlagPeriodic;
            ReportCDStatus();
            SetInterrupt(kHIRQ_SCDQ);
        }
    }
}

bool CDBlock::SetupPlayback(uint32 startParam, uint32 endParam, uint16 repeatParam) {
    // Handle "no change" parameters
    if (startParam == 0xFFFFFF) {
        startParam = m_playStartParam;
    }
    if (endParam == 0xFFFFFF) {
        endParam = m_playEndParam;
    }
    if (repeatParam == 0xFF) {
        repeatParam = m_playRepeatParam;
    }

    const bool isStartFAD = bit::extract<23>(startParam);
    const bool isEndFAD = bit::extract<23>(endParam);
    const bool resetPos = bit::extract<15>(repeatParam);

    // Sanity check: both must be FADs or tracks, not a mix
    if (isStartFAD != isEndFAD) {
        fmt::println("CDBlock: playback start: start/end FAD type mismatch: {:06X} {:06X}", startParam, endParam);
        return false; // reject
    }

    // Store playback parameters
    m_playStartParam = startParam;
    m_playEndParam = endParam;
    m_playRepeatParam = repeatParam & 0xF;

    // Make sure we have a disc
    if (m_disc.sessions.empty()) {
        fmt::println("CDBlock: playback start: no disc");
        m_status.statusCode = kStatusCodeNoDisc;
        m_targetDriveCycles = kDriveCyclesNotPlaying;
        return true;
    }

    const media::Session &session = m_disc.sessions.back();

    if (isStartFAD) {
        // Frame address range
        m_playStartPos = startParam & 0x7FFFFF;
        m_playEndPos = m_playStartPos + (endParam & 0x7FFFFF) - 1;

        fmt::println("CDBlock: playback start: FAD range {:06X} to {:06X}", m_playStartPos, m_playEndPos);

        // Find track containing the requested start frame address
        const uint8 trackIndex = session.FindTrackIndex(m_playStartPos);
        if (trackIndex != 0xFF) {
            m_status.statusCode = kStatusCodeSeek;
            m_status.flags = 0x8;     // CD-ROM decoding flag
            m_status.repeatCount = 0; // first repeat
            m_status.controlADR = session.tracks[trackIndex].controlADR;
            m_status.track = trackIndex + 1;
            m_status.index = 1; // TODO: handle indexes

            // TODO: delay seek for a realistic amount of time
            if (m_status.controlADR == 0x41) {
                m_targetDriveCycles = kDriveCyclesPlaying1x * m_readSpeed;
            } else {
                // Force 1x speed if playing audio track
                m_targetDriveCycles = kDriveCyclesPlaying1x;
            }

            fmt::println("CDBlock: playback start: track:index {:02d}:{:02d} ctl/ADR={:02X}", m_status.track,
                         m_status.index, m_status.controlADR);

            if (resetPos) {
                m_status.frameAddress = m_playStartPos;
                fmt::println("CDBlock: playback start: reset playback position to {:06X}", m_status.frameAddress);
            }
        } else {
            m_targetDriveCycles = kDriveCyclesNotPlaying;
            m_status.statusCode = kStatusCodePause;
        }
    } else {
        // Track range

        // startParam and endParam contain the track number on the upper byte and index on the lower byte
        uint8 startTrack = bit::extract<8, 15>(startParam);
        uint8 endTrack = bit::extract<8, 15>(endParam);
        uint8 startIndex = bit::extract<0, 7>(startParam);
        uint8 endIndex = bit::extract<0, 7>(endParam);

        // Handle default parameters - use first or last track and index in the disc
        if (startParam == 0) {
            startTrack = session.firstTrackIndex + 1;
            startIndex = 1;
        }
        if (endParam == 0) {
            endTrack = session.firstTrackIndex + session.numTracks;
            endIndex = 1;
        }

        fmt::println("CDBlock: playback start: track:index range {:02d}:{:02d}-{:02d}{:02d} ", startTrack, startIndex,
                     endTrack, endIndex);

        // TODO: implement; the code below is incorrect and untested

        /*
        // Clamp track numbers to what's available in the disc
        // If end < start, ProcessDriveState() will switch to the Pause state automatically
        uint8 firstTrack = session.firstTrackIndex + 1;
        uint8 lastTrack = firstTrack + session.numTracks - 1;
        startTrack = std::clamp(startTrack, firstTrack, lastTrack);
        endTrack = std::clamp(endTrack, firstTrack, lastTrack);

        // TODO: Track needs to store Index information

        fmt::println("CDBlock: playback start: track range after clamping {:02d}-{:02d}", startTrack, endTrack);

        // Get frame address range for the specified tracks
        if (m_status.statusCode != kStatusCodePause) {
            m_playStartParam = session.tracks[startTrack - 1].startFrameAddress;
            m_playEndParam = session.tracks[endTrack - 1].endFrameAddress;
            fmt::println("CDBlock: playback start: track FAD range {:06X}-{:06X}", m_playStartParam, m_playEndParam);
        }*/
    }

    return true;
}

void CDBlock::ProcessDriveState() {
    switch (m_status.statusCode & 0xF) {
    case kStatusCodeSeek:
        m_targetDriveCycles = kDriveCyclesPlaying1x * m_readSpeed;
        m_status.statusCode = kStatusCodePlay;
        m_status.frameAddress = m_playStartPos;
        break;
    case kStatusCodePlay: ProcessDriveStatePlay(); break;
    }
}

void CDBlock::ProcessDriveStatePlay() {
    if (m_status.frameAddress <= m_playEndPos) {
        if (m_cdDeviceConnection != media::Filter::kDisconnected) [[likely]] {
            assert(m_cdDeviceConnection < m_filters.size());

            fmt::println("CDBlock: playback: read from frame address {:06X}", m_status.frameAddress);

            Buffer *buffer = m_bufferManager.Allocate();
            if (buffer == nullptr) [[unlikely]] {
                fmt::println("CDBlock: playback: no free buffer available");

                m_status.statusCode = kStatusCodePause;
                SetInterrupt(kHIRQ_BFUL);
                // TODO: when buffer no longer full, switch to Play if we paused because of BFUL
                // - or maybe if m_status.frameAddress <= m_playEndPos
            } else if (m_disc.sessions.empty()) [[unlikely]] {
                fmt::println("CDBlock: playback: disc removed");

                m_status.statusCode = kStatusCodeNoDisc; // TODO: is this correct?
                SetInterrupt(kHIRQ_DCHG);
            } else {
                // TODO: consider caching the track pointer
                const media::Session &session = m_disc.sessions.back();
                const media::Track *track = session.FindTrack(m_status.frameAddress);

                // Sanity check: is the track valid?
                if (track != nullptr) [[likely]] {
                    buffer->size = track->ReadSectorRaw(m_status.frameAddress, buffer->data);

                    fmt::println("CDBlock: playback: read {} bytes", buffer->size);

                    // Check against filter and send data to the appropriate destination
                    uint8 filterNum = m_cdDeviceConnection;
                    for (int i = 0; i < 24 && filterNum != media::Filter::kDisconnected; i++) {
                        const media::Filter &filter = m_filters[filterNum];
                        if (filter.Test({buffer->data.begin(), buffer->size})) {
                            if (filter.trueOutput == media::Filter::kDisconnected) [[unlikely]] {
                                fmt::println("CDBlock: passed filter; output disconnected - discarded");
                                m_bufferManager.Free(buffer);
                            } else {
                                assert(filter.trueOutput < m_filters.size());
                                fmt::println("CDBlock: passed filter; sent to buffer partition {}", filter.trueOutput);
                                m_partitionManager.InsertHead(filter.trueOutput, buffer);
                                m_lastCDWritePartition = filter.trueOutput;
                            }
                            break;
                        } else {
                            if (filter.falseOutput == media::Filter::kDisconnected) [[unlikely]] {
                                fmt::println("CDBlock: failed filter; output disconnected - discarded");
                                m_bufferManager.Free(buffer);
                                break;
                            } else {
                                assert(filter.falseOutput < m_filters.size());
                                fmt::println("CDBlock: failed filter; sent to filter {}", filter.falseOutput);
                                filterNum = filter.falseOutput;
                            }
                        }
                    }

                    SetInterrupt(kHIRQ_CSCT);
                } else {
                    // This shouldn't really happen unless we're given an invalid disc image
                    // Let's pretend this is a disc read error
                    // TODO: what happens on a real disc read error?
                    fmt::println("CDBlock: playback: track not found");
                    m_status.statusCode = kStatusCodeError;
                }
            }
        } else {
            fmt::println("CDBlock: playback: read from {:06X} discarded", m_status.frameAddress);
        }

        m_status.frameAddress++;
    }

    if (m_status.frameAddress > m_playEndPos) {
        // 0x0 to 0xE = 0 to 14 repeats
        // 0xF = infinite repeats
        if (m_playRepeatParam == 0xF || m_status.repeatCount < m_playRepeatParam) {
            if (m_playRepeatParam == 0xF) {
                fmt::println("CDBlock: playback repeat (infinite)");
            } else {
                fmt::println("CDBlock: playback repeat: {} of {}", m_status.repeatCount + 1, m_playRepeatParam);
            }
            m_status.frameAddress = m_playStartPos;
            m_status.repeatCount++;
        } else {
            fmt::println("CDBlock: playback ended");
            m_status.frameAddress = m_playEndPos;
            m_status.statusCode = kStatusCodePause;
            m_targetDriveCycles = kDriveCyclesNotPlaying;
            SetInterrupt(kHIRQ_PEND);
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
    m_CR[0] = (statusCode << 8u) | (m_status.flags << 4u) | (m_status.repeatCount);
    m_CR[1] = (m_status.controlADR << 8u) | m_status.track;
    m_CR[2] = (m_status.index << 8u) | ((m_status.frameAddress >> 16u) & 0xFF);
    m_CR[3] = m_status.frameAddress;
}

void CDBlock::SetupTransfer(TransferType type) {
    // const TransferType prevType = m_transferType;
    m_transferType = type;
    m_transferPos = 0;
    m_transferLength = 0;
    m_transferCount = 0x1FFFFFF;

    switch (type) {
    case TransferType::None:
        fmt::println("CDBlock: Ending transfer");
        // TODO:
        /*if (prevType == TransferType::GetSector || prevType == TransferType::GetThenDeleteSector ||
            prevType == TransferType::PutSector) {
            SetInterrupt(kHIRQ_EHST);
        }*/
        break;
    case TransferType::TOC:
        fmt::println("CDBlock: Starting TOC transfer");
        m_transferLength = sizeof(media::Session::toc) / sizeof(uint16);
        m_transferCount = 0;
        break;
    default: assert(false); break; // this should never happen
    }
}

uint16 CDBlock::DoReadTransfer() {
    uint16 value = 0;
    switch (m_transferType) {
    case TransferType::TOC:
        if (m_disc.sessions.empty()) {
            value = 0xFFFF;
        } else {
            const bool evenWord = (m_transferPos & 1) == 0;
            const std::size_t tocIndex = m_transferPos * sizeof(uint16) / sizeof(uint32);
            value = m_disc.sessions.back().toc[tocIndex] >> (evenWord * 16u);
        }
        break;
    default: value = 0; break; // write-only or no active transfer
    }

    AdvanceTransfer();

    return value;
}

void CDBlock::DoWriteTransfer(uint16 value) {
    // TODO: implement write transfers
    /*switch (m_transferType) {
    }*/

    AdvanceTransfer();
}

void CDBlock::AdvanceTransfer() {
    m_transferPos++;
    m_transferCount++;
    if (m_transferPos >= m_transferLength) {
        m_transferType = TransferType::None;
        m_transferPos = 0;
        m_transferLength = 0;
    }
}

void CDBlock::DisconnectFilterInput(uint8 filterNumber) {
    if (m_cdDeviceConnection == filterNumber) {
        m_cdDeviceConnection = media::Filter::kDisconnected;
    }
    for (auto &filter : m_filters) {
        if (filter.falseOutput == filterNumber) {
            filter.falseOutput = media::Filter::kDisconnected;
            break; // there can be only one input connection to a filter
        }
    }
}

void CDBlock::SetupCommand() {
    m_targetCommandCycles = 50;
}

void CDBlock::ProcessCommand() {
    const uint8 cmd = m_CR[0] >> 8u;
    fmt::println("CDBlock: processing command {:04X} {:04X} {:04X} {:04X}", m_CR[0], m_CR[1], m_CR[2], m_CR[3]);

    switch (cmd) {
    case 0x00: CmdGetStatus(); break;
    case 0x01: CmdGetHardwareInfo(); break;
    case 0x02: CmdGetTOC(); break;
    case 0x03: CmdGetSessionInfo(); break;
    case 0x04: CmdInitializeCDSystem(); break;
    // case 0x05: CmdOpenTray(); break;
    case 0x06: CmdEndDataTransfer(); break;
    case 0x10: CmdPlayDisc(); break;
    case 0x11: CmdSeekDisc(); break;
    // case 0x12: CmdScanDisc(); break;
    // case 0x20: CmdGetSubcodeQ_RW(); break;
    case 0x30: CmdSetCDDeviceConnection(); break;
    case 0x31: CmdGetCDDeviceConnection(); break;
    case 0x32: CmdGetLastBufferDest(); break;
    case 0x40: CmdSetFilterRange(); break;
    case 0x41: CmdGetFilterRange(); break;
    case 0x42: CmdSetFilterSubheaderConditions(); break;
    case 0x43: CmdGetFilterSubheaderConditions(); break;
    case 0x44: CmdSetFilterMode(); break;
    case 0x45: CmdGetFilterMode(); break;
    case 0x46: CmdSetFilterConnection(); break;
    case 0x47: CmdGetFilterConnection(); break;
    case 0x48: CmdResetSelector(); break;
    case 0x50: CmdGetBufferSize(); break;
    case 0x51: CmdGetSectorNumber(); break;
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
    m_targetDriveCycles = kDriveCyclesNotPlaying;

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

    m_CR[0] = m_status.statusCode << 8u;
    m_CR[1] = 0x0000;

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

    // TODO: make busy for a brief moment
    m_status.statusCode = kStatusCodePause;
    m_targetDriveCycles = kDriveCyclesNotPlaying;

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
        // TODO: switch to Busy for a bit before NoDisc/Pause
        if (m_disc.sessions.empty()) {
            m_status.statusCode = kStatusCodeNoDisc;
        } else {
            m_status.statusCode = kStatusCodePause;
        }
        m_targetDriveCycles = kDriveCyclesNotPlaying;
        // TODO: reset state and configuration
    }

    m_readSpeed = readSpeed == 1 ? 1 : 2;
    fmt::println("CDBlock: Read speed: {}x", m_readSpeed);

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdOpenTray() {
    fmt::println("CDBlock: -> Open tray");

    // Input structure:
    // 0x05     <blank>
    // <blank>
    // <blank>
    // <blank>

    // TODO: stay in Busy status while disc stops spinning
    m_status.statusCode = kStatusCodeOpen;
    m_discAuthStatus = 0;

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_EFLS | kHIRQ_DCHG);
}

void CDBlock::CmdEndDataTransfer() {
    fmt::println("CDBlock: -> End data transfer");

    // Input structure:
    // 0x06     <blank>
    // <blank>
    // <blank>
    // <blank>

    const uint32 transferCount = m_transferCount;

    SetupTransfer(TransferType::None);

    // Output structure:
    // status code      transferred word count bits 23-16
    // transferred word count bits 15-0
    // <blank>
    // <blank>
    m_CR[0] = (m_status.statusCode << 8u) | (transferCount >> 16u);
    m_CR[1] = transferCount;
    m_CR[2] = 0x0000;
    m_CR[3] = 0x0000;

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdPlayDisc() {
    fmt::println("CDBlock: -> Play disc");

    // Input structure:
    // 0x10           start position bits 23-16
    // start position bits 15-0
    // play mode      end position bits 23-16
    // end position bits 15-0
    const uint8 repeatParam = bit::extract<8, 15>(m_CR[2]);
    const uint32 startParam = (bit::extract<0, 7>(m_CR[0]) << 16u) | m_CR[1];
    const uint32 endParam = (bit::extract<0, 7>(m_CR[2]) << 16u) | m_CR[3];

    fmt::println("CDBlock: start={:06X} end={:06X} repeat={:X}", startParam, endParam, repeatParam);

    // Output structure: standard CD status data
    if (SetupPlayback(startParam, endParam, repeatParam)) {
        ReportCDStatus();
    } else {
        ReportCDStatus(kStatusReject);
    }

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdSeekDisc() {
    fmt::println("CDBlock: -> Seek disc");

    // Input structure:
    // 0x11           start position bits 23-16
    // start position bits 15-0
    // <blank>
    // <blank>
    const uint32 startPos = (bit::extract<0, 7>(m_CR[0]) << 16u) | m_CR[1];
    // const bool isStartFAD = bit::extract<23>(startPos);

    fmt::println("CDBlock: Seek start {:06X}", startPos);
    // TODO: implement
    // isStartFAD:
    //   true: startPos is FAD (& 0x7FFFFF)
    //   false: startPos is track number
    // stops playing if status is Play
    // status after running the command: -> Paused
    // setting invalid track: -> Standby
    // startPos = 0xFFFFFF: stops playing and -> Paused

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdScanDisc() {
    fmt::println("CDBlock: -> Scan disc");

    // Input structure:
    // 0x12     scan direction
    // <blank>
    // <blank>
    // <blank>
    const uint8 direction = bit::extract<0, 7>(m_CR[0]);

    // Output structure: standard CD status data
    if (direction < 2) {
        m_status.statusCode = kStatusCodeBusy;
        // TODO: SetupScan(direction);
        ReportCDStatus();
    } else {
        ReportCDStatus(kStatusReject);
    }

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdGetSubcodeQ_RW() {
    fmt::println("CDBlock: -> Get Subcode Q/RW");

    // Input structure:
    // 0x20     type
    // <blank>
    // <blank>
    // <blank>
    // const uint8 type = bit::extract<0, 7>(m_CR[0]);

    // TODO: handle types
    //   type 0 = Q subcode
    //   type 1 = R-W subcodes

    // Output structure if valid:
    // status code     <blank>
    // Q/RW size in words (Q = 5, RW = 12)
    // <blank>
    // subcode flags
    //
    // TODO: raise kHIRQ_DRDY if valid
    // TODO: setup read transfer if valid
    // - subcode Q: 5 words
    // - subcodes R-W: 12 words

    // Output structure if invalid:
    // 0x80   <blank>
    // <blank>
    // <blank>
    // <blank>
    m_CR[0] = 0x8000;
    m_CR[1] = 0x0000;
    m_CR[2] = 0x0000;
    m_CR[3] = 0x0000;

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdSetCDDeviceConnection() {
    fmt::println("CDBlock: -> Set CD device connection");

    // Input structure:
    // 0x30           <blank>
    // <blank>
    // filter number  <blank>
    // <blank>
    const uint8 filterNumber = bit::extract<8, 15>(m_CR[2]);

    bool reject = false;
    if (filterNumber < m_filters.size()) {
        // Connect CD to specified filter
        DisconnectFilterInput(filterNumber);
        m_cdDeviceConnection = filterNumber;
    } else if (filterNumber == media::Filter::kDisconnected) {
        // Disconnect CD
        m_cdDeviceConnection = media::Filter::kDisconnected;
    } else {
        reject = true;
    }

    // Output structure: standard CD status data
    if (reject) {
        ReportCDStatus(kStatusReject);
    } else {
        ReportCDStatus();
    }

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdGetCDDeviceConnection() {
    fmt::println("CDBlock: -> Get CD device connection");

    // Input structure:
    // 0x31           <blank>
    // <blank>
    // <blank>
    // <blank>

    // Output structure:
    // status code    <blank>
    // <blank>
    // filter number  <blank>
    // <blank>
    m_CR[0] = m_status.statusCode << 8u;
    m_CR[1] = 0x0000;
    m_CR[2] = m_cdDeviceConnection << 8u;
    m_CR[3] = 0x0000;

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdGetLastBufferDest() {
    fmt::println("CDBlock: -> Get last buffer destination");

    // Input structure:
    // 0x32     <blank>
    // <blank>
    // <blank>
    // <blank>

    // Output structure:
    // status code        <blank>
    // <blank>
    // partition number   <blank>
    // <blank>
    m_CR[0] = m_status.statusCode << 8u;
    m_CR[1] = 0x0000;
    m_CR[2] = m_lastCDWritePartition << 8u;
    m_CR[3] = 0x0000;

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdSetFilterRange() {
    fmt::println("CDBlock: -> Set filter range");

    // Input structure:
    // 0x40           start frame address bits 23-16
    // start frame address bits 15-0
    // filter number  frame address count bits 23-16
    // frame address count bits 15-0
    const uint8 filterNumber = bit::extract<8, 15>(m_CR[2]);

    if (filterNumber < 24) {
        const uint32 startFrameAddress = (bit::extract<0, 7>(m_CR[0]) << 16u) | m_CR[1];
        const uint32 frameAddressCount = (bit::extract<0, 7>(m_CR[2]) << 16u) | m_CR[3];
        auto &filter = m_filters[filterNumber];
        filter.startFrameAddress = startFrameAddress;
        filter.frameAddressCount = frameAddressCount;

        // Output structure: standard CD status data
        ReportCDStatus();
    } else {
        ReportCDStatus(kStatusReject);
    }

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdGetFilterRange() {
    fmt::println("CDBlock: -> Get filter range");

    // Input structure:
    // 0x41           <blank>
    // <blank>
    // filter number  <blank>
    // <blank>
    const uint8 filterNumber = bit::extract<8, 15>(m_CR[2]);

    if (filterNumber < 24) {
        // Output structure:
        // status code    start frame address bits 23-16
        // start frame address bits 15-0
        // filter number  frame address count bits 23-16
        // frame address count bits 15-0
        const auto &filter = m_filters[filterNumber];
        m_CR[0] = (m_status.statusCode << 8u) | (filter.startFrameAddress >> 16u);
        m_CR[1] = filter.startFrameAddress;
        m_CR[2] = (filterNumber << 8u) | (filter.frameAddressCount >> 16u);
        m_CR[3] = filter.frameAddressCount;
    } else {
        ReportCDStatus(kStatusReject);
    }

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdSetFilterSubheaderConditions() {
    fmt::println("CDBlock: -> Set filter subheader conditions");

    // Input structure:
    // 0x42           channel
    // submode mask   coding info mask
    // filter number  file ID
    // submode value  coding info value
    const uint8 filterNumber = bit::extract<8, 15>(m_CR[2]);

    if (filterNumber < 24) {
        const uint8 chanNum = bit::extract<0, 7>(m_CR[0]);
        const uint8 submodeMask = bit::extract<8, 15>(m_CR[1]);
        const uint8 codingInfoMask = bit::extract<0, 7>(m_CR[1]);
        const uint8 fileID = bit::extract<0, 7>(m_CR[2]);
        const uint8 submodeValue = bit::extract<8, 15>(m_CR[3]);
        const uint8 codingInfoValue = bit::extract<0, 7>(m_CR[3]);

        auto &filter = m_filters[filterNumber];
        filter.chanNum = chanNum;
        filter.fileNum = fileID;
        filter.submodeMask = submodeMask;
        filter.submodeValue = submodeValue;
        filter.codingInfoMask = codingInfoMask;
        filter.codingInfoValue = codingInfoValue;

        // Output structure: standard CD status data
        ReportCDStatus();
    } else {
        ReportCDStatus(kStatusReject);
    }

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdGetFilterSubheaderConditions() {
    fmt::println("CDBlock: -> Get filter subheader conditions");

    // Input structure:
    // 0x43           <blank>
    // <blank>
    // filter number  <blank>
    // <blank>
    const uint8 filterNumber = bit::extract<8, 15>(m_CR[2]);

    if (filterNumber < 24) {
        // Output structure:
        // status code    channel
        // submode mask   coding info mask
        // filter number  file ID
        // submode value  coding info value
        const auto &filter = m_filters[filterNumber];
        m_CR[0] = (m_status.statusCode << 8u) | filter.chanNum;
        m_CR[1] = (filter.submodeMask << 8u) | filter.codingInfoMask;
        m_CR[2] = (filterNumber << 8u) | filter.fileNum;
        m_CR[3] = (filter.submodeValue << 8u) | filter.codingInfoValue;
    } else {
        ReportCDStatus(kStatusReject);
    }

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdSetFilterMode() {
    fmt::println("CDBlock: -> Set filter mode");

    // Input structure:
    // 0x44           mode
    // <blank>
    // filter number  <blank>
    // <blank>
    const uint8 filterNumber = bit::extract<8, 15>(m_CR[2]);

    if (filterNumber < 24) {
        const uint8 mode = bit::extract<0, 7>(m_CR[0]);

        auto &filter = m_filters[filterNumber];
        filter.mode = mode & 0x5F; // TODO: should it be masked?
        if (mode & 0x80) {
            filter.ResetConditions();
        }

        // Output structure: standard CD status data
        ReportCDStatus();
    } else {
        ReportCDStatus(kStatusReject);
    }

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdGetFilterMode() {
    fmt::println("CDBlock: -> Get filter mode");

    // Input structure:
    // 0x45           <blank>
    // <blank>
    // filter number  <blank>
    // <blank>
    const uint8 filterNumber = bit::extract<8, 15>(m_CR[2]);

    if (filterNumber < 24) {
        // Output structure:
        // status code    mode
        // <blank>
        // filter number  <blank>
        // <blank>
        const auto &filter = m_filters[filterNumber];
        m_CR[0] = (m_status.statusCode << 8u) | filter.mode;
        m_CR[1] = 0x0000;
        m_CR[2] = (filterNumber << 8u);
        m_CR[3] = 0x0000;
    } else {
        ReportCDStatus(kStatusReject);
    }

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdSetFilterConnection() {
    fmt::println("CDBlock: -> Set filter connection");

    // Input structure:
    // 0x46           connection flags
    // true conn      false conn
    // filter number  <blank>
    // <blank>
    const bool setTrueConn = bit::extract<0>(m_CR[0]);
    const bool setFalseConn = bit::extract<1>(m_CR[0]);
    const uint8 trueConn = bit::extract<8, 15>(m_CR[1]);
    const uint8 falseConn = bit::extract<0, 7>(m_CR[1]);
    const uint8 filterNumber = bit::extract<8, 15>(m_CR[2]);

    if (filterNumber < 24) {
        auto &filter = m_filters[filterNumber];
        if (setTrueConn) {
            filter.trueOutput = trueConn;
        }
        if (setFalseConn) {
            DisconnectFilterInput(filterNumber);
            filter.falseOutput = falseConn;
        }

        // Output structure: standard CD status data
        ReportCDStatus();
    } else {
        ReportCDStatus(kStatusReject);
    }

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdGetFilterConnection() {
    fmt::println("CDBlock: -> Get filter connection");

    // Input structure:
    // 0x47           <blank>
    // <blank>
    // filter number  <blank>
    // <blank>
    const uint8 filterNumber = bit::extract<8, 15>(m_CR[2]);

    if (filterNumber < 24) {
        // Output structure:
        // status code    <blank>
        // true conn      false conn
        // <blank>
        // <blank>
        const auto &filter = m_filters[filterNumber];
        m_CR[0] = (m_status.statusCode << 8u);
        m_CR[1] = (filter.trueOutput << 8u) | filter.falseOutput;
        m_CR[2] = 0x0000;
        m_CR[3] = 0x0000;
    } else {
        ReportCDStatus(kStatusReject);
    }

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdResetSelector() {
    fmt::println("CDBlock: -> Reset selector");

    // Input structure:
    // 0x48              reset flags
    // <blank>
    // partition number  <blank>
    // <blank>
    const uint8 resetFlags = bit::extract<0, 7>(m_CR[0]);

    bool reject = false;
    if (resetFlags == 0) {
        const uint8 partitionNumber = bit::extract<8, 15>(m_CR[2]);
        fmt::println("CDBlock: clearing buffer partition {}", partitionNumber);
        if (partitionNumber < 24) {
            m_partitionManager.Clear(partitionNumber);
        } else {
            reject = true;
        }
    } else {
        const bool clearBufferData = bit::extract<2>(resetFlags);
        const bool clearPartitionOutputs = bit::extract<3>(resetFlags);
        const bool clearFilterConditions = bit::extract<4>(resetFlags);
        const bool clearFilterInputs = bit::extract<5>(resetFlags);
        const bool clearFilterTrueOutputs = bit::extract<6>(resetFlags);
        const bool clearFilterFalseOutputs = bit::extract<7>(resetFlags);

        if (clearBufferData) {
            fmt::println("CDBlock: clearing all buffer partitions");
            m_partitionManager.ClearAll();
        }
        if (clearPartitionOutputs) {
            fmt::println("CDBlock: clearing all partition output connectors");
            // TODO: clear device inputs and filter inputs connected to partition outputs
        }
        if (clearFilterConditions) {
            fmt::println("CDBlock: clearing all filter conditions");
            for (auto &filter : m_filters) {
                filter.ResetConditions();
            }
        }
        if (clearFilterInputs) {
            fmt::println("CDBlock: clearing all filter input connectors");
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
    if (reject) {
        ReportCDStatus(kStatusReject);
    } else {
        ReportCDStatus();
    }

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdGetBufferSize() {
    fmt::println("CDBlock: -> Get buffer size");

    // Input structure:
    // 0x50     <blank>
    // <blank>
    // <blank>
    // <blank>

    // Output structure:
    // status code          <blank>
    // free buffer count
    // total filter count   <blank>
    // total buffer count
    m_CR[0] = m_status.statusCode << 8u;
    m_CR[1] = m_bufferManager.FreeBufferCount();
    m_CR[2] = m_filters.size() << 8u;
    m_CR[3] = m_bufferManager.TotalBufferCount();

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdGetSectorNumber() {
    fmt::println("CDBlock: -> Get sector number");

    // Input structure:
    // 0x51              <blank>
    // <blank>
    // partition number  <blank>
    // <blank>
    const uint8 partitionNumber = bit::extract<8, 15>(m_CR[2]);

    // Output structure:
    // status code      <blank>
    // <blank>
    // <blank>
    // number of blocks
    m_CR[0] = (m_status.statusCode << 8u);
    m_CR[1] = 0x0000;
    m_CR[2] = 0x0000;
    m_CR[3] = m_partitionManager.GetBufferCount(partitionNumber);

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdCalculateActualSize() {
    fmt::println("CDBlock: -> Calculate actual size");

    // Input structure:
    // 0x52               <blank>
    // sector offset
    // partition number   <blank>
    // sector number
    // const uint16 sectorOffset = m_CR[1];
    // const uint8 partitionNumber = bit::extract<8, 15>(m_CR[2]);
    // const uint16 sectorNumber = m_CR[3];

    // TODO: calculate data size in words in specified partition starting from sectorOffset (0xFFFF = end) for
    // sectorNumber sectors (0xFFFF = until the end)

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdGetActualSize() {
    fmt::println("CDBlock: -> Get actual size");

    // Input structure:
    // 0x53     <blank>
    // <blank>
    // <blank>
    // <blank>

    // Output structure:
    // status code   calculated size bits 23-16 (in words)
    // calculated size bits 15-0 (in words)
    // <blank>
    // <blank>
    m_CR[0] = (m_status.statusCode << 8u); // TODO: calculated size
    m_CR[1] = 0;                           // TODO: calculated size
    m_CR[2] = 0x0000;
    m_CR[3] = 0x0000;

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdGetSectorInfo() {
    fmt::println("CDBlock: -> Get sector info");

    // Input structure:
    // 0x54               <blank>
    // <blank>            sector number
    // partition number   <blank>
    // <blank>
    // const uint8 sectorNumber = bit::extract<0, 7>(m_CR[1]);
    // const uint8 partitionNumber = bit::extract<8, 15>(m_CR[2]);

    // TODO:

    // Output structure:
    // status code          sector frame address bits 23-16
    // sector frame address bits 15-0
    // sector file number   sector coding number
    // sector submode       sector coding info
    m_CR[0] = (m_status.statusCode << 8u); // TODO: frame address high
    m_CR[1] = 0;                           // TODO: frame address low
    m_CR[2] = 0;                           // TODO: file number, coding number
    m_CR[3] = 0;                           // TODO: submode, coding info

    SetInterrupt(kHIRQ_CMOK);
}

void CDBlock::CmdExecuteFADSearch() {
    fmt::println("CDBlock: -> Execute frame address search");

    // Input structure:
    // 0x55     <blank>
    // sector position
    // partition number   frame address bits 23-16
    // frame address bits 15-0
    // const uint16 sectorPos = m_CR[1];
    // const uint8 partitionNumber = bit::extract<8, 15>(m_CR[2]);
    // const uint32 frameAddress = (bit::extract<0, 7>(m_CR[2]) << 16u) | m_CR[3];

    // TODO: search for a sector with the largest FAD <= searched FAD within specified partition
    // - how does sectorPos factor in here?

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ESEL);
}

void CDBlock::CmdGetFADSearchResults() {
    fmt::println("CDBlock: -> Get frame address search results");

    // Input structure:
    // 0x56     <blank>
    // <blank>
    // <blank>
    // <blank>

    // Output structure:
    // status code        <blank>
    // sector position
    // partition number   frame address bits 23-16
    // frame address bits 15-0
    m_CR[0] = (m_status.statusCode << 8u);
    m_CR[1] = 0; // TODO: sector position
    m_CR[2] = 0; // TODO: partition number, FAD high
    m_CR[3] = 0; // TODO: FAD low

    SetInterrupt(kHIRQ_CMOK);
}

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

void CDBlock::CmdGetSectorData() {
    fmt::println("CDBlock: -> Get sector data");

    // Input structure:
    // 0x61               <blank>
    // sector offset
    // partition number   <blank>
    // sector number
    // const uint16 sectorOffset = m_CR[1];
    // const uint8 partitionNumber = bit::extract<8, 15>(m_CR[2]);
    // const uint16 sectorNumber = m_CR[3];

    // TODO: setup sector read transfer
    // TODO: should set status flag kStatusFlagXferRequest until ready

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_EHST);
}

void CDBlock::CmdDeleteSectorData() {
    fmt::println("CDBlock: -> Delete sector data");

    // Input structure:
    // 0x62               <blank>
    // sector position
    // partition number   <blank>
    // sector number
    // const uint16 sectorPos = m_CR[1];
    // const uint8 partitionNumber = bit::extract<8, 15>(m_CR[2]);
    // const uint16 sectorNumber = m_CR[3];

    // TODO: setup sector delete
    // - if sectorPos is 0xFFFF, deletes sectorNumber sectors from the end
    // - if sectorNumber is 0xFFFF, deletes from sectorPos until the end

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_EHST);
}

void CDBlock::CmdGetThenDeleteSectorData() {
    fmt::println("CDBlock: -> Get then delete sector data");

    // Input structure:
    // 0x63               <blank>
    // sector offset
    // partition number   <blank>
    // sector number
    // const uint16 sectorOffset = m_CR[1];
    // const uint8 partitionNumber = bit::extract<8, 15>(m_CR[2]);
    // const uint16 sectorNumber = m_CR[3];

    // TODO: setup sector read transfer
    // TODO: should set status flag kStatusFlagXferRequest until ready

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_EHST | kHIRQ_DRDY);
}

void CDBlock::CmdPutSectorData() {
    fmt::println("CDBlock: -> Put sector data");

    // Input structure:
    // 0x64               <blank>
    // <blank>
    // partition number   <blank>
    // sector number
    // const uint8 partitionNumber = bit::extract<8, 15>(m_CR[2]);
    // const uint16 sectorNumber = m_CR[3];

    // TODO: setup sector write transfer
    // TODO: raise kHIRQ_EHST if not enough buffer space available
    // TODO: should set status flag kStatusFlagXferRequest until ready

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_DRDY);
}

void CDBlock::CmdCopySectorData() {
    fmt::println("CDBlock: -> Copy sector data");

    // Input structure:
    // 0x65                      destination filter number
    // sector offset
    // source partition number   <blank>
    // sector number
    // const uint8 dstPartitionNumber = bit::extract<0, 7>(m_CR[0]);
    // const uint16 sectorOffset = m_CR[1];
    // const uint8 srcPartitionNumber = bit::extract<8, 15>(m_CR[2]);
    // const uint16 sectorNumber = m_CR[3];

    // TODO: setup async sector copy transfer
    // TODO: report Reject status if not enough buffer space available

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ECPY);
}

void CDBlock::CmdMoveSectorData() {
    fmt::println("CDBlock: -> Move sector data");

    // Input structure:
    // 0x66                      destination filter number
    // sector offset
    // source partition number   <blank>
    // sector number
    // const uint8 dstPartitionNumber = bit::extract<0, 7>(m_CR[0]);
    // const uint16 sectorOffset = m_CR[1];
    // const uint8 srcPartitionNumber = bit::extract<8, 15>(m_CR[2]);
    // const uint16 sectorNumber = m_CR[3];

    // TODO: setup async sector move transfer

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_ECPY);
}

void CDBlock::CmdGetCopyError() {
    fmt::println("CDBlock: -> Get copy error");

    // Input structure:
    // 0x67     <blank>
    // <blank>
    // <blank>
    // <blank>

    // Output structure:
    // status code   error code
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
    // 0x70            <blank>
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

void CDBlock::CmdReadDirectory() {
    fmt::println("CDBlock: -> Read directory");

    // Input structure:
    // 0x71            <blank>
    // <blank>
    // filter number   file ID bits 23-16
    // file ID bits 15-0
    // const uint8 filterNum = bit::extract<8, 15>(m_CR[2]);
    // const uint32 fileID = (bit::extract<0, 7>(m_CR[2]) << 16u) | m_CR[3];

    // TODO: read directory contents starting from fileID
    // TODO: write sectors to specified filter

    // Output structure: standard CD status data
    ReportCDStatus();

    SetInterrupt(kHIRQ_CMOK | kHIRQ_EFLS);
}

void CDBlock::CmdGetFileSystemScope() {
    fmt::println("CDBlock: -> Get file system scope");

    // Input structure:
    // 0x72     <blank>
    // <blank>
    // <blank>
    // <blank>

    // Output structure:
    // status code            <blank>
    // index number
    // directory end offset   first file ID bits 23-16
    // first file ID bits 15-0
    m_CR[0] = m_status.statusCode << 8u;
    m_CR[1] = 0; // TODO: index number
    m_CR[2] = 0; // TODO: directory end offset, first file ID high
    m_CR[3] = 0; // TODO: first file ID low

    SetInterrupt(kHIRQ_CMOK | kHIRQ_EFLS);
}

void CDBlock::CmdGetFileInfo() {
    fmt::println("CDBlock: -> Get file info");

    // Input structure:
    // 0x73     <blank>
    // <blank>
    // <blank>  file ID bits 23-16
    // file ID bits 15-0
    // const uint32 fileID = (bit::extract<0, 7>(m_CR[2]) << 16u) | m_CR[3];

    // TODO: setup read transfer from the file info table

    // Output structure:
    // status code            <blank>
    // file info size in bytes
    // <blank>
    // <blank>
    m_CR[0] = m_status.statusCode << 8u;
    m_CR[1] = 0x0000; // TODO: file info size in bytes
    m_CR[2] = 0x0000;
    m_CR[3] = 0x0000;

    SetInterrupt(kHIRQ_CMOK | kHIRQ_DRDY);
}

void CDBlock::CmdReadFile() {
    fmt::println("CDBlock: -> Get file info");

    // Input structure:
    // 0x74            offset bits 23-16
    // offset bits 15-0
    // filter number   file ID bits 23-16
    // file ID bits 15-0
    // const uint32 offset = (bit::extract<0, 7>(m_CR[0]) << 16u) | m_CR[1];
    // const uint8 filterNumber = bit::extract<8, 15>(m_CR[2]);
    // const uint32 fileID = (bit::extract<0, 7>(m_CR[2]) << 16u) | m_CR[3];

    // TODO: setup file "playback"

    // Output structure: standard CD status data
    ReportCDStatus();

    // TODO: trigger kHIRQ_EFLS when done reading the entire file(last frame written to buffer)

    SetInterrupt(kHIRQ_CMOK | kHIRQ_DRDY);
}

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
    m_targetDriveCycles = kDriveCyclesNotPlaying;

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
