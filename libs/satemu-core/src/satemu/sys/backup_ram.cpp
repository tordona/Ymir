#include <satemu/sys/backup_ram.hpp>

#include <satemu/util/size_ops.hpp>

#include <string_view>

namespace satemu::bup {

static constexpr std::string_view kHeader = "BackUpRam Format";

static constexpr size_t kSizes[] = {
    32_KiB,  // Internal Backup RAM
    512_KiB, // 4 Mbit External Backup RAM
    1_MiB,   // 8 Mbit External Backup RAM
    2_MiB,   // 16 Mbit External Backup RAM
    4_MiB,   // 32 Mbit External Backup RAM
};

static constexpr size_t kBlockSizes[] = {
    64,   // Internal Backup RAM
    512,  // 4 Mbit External Backup RAM
    512,  // 8 Mbit External Backup RAM
    512,  // 16 Mbit External Backup RAM
    1024, // 32 Mbit External Backup RAM
};

void BackupMemory::MapMemory(sys::Bus &bus, uint32 start, uint32 end) {
    bus.MapBoth(
        start, end, this,
        [](uint32 address, void *ctx) -> uint8 { return static_cast<BackupMemory *>(ctx)->ReadByte(address); },
        [](uint32 address, void *ctx) -> uint16 { return static_cast<BackupMemory *>(ctx)->ReadWord(address); },
        [](uint32 address, void *ctx) -> uint32 { return static_cast<BackupMemory *>(ctx)->ReadLong(address); },
        [](uint32 address, uint8 value, void *ctx) { static_cast<BackupMemory *>(ctx)->WriteByte(address, value); },
        [](uint32 address, uint16 value, void *ctx) { static_cast<BackupMemory *>(ctx)->WriteWord(address, value); },
        [](uint32 address, uint32 value, void *ctx) { static_cast<BackupMemory *>(ctx)->WriteLong(address, value); });
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

void BackupMemory::LoadFrom(const std::filesystem::path &path, BackupMemorySize size, std::error_code &error) {
    error.clear();

    bool format = false;

    // Convert enum into size
    const size_t sz = kSizes[static_cast<size_t>(size)];

    // Create file if it does not exist
    if (!std::filesystem::is_regular_file(path)) {
        format = true;
        std::ofstream out{path, std::ios::binary};
        if (!out) {
            error.assign(errno, std::generic_category());
            return;
        }
    }

    // Resize file if necessary
    if (std::filesystem::file_size(path) != sz) {
        format = true;
        std::filesystem::resize_file(path, sz);
    }

    // Attempt to memory-map the file
    m_backupRAM = mio::make_mmap_sink(path.string(), error);
    if (error) {
        return;
    }

    // Check if it has a valid header
    if (!IsValid()) {
        format = true;
    }

    // Format if requested
    if (format) {
        Format();
    }

    m_addressMask = m_backupRAM.size() - 1;
    m_blockSize = kBlockSizes[static_cast<size_t>(size)];
}

bool BackupMemory::IsValid() const {
    if (Size() == 0) {
        return false;
    }

    // Check that the first block contains the header
    for (uint32 i = 0; i < m_blockSize; i++) {
        if (m_backupRAM[i] != kHeader[i & 0xF]) {
            return false;
        }
    }

    // Check that the second block is entirely empty
    for (uint32 i = m_blockSize; i < m_blockSize * 2; i++) {
        if (m_backupRAM[i] != 0x00) {
            return false;
        }
    }

    // Seems to be valid backup memory
    return true;
}

uint32 BackupMemory::Size() const {
    return m_backupRAM.size();
}

uint32 BackupMemory::GetBlockSize() const {
    return m_blockSize;
}

uint32 BackupMemory::GetTotalBlocks() const {
    const uint32 size = Size();
    const uint32 blockSize = GetBlockSize();
    if (blockSize != 0) {
        return size / blockSize;
    } else {
        return 0;
    }
}

uint32 BackupMemory::GetUsedBlocks() const {
    // TODO: Determine number of blocks used by existing backup files
    uint32 usedBlocks = 2; // Backup RAM header + null block

    return usedBlocks;
}

void BackupMemory::Format() {
    if (Size() == 0) {
        return;
    }

    // Fill first block with the header
    for (uint32 i = 0; i < m_blockSize; i += 0x10) {
        std::copy_n(kHeader.begin(), 0x10, m_backupRAM.begin() + i);
    }

    // Fill the rest with zeros
    std::fill(m_backupRAM.begin() + m_blockSize, m_backupRAM.end(), 0);
}

std::vector<BackupFileInfo> BackupMemory::GetBackupFiles() const {
    // TODO: list files
    return {};
}

std::optional<BackupFile> BackupMemory::ExportBackup(std::string_view filename) const {
    // TODO: find file by name; if found, build object and return, otherwise return std::nullopt
    return std::nullopt;
}

BackupFileImportResult BackupMemory::ImportBackup(const BackupFile &data, bool overwrite) const {
    // TODO: implement
    // - find existing file
    //   - if found
    //     - if overwrite, return BackupFileImportResult::FileExists
    //     - otherwise, get blocks list used by the file
    //   - otherwise, leave blocks list empty
    // - determine total size required for the file
    //   - take into account the blocks list
    //   - if not enough space, return BackupFileImportResult::NoSpace
    // - write file
    //   - reuse blocks list from existing file
    //   - allocate new blocks as needed
    // - return BackupFileImportResult::Overwritten if existing file was overwritten
    // - return BackupFileImportResult::Imported otherwise
}

bool BackupMemory::DeleteBackup(std::string_view filename) {
    // TODO: find and delete file
    return false;
}

} // namespace satemu::bup
