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

    m_HIRQ = 0;
    m_HIRQMASK = 0;
}

void CDBlock::Advance(uint64 cycles) {}

void CDBlock::UpdateInterrupts() {
    if (m_HIRQ & m_HIRQMASK) {
        m_scu.TriggerExternalInterrupt0();
    }
}

void CDBlock::CmdGetStatus() {}

void CDBlock::CmdGetHardwareInfo() {}

void CDBlock::CmdGetTOC() {}

void CDBlock::CmdGetSessionInfo() {}

void CDBlock::CmdInitializeCDSystem() {}

void CDBlock::CmdOpenTray() {}

void CDBlock::CmdEndDataTransfer() {}

void CDBlock::CmdPlayDisc() {}

void CDBlock::CmdSeekDisc() {}

void CDBlock::CmdScanDisc() {}

void CDBlock::CmdGetSubcodeQ_RW() {}

void CDBlock::CmdSetCDDeviceConnection() {}

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

void CDBlock::CmdResetSelector() {}

void CDBlock::CmdGetBufferSize() {}

void CDBlock::CmdGetSectorNumber() {}

void CDBlock::CmdCalculateActualSize() {}

void CDBlock::CmdGetActualSize() {}

void CDBlock::CmdGetSectorInfo() {}

void CDBlock::CmdExecuteFADSearch() {}

void CDBlock::CmdGetFADSearchResults() {}

void CDBlock::CmdSetSectorLength() {}

void CDBlock::CmdGetSectorData() {}

void CDBlock::CmdDeleteSectorData() {}

void CDBlock::CmdGetThenDeleteSectorData() {}

void CDBlock::CmdPutSectorData() {}

void CDBlock::CmdCopySectorData() {}

void CDBlock::CmdMoveSectorData() {}

void CDBlock::CmdGetCopyError() {}

void CDBlock::CmdChangeDirectory() {}

void CDBlock::CmdReadDirectory() {}

void CDBlock::CmdGetFileSystemScope() {}

void CDBlock::CmdGetFileInfo() {}

void CDBlock::CmdReadFile() {}

void CDBlock::CmdAbortFile() {}

} // namespace satemu::cdblock
