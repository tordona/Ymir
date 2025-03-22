#pragma once

#include <satemu/core/types.hpp>

#include <satemu/sys/bus.hpp>

#include <mio/mmap.hpp>

#include <filesystem>

namespace satemu::bup {

class BackupMemory {
public:
    void LoadFrom(const std::filesystem::path &path, size_t size, std::error_code &error);

    uint32 Size() const;

    void MapMemory(sys::Bus &bus, uint32 start, uint32 end);

    uint8 ReadByte(uint32 address) const;
    uint16 ReadWord(uint32 address) const;
    uint32 ReadLong(uint32 address) const;

    void WriteByte(uint32 address, uint8 value);
    void WriteWord(uint32 address, uint16 value);
    void WriteLong(uint32 address, uint32 value);

private:
    // TODO: support three types
    // - in-memory (std::vector)
    // - memory-mapped file (mio::mmap_sink)
    // - memory-mapped copy-on-write file (mio::mmap_cow_sink)
    mio::mmap_sink m_backupRAM;

    uint32 m_addressMask = 0;
};

} // namespace satemu::bup
