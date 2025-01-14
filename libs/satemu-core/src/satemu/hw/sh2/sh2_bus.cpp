#include <satemu/hw/sh2/sh2_bus.hpp>

#include <satemu/hw/sh2/sh2.hpp>

#include <fstream>

namespace satemu::sh2 {

SH2Bus::SH2Bus(SH2 &masterSH2, SH2 &slaveSH2, scu::SCU &scu, smpc::SMPC &smpc)
    : m_masterSH2(masterSH2)
    , m_slaveSH2(slaveSH2)
    , m_SCU(scu)
    , m_SMPC(smpc) {

    std::filesystem::path bupRAMPath = "internal_backup_ram.bin";
    std::error_code err{};
    if (!std::filesystem::is_regular_file(bupRAMPath)) {
        std::ofstream out{bupRAMPath, std::ios::binary};
        out.seekp(kInternalBackupRAMSize - 1);
        out.put(0);
    }
    if (std::filesystem::file_size(bupRAMPath) < kInternalBackupRAMSize) {
        std::ofstream out{bupRAMPath, std::ios::binary | std::ios::ate};
        out.seekp(kInternalBackupRAMSize - 1);
        out.put(0);
    }
    internalBackupRAM = mio::make_mmap_sink(bupRAMPath.string(), err);

    IPL.fill(0);
    Reset(true);
}

void SH2Bus::Reset(bool hard) {
    WRAMLow.fill(0);
    WRAMHigh.fill(0);
}

void SH2Bus::LoadIPL(std::span<uint8, kIPLSize> ipl) {
    std::copy(ipl.begin(), ipl.end(), IPL.begin());
}

void SH2Bus::DumpWRAMLow(std::ostream &out) {
    out.write((const char *)WRAMLow.data(), WRAMLow.size());
}

void SH2Bus::DumpWRAMHigh(std::ostream &out) {
    out.write((const char *)WRAMHigh.data(), WRAMHigh.size());
}

void SH2Bus::AcknowledgeExternalInterrupt() {
    m_SCU.AcknowledgeExternalInterrupt();
}

void SH2Bus::WriteMINIT(uint16 value) {
    m_slaveSH2.TriggerFRTInputCapture();
}

void SH2Bus::WriteSINIT(uint16 value) {
    m_masterSH2.TriggerFRTInputCapture();
}

} // namespace satemu::sh2
