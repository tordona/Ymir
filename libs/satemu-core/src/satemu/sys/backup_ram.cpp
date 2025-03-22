#include <satemu/sys/backup_ram.hpp>

#include <string_view>

namespace satemu::bup {

static constexpr std::string_view kHeader = "BackUpRam Format";

void BackupMemory::LoadFrom(const std::filesystem::path &path, size_t size, std::error_code &error) {
    assert(size != 0 && (size & (size - 1)) == 0); // size must be a power of two

    if (!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path) < size) {
        std::ofstream out{path, std::ios::binary};

        // Write header at the beginning
        out.write(kHeader.data(), kHeader.size());

        // Clear the rest of the file
        out.seekp(0, std::ios::end);
        for (size_t i = out.tellp(); i < size; i++) {
            out.put(0);
        }
    }
    std::error_code err{};
    m_backupRAM = mio::make_mmap_sink(path.string(), err);
    m_addressMask = size - 1;
}

uint32 BackupMemory::Size() const {
    return m_backupRAM.size();
}

void BackupMemory::MapMemory(sys::Bus &bus, uint32 start, uint32 end) {
    auto read8 = [](uint32 address, void *ctx) -> uint8 { return static_cast<BackupMemory *>(ctx)->ReadByte(address); };
    auto read16 = [](uint32 address, void *ctx) -> uint16 {
        return static_cast<BackupMemory *>(ctx)->ReadWord(address);
    };
    auto read32 = [](uint32 address, void *ctx) -> uint32 {
        return static_cast<BackupMemory *>(ctx)->ReadLong(address);
    };

    auto write8 = [](uint32 address, uint8 value, void *ctx) {
        static_cast<BackupMemory *>(ctx)->WriteByte(address, value);
    };
    auto write16 = [](uint32 address, uint16 value, void *ctx) {
        static_cast<BackupMemory *>(ctx)->WriteWord(address, value);
    };
    auto write32 = [](uint32 address, uint32 value, void *ctx) {
        static_cast<BackupMemory *>(ctx)->WriteLong(address, value);
    };

    bus.MapMemory(start, end,
                  {
                      .ctx = this,
                      .read8 = read8,
                      .read16 = read16,
                      .read32 = read32,
                      .write8 = write8,
                      .write16 = write16,
                      .write32 = write32,
                      .peek8 = read8,
                      .peek16 = read16,
                      .peek32 = read32,
                      .poke8 = write8,
                      .poke16 = write16,
                      .poke32 = write32,
                  });
}

uint8 BackupMemory::ReadByte(uint32 address) const {
    if ((address & 1) && m_addressMask != 0) {
        return m_backupRAM[(address >> 1) & m_addressMask];
    } else {
        return 0xFFu;
    }
}

uint16 BackupMemory::ReadWord(uint32 address) const {
    if (m_addressMask != 0) {
        return 0xFF00u | m_backupRAM[(address >> 1) & m_addressMask];
    } else {
        return 0xFFFFu;
    }
}

uint32 BackupMemory::ReadLong(uint32 address) const {
    uint32 value = ReadWord(address + 0) << 16u;
    value |= ReadWord(address + 2) << 0u;
    return value;
}

void BackupMemory::WriteByte(uint32 address, uint8 value) {
    if ((address & 1) && m_addressMask != 0) {
        m_backupRAM[(address >> 1) & m_addressMask] = value;
    }
}

void BackupMemory::WriteWord(uint32 address, uint16 value) {
    if (m_addressMask != 0) {
        m_backupRAM[(address >> 1) & m_addressMask] = value;
    }
}

void BackupMemory::WriteLong(uint32 address, uint32 value) {
    WriteWord(address + 0, value >> 16u);
    WriteWord(address + 2, value >> 0u);
}

} // namespace satemu::bup
