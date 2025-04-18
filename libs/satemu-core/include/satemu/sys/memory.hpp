#pragma once

#include "memory_defs.hpp"

#include <satemu/sys/backup_ram.hpp>
#include <satemu/sys/bus.hpp>

#include <satemu/state/state_system.hpp>

#include <satemu/core/hash.hpp>
#include <satemu/core/types.hpp>

#include <array>
#include <iosfwd>
#include <span>

namespace satemu::sys {

struct SystemMemory {
    SystemMemory();

    void Reset(bool hard);

    void MapMemory(Bus &bus);

    void LoadIPL(std::span<uint8, kIPLSize> ipl);
    XXH128Hash GetIPLHash() const;

    void LoadInternalBackupMemoryImage(std::filesystem::path path, std::error_code &error);

    void DumpWRAMLow(std::ostream &out) const;
    void DumpWRAMHigh(std::ostream &out) const;

    bup::IBackupMemory &GetInternalBackupRAM() {
        return m_internalBackupRAM;
    }

    void SetInternalBackupRAM(bup::BackupMemory &&bupMem) {
        std::swap(m_internalBackupRAM, bupMem);
    }

    // -------------------------------------------------------------------------
    // Save states

    void SaveState(state::SystemState &state) const;
    bool ValidateState(const state::SystemState &state) const;
    void LoadState(const state::SystemState &state);

    // -------------------------------------------------------------------------
    // Memory

    alignas(16) std::array<uint8, kIPLSize> IPL; // aka BIOS ROM

    alignas(16) std::array<uint8, kWRAMLowSize> WRAMLow;
    alignas(16) std::array<uint8, kWRAMHighSize> WRAMHigh;

private:
    bup::BackupMemory m_internalBackupRAM;

    XXH128Hash m_iplHash{};
};

} // namespace satemu::sys
