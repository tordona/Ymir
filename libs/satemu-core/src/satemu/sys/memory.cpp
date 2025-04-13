#include <satemu/sys/memory.hpp>

#include <xxh3.h>

namespace satemu::sys {

SystemMemory::SystemMemory() {
    // TODO: configurable path and mode
    std::error_code error{};
    m_internalBackupRAM.CreateFrom("bup-int.bin", kInternalBackupRAMSize, error);
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
    XXH128_hash_t hash = XXH128(IPL.data(), IPL.size(), 0x94B487AF51733FBEull);
    XXH128_canonical_t canonicalHash{};
    XXH128_canonicalFromHash(&canonicalHash, hash);
    std::copy_n(canonicalHash.digest, m_iplHash.size(), m_iplHash.begin());
}

Hash128 SystemMemory::GetIPLHash() const {
    return m_iplHash;
}

void SystemMemory::DumpWRAMLow(std::ostream &out) const {
    out.write((const char *)WRAMLow.data(), WRAMLow.size());
}

void SystemMemory::DumpWRAMHigh(std::ostream &out) const {
    out.write((const char *)WRAMHigh.data(), WRAMHigh.size());
}

void SystemMemory::SaveState(state::SystemState &state) const {
    state.iplRomHash = m_iplHash;
    state.WRAMLow = WRAMLow;
    state.WRAMHigh = WRAMHigh;
}

bool SystemMemory::ValidateState(const state::SystemState &state) const {
    if (state.iplRomHash != m_iplHash) {
        return false;
    }
    return true;
}

void SystemMemory::LoadState(const state::SystemState &state) {
    WRAMLow = state.WRAMLow;
    WRAMHigh = state.WRAMHigh;
}

} // namespace satemu::sys
