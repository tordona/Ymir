#include <satemu/hw/sh2/sh2_bus.hpp>

#include <satemu/hw/sh2/sh2.hpp>

#include <fstream>

namespace satemu::sh2 {

SH2Bus::SH2Bus(SH2 &masterSH2, SH2 &slaveSH2, scu::SCU &scu, smpc::SMPC &smpc)
    : m_masterSH2(masterSH2)
    , m_slaveSH2(slaveSH2)
    , m_SCU(scu)
    , m_SMPC(smpc) {

    // HACK: should be in its own class, shared with external backup RAM cartridges
    static constexpr std::string_view kHeader = "BackUpRam Format";
    std::filesystem::path bupRAMPath = "bup-int.bin";
    if (!std::filesystem::is_regular_file(bupRAMPath) ||
        std::filesystem::file_size(bupRAMPath) < kInternalBackupRAMSize) {
        std::ofstream out{bupRAMPath, std::ios::binary};

        // Write header at the beginning
        for (int i = 0; i < 4; i++) {
            for (char ch : kHeader) {
                out.put(0xFF);
                out.put(ch);
            }
        }

        // Clear the rest of the file
        out.seekp(0, std::ios::end);
        if (out.tellp() & 1) {
            out.put(0);
        }
        for (size_t i = out.tellp(); i < kInternalBackupRAMSize; i += 2) {
            out.put(0xFF);
            out.put(0);
        }
    }
    std::error_code err{};
    internalBackupRAM = mio::make_mmap_sink(bupRAMPath.string(), err);
    // TODO: handle error

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
