#pragma once

#include "cart_base.hpp"

#include <satemu/sys/backup_ram.hpp>

#include <filesystem>

namespace satemu::cart {

class BackupMemoryCartridge final : public BaseCartridge {
public:
    enum class Size { _4Mbit, _8Mbit, _16Mbit, _32Mbit };

    // TODO: specify copy-on-write mode
    BackupMemoryCartridge(Size size, const std::filesystem::path &path, std::error_code &error);

    // TODO: constructor for in-memory backup memory
    // - needs support from bup::BackupMemory

    bool IsInitialized() const final {
        return m_backupRAM.Size() > 0;
    }

    uint8 ReadByte(uint32 address) const final {
        return m_backupRAM.ReadByte(address);
    }

    uint16 ReadWord(uint32 address) const final {
        return m_backupRAM.ReadWord(address);
    }

    void WriteByte(uint32 address, uint8 value) final {
        m_backupRAM.WriteByte(address, value);
    }

    void WriteWord(uint32 address, uint16 value) final {
        m_backupRAM.WriteWord(address, value);
    }

private:
    bup::BackupMemory m_backupRAM;
};

} // namespace satemu::cart
