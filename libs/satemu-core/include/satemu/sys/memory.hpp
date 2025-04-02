#pragma once

#include <satemu/core/types.hpp>

#include <satemu/sys/backup_ram.hpp>
#include <satemu/sys/bus.hpp>

#include <satemu/util/size_ops.hpp>

#include <array>
#include <iosfwd>
#include <span>

namespace satemu::sys {

inline constexpr std::size_t kIPLSize = 512_KiB;

inline constexpr std::size_t kWRAMLowSize = 1_MiB;
inline constexpr std::size_t kWRAMHighSize = 1_MiB;

inline constexpr bup::BackupMemorySize kInternalBackupRAMSize = bup::BackupMemorySize::_256Kbit;

struct SystemMemory {
    SystemMemory();

    void Reset(bool hard);

    void MapMemory(Bus &bus);

    void LoadIPL(std::span<uint8, kIPLSize> ipl);

    void DumpWRAMLow(std::ostream &out) const;
    void DumpWRAMHigh(std::ostream &out) const;

    alignas(16) std::array<uint8, kIPLSize> IPL; // aka BIOS ROM

    alignas(16) std::array<uint8, kWRAMLowSize> WRAMLow;
    alignas(16) std::array<uint8, kWRAMHighSize> WRAMHigh;

    bup::BackupMemory internalBackupRAM;
};

} // namespace satemu::sys
