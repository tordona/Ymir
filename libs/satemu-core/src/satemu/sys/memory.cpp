#include <satemu/sys/memory.hpp>

namespace satemu::sys {

SystemMemory::SystemMemory() {
    // TODO: configurable path and mode
    std::error_code error{};
    m_internalBackupRAM.LoadFrom("bup-int.bin", kInternalBackupRAMSize, error);
    // TODO: handle error

    IPL.fill(0);
    Reset(true);
}

void SystemMemory::Reset(bool hard) {
    if (hard) {
        WRAMLow.fill(0);
        WRAMHigh.fill(0);
    }
}

void SystemMemory::MapMemory(Bus &bus) {
    bus.MapArray(0x000'0000, 0x00F'FFFF, IPL, false);
    m_internalBackupRAM.MapMemory(bus, 0x018'0000, 0x01F'FFFF);
    bus.MapArray(0x020'0000, 0x02F'FFFF, WRAMLow, true);
    bus.MapArray(0x600'0000, 0x7FF'FFFF, WRAMHigh, true);
}

void SystemMemory::LoadIPL(std::span<uint8, kIPLSize> ipl) {
    std::copy(ipl.begin(), ipl.end(), IPL.begin());
}

void SystemMemory::DumpWRAMLow(std::ostream &out) const {
    out.write((const char *)WRAMLow.data(), WRAMLow.size());
}

void SystemMemory::DumpWRAMHigh(std::ostream &out) const {
    out.write((const char *)WRAMHigh.data(), WRAMHigh.size());
}

} // namespace satemu::sys
