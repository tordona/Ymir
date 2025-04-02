#include <satemu/sys/backup_ram.hpp>

#include <satemu/util/data_ops.hpp>
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
    if (!IsHeaderValid()) {
        format = true;
    }

    // Format if requested
    if (format) {
        Format();
    }

    m_addressMask = m_backupRAM.size() - 1;
    m_blockSize = kBlockSizes[static_cast<size_t>(size)];
}

bool BackupMemory::IsHeaderValid() const {
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
    uint32 usedBlocks = 2; // Backup RAM header + null block

    const uint32 totalBlocks = GetTotalBlocks();

    // Determine number of blocks used by existing backup files
    for (uint32 i = 2; i < totalBlocks; i++) {
        const uint32 offset = i * m_blockSize;
        if ((m_backupRAM[offset] & 0x80) == 0x00) {
            continue;
        }
        usedBlocks += ReadBlockList(i).size();
    }

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

std::vector<BackupFileInfo> BackupMemory::List() const {
    if (Size() == 0) {
        return {};
    }

    const uint32 totalBlocks = GetTotalBlocks();

    std::vector<BackupFileInfo> bupInfos{};
    for (uint32 i = 2; i < totalBlocks; i++) {
        const uint32 offset = i * m_blockSize;

        if ((m_backupRAM[offset] & 0x80) == 0x00) {
            continue;
        }

        auto &bupInfo = bupInfos.emplace_back();
        ReadHeader(i, bupInfo.header);
        bupInfo.blocks = ReadBlockList(i).size();
    }

    return bupInfos;
}

std::optional<BackupFile> BackupMemory::Export(std::string_view filename) const {
    const uint32 fileIndex = FindFile(filename);
    if (fileIndex == 0) {
        return std::nullopt;
    }

    BackupFile file{};
    ReadHeader(fileIndex, file.header);

    const auto blockList = ReadBlockList(fileIndex);
    const uint32 blockListSize = blockList.size() * 2;
    uint32 blockListIndex = 0;

    uint32 blockListRemaining = blockListSize;
    uint32 remaining = file.header.size;

    while (remaining > 0) {
        const uint32 blockOffset = blockList[blockListIndex] * m_blockSize;
        uint32 innerOffset = blockListIndex == 0 ? 0x22 : 0x04;
        uint32 availBytes = m_blockSize - innerOffset;
        blockListIndex++;

        // Skip block list
        if (blockListRemaining >= availBytes) {
            blockListRemaining -= availBytes;
            continue;
        } else if (blockListRemaining > 0) {
            availBytes -= blockListRemaining;
            innerOffset += blockListRemaining;
            blockListRemaining = 0;
        }

        // Read data
        availBytes = std::min(availBytes, remaining);
        remaining -= availBytes;
        file.data.insert(file.data.end(), &m_backupRAM[blockOffset + innerOffset],
                         &m_backupRAM[blockOffset + innerOffset + availBytes]);
    }

    return file;
}

BackupFileImportResult BackupMemory::Import(const BackupFile &data, bool overwrite) const {
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
    return BackupFileImportResult::NoSpace;
}

bool BackupMemory::Delete(std::string_view filename) {
    const uint32 fileIndex = FindFile(filename);

    // If file exists, clear the flag on the first block
    if (fileIndex != 0) {
        m_backupRAM[fileIndex * m_blockSize] &= ~0x80;
        return true;
    }

    return false;
}

uint32 BackupMemory::FindFile(std::string_view filename) const {
    if (Size() == 0) {
        return 0;
    }

    const uint32 totalBlocks = GetTotalBlocks();

    for (uint32 i = 2; i < totalBlocks; i++) {
        const uint32 offset = i * m_blockSize;
        if (m_backupRAM[offset] & 0x80) {
            const auto currFilename = std::string_view((const char *)&m_backupRAM[offset + 0x04], (size_t)11);
            if (filename == currFilename) {
                return i;
            }
        }
    }

    return 0;
}

void BackupMemory::ReadHeader(uint32 blockIndex, BackupFileHeader &header) const {
    const uint32 offset = blockIndex * m_blockSize;
    header.filename.assign(&m_backupRAM[offset + 0x04], (size_t)11);
    header.comment.assign(&m_backupRAM[offset + 0x10], (size_t)10);
    header.language = static_cast<Language>(m_backupRAM[offset + 0x0F]);
    header.date = util::ReadBE<uint32>((const uint8 *)&m_backupRAM[offset + 0x1A]);
    header.size = util::ReadBE<uint32>((const uint8 *)&m_backupRAM[offset + 0x1E]);
}

std::vector<uint16> BackupMemory::ReadBlockList(uint32 blockIndex) const {
    uint32 offset = blockIndex * m_blockSize + 0x22;

    const uint32 totalBlocks = GetTotalBlocks();

    std::vector<uint16> blockList{};
    blockList.push_back(blockIndex);
    uint32 listIndex = 1;

    do {
        uint16 nextBlock = util::ReadBE<uint16>((const uint8 *)&m_backupRAM[offset]);
        if (nextBlock == 0) {
            // End of list
            break;
        }
        if (nextBlock >= totalBlocks) {
            // Invalid block index
            continue;
        }

        blockList.push_back(nextBlock);

        // Advance pointer
        offset += 2;
        if ((offset & (m_blockSize - 1)) == 0) {
            // Skip to the next block at offset 0x04 if we reach the start of the next block
            offset = blockList[listIndex++] * m_blockSize + 4;
        }
    } while (offset < m_backupRAM.size());

    return blockList;
}

} // namespace satemu::bup
