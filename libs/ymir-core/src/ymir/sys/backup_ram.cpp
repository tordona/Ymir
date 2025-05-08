#include <ymir/sys/backup_ram.hpp>

#include <ymir/util/data_ops.hpp>
#include <ymir/util/size_ops.hpp>

#include <string_view>

namespace ymir::bup {

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

BackupMemoryImageLoadResult BackupMemory::LoadFrom(const std::filesystem::path &path, std::error_code &error) {
    error.clear();

    // Attempt to memory-map the file
    m_backupRAM = mio::make_mmap_sink(path.native(), error);
    if (error) {
        return BackupMemoryImageLoadResult::FilesystemError;
    }

    // Determine if image size matches any valid backup memory size
    bool valid = false;
    BackupMemorySize size{};
    for (uint32 i = 0; i < std::size(kSizes); i++) {
        if (kSizes[i] == m_backupRAM.size()) {
            valid = true;
            size = static_cast<BackupMemorySize>(i);
            break;
        }
    }
    if (!valid) {
        // Fail without specifying error code
        m_backupRAM.unmap();
        return BackupMemoryImageLoadResult::InvalidSize;
    }

    // Update parameters
    m_headerValid = CheckHeader();
    m_addressMask = m_backupRAM.size() - 1;
    m_blockSize = kBlockSizes[static_cast<size_t>(size)];
    m_blockBitmap.resize(GetTotalBlocks() / 64);

    RebuildFileList(true);

    m_path = path;

    return BackupMemoryImageLoadResult::Success;
}

void BackupMemory::CreateFrom(const std::filesystem::path &path, BackupMemorySize size, std::error_code &error) {
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
        std::filesystem::resize_file(path, sz, error);
        if (error) {
            return;
        }
    }

    // Attempt to memory-map the file
    m_backupRAM = mio::make_mmap_sink(path.native(), error);
    if (error) {
        return;
    }

    // Check if it has a valid header
    m_headerValid = CheckHeader();
    if (!m_headerValid) {
        format = true;
    }

    // Update parameters
    m_addressMask = m_backupRAM.size() - 1;
    m_blockSize = kBlockSizes[static_cast<size_t>(size)];
    m_blockBitmap.resize(GetTotalBlocks() / 64);

    // Format if requested
    if (format) {
        Format();
    }

    RebuildFileList(true);

    m_path = path;
}

bool BackupMemory::CopyFrom(const IBackupMemory &backupRAM) {
    // Preemptively fail to import from larger backup memories, even if their contents would fit here
    if (backupRAM.Size() > Size()) {
        return false;
    }

    // Clear this backup memory
    Format();

    // Copy everything from the other backup memory
    for (auto &file : backupRAM.ExportAll()) {
        Import(file, true);
    }

    return true;
}

std::filesystem::path BackupMemory::GetPath() const {
    return m_path;
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
        m_dirty = true;
    }
}

void BackupMemory::WriteWord(uint32 address, uint16 value) {
    if (m_addressMask != 0) {
        m_backupRAM[(address >> 1) & m_addressMask] = value;
        m_dirty = true;
    }
}

void BackupMemory::WriteLong(uint32 address, uint32 value) {
    if (m_addressMask != 0) {
        m_backupRAM[((address + 0) >> 1) & m_addressMask] = value >> 16u;
        m_backupRAM[((address + 2) >> 1) & m_addressMask] = value >> 0u;
        m_dirty = true;
    }
}

std::vector<uint8> BackupMemory::ReadAll() const {
    return {m_backupRAM.begin(), m_backupRAM.end()};
}

bool BackupMemory::IsHeaderValid() const {
    return m_headerValid;
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

uint32 BackupMemory::GetUsedBlocks() {
    uint32 usedBlocks = 2; // Backup RAM header + null block

    RebuildFileList();

    // Determine number of blocks used by existing backup files
    for (const BackupFileParams &file : m_fileParams) {
        usedBlocks += file.blocks.size();
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

    // Reset bitmap
    std::fill(m_blockBitmap.begin(), m_blockBitmap.end(), 0);
    m_blockBitmap[0] |= (1ull << 0ull) | (1ull << 1ull);

    // Clear cached file parameters
    m_fileParams.clear();

    m_dirty = false;
}

std::vector<BackupFileInfo> BackupMemory::List() const {
    RebuildFileList();

    std::vector<BackupFileInfo> files{};
    for (auto &params : m_fileParams) {
        files.push_back(params.info);
    }
    return files;
}

std::optional<BackupFileInfo> BackupMemory::GetInfo(std::string_view filename) const {
    RebuildFileList();

    for (auto &params : m_fileParams) {
        if (params.info.header.filename == filename) {
            return params.info;
        }
    }
    return std::nullopt;
}

std::optional<BackupFile> BackupMemory::Export(std::string_view filename) const {
    RebuildFileList();

    const BackupFileParams *params = FindFile(filename);
    if (params == nullptr) {
        return std::nullopt;
    }

    return BuildFile(*params);
}

std::vector<BackupFile> BackupMemory::ExportAll() const {
    RebuildFileList();

    std::vector<BackupFile> files{};
    for (auto &params : m_fileParams) {
        files.push_back(BuildFile(params));
    }
    return files;
}

BackupFileImportResult BackupMemory::Import(const BackupFile &file, bool overwrite) {
    RebuildFileList();

    std::vector<uint16> blockList{};
    uint32 blockListIndex = 0;

    // Get blocks list from existing file if found
    BackupFileParams *params = FindFile(file.header.filename);
    if (params != nullptr) {
        if (!overwrite) {
            return BackupFileImportResult::FileExists;
        }
        blockList = params->blocks;
    }

    // Calculate available data bytes per block
    const uint16 totalBlocks = GetTotalBlocks();
    uint16 freeBlockSearchIndex = 0;

    std::vector<uint64> allocBitmap = m_blockBitmap;

    auto allocateBlock = [&]() -> bool {
        // Find a free block if the existing file doesn't have one already in use
        if (blockListIndex == blockList.size()) {
            while (freeBlockSearchIndex < totalBlocks / 64) {
                uint64 &bitmap = allocBitmap[freeBlockSearchIndex];
                const uint16 pos = std::countr_one(bitmap);
                if (pos < 64) {
                    bitmap |= 1ull << pos;
                    blockList.push_back(pos + freeBlockSearchIndex * 64);
                    blockListIndex++;
                    return true;
                }
                freeBlockSearchIndex++;
            }
        } else {
            blockListIndex++;
            return true;
        }

        // No blocks are avaiable
        return false;
    };

    // Every block reserves 4 bytes for the "in use" flag and padding/unused bytes.
    const uint32 blockDataSize = m_blockSize - 4;

    // Try to allocate all blocks needed to store the data, including the block list and the header.
    uint32 remaining = file.data.size() + 30 /*header*/;
    while (remaining > 0) {
        if (!allocateBlock()) {
            return BackupFileImportResult::NoSpace;
        }
        remaining += 2; // One entry added to the block list
        if (remaining >= blockDataSize) {
            remaining -= blockDataSize;
        } else {
            remaining = 0;
        }
    }

    // Update allocation bitmap
    for (uint32 i = 0; i < blockList.size(); i++) {
        const uint16 blockIndex = blockList[i];
        const uint64 bit = 1ull << (blockIndex & 63ull);
        if (i < blockListIndex) {
            m_blockBitmap[blockIndex / 64] |= bit;
        } else {
            m_blockBitmap[blockIndex / 64] &= ~bit;
        }
    }

    // Trim block list to the number of blocks actually allocated
    blockList.resize(blockListIndex);
    blockListIndex = 0;

    // Now write the data
    bool headerWritten = false;
    uint32 writtenBlockListEntries = 0;
    uint32 fileDataOffset = 0;
    uint32 blockListWriteIndex = 1;
    remaining = file.data.size() + blockList.size() * 2 + 30; // include header
    while (remaining > 0) {
        const uint16 blockIndex = blockList[blockListIndex];
        uint32 blockOffset = blockIndex * m_blockSize;
        uint32 remainingInBlock = blockDataSize;

        // Write "in use" marker and padding on first block
        if (blockListIndex == 0) {
            m_backupRAM[blockOffset + 0] = 0x80;
            m_backupRAM[blockOffset + 1] = 0x00;
            m_backupRAM[blockOffset + 2] = 0x00;
            m_backupRAM[blockOffset + 3] = 0x00;
        }
        blockListIndex++;

        // Write header
        if (!headerWritten) {
            std::fill_n(&m_backupRAM[blockOffset + 0x04], 11, '\0');
            std::copy_n(file.header.filename.begin(), std::min<size_t>(file.header.filename.size(), 11),
                        &m_backupRAM[blockOffset + 0x04]);

            std::fill_n(&m_backupRAM[blockOffset + 0x10], 10, '\0');
            std::copy_n(file.header.comment.begin(), std::min<size_t>(file.header.comment.size(), 10),
                        &m_backupRAM[blockOffset + 0x10]);

            m_backupRAM[blockOffset + 0x0F] = static_cast<char>(file.header.language);
            util::WriteBE<uint32>((uint8 *)&m_backupRAM[blockOffset + 0x1A], file.header.date);
            util::WriteBE<uint32>((uint8 *)&m_backupRAM[blockOffset + 0x1E], file.data.size());

            blockOffset += 30;
            remainingInBlock -= 30;
            remaining -= 30;
            headerWritten = true;
        }

        // Skip the reserved bytes
        blockOffset += 4;

        // Write block list
        if (writtenBlockListEntries < blockList.size()) {
            const uint32 remainingBlockListEntries = blockList.size() - writtenBlockListEntries;
            const uint32 entriesToWrite = std::min<uint32>(remainingBlockListEntries, remainingInBlock / 2);

            // NOTE: blockListWriteIndex is offset by 1 since the first entry in the list is the starting block, which
            // is not written here. We still write blockList.size() entries so that we can write the last one as 0000.
            for (uint32 i = 0; i < entriesToWrite; i++) {
                if (blockListWriteIndex < blockList.size()) {
                    util::WriteBE<uint16>((uint8 *)&m_backupRAM[blockOffset + i * 2], blockList[blockListWriteIndex++]);
                } else {
                    util::WriteBE<uint16>((uint8 *)&m_backupRAM[blockOffset + i * 2], 0x0000);
                }
            }

            writtenBlockListEntries += entriesToWrite;
            remainingInBlock -= entriesToWrite * 2;
            remaining -= entriesToWrite * 2;
            blockOffset += entriesToWrite * 2;
        }

        // Write data
        const uint32 dataToWrite = std::min(remainingInBlock, remaining);
        std::copy_n(&file.data[fileDataOffset], dataToWrite, &m_backupRAM[blockOffset]);
        fileDataOffset += dataToWrite;
        blockOffset += dataToWrite;

        if (remaining >= dataToWrite) {
            remaining -= dataToWrite;
        }

        // Pad the rest of the final block with zeros
        if (remaining == 0) {
            std::fill(&m_backupRAM[blockOffset], &m_backupRAM[(blockIndex + 1) * m_blockSize], 0);
        }
    }

    // Upsert file info
    bool overwritten = params != nullptr;
    if (params == nullptr) {
        params = &m_fileParams.emplace_back();
    }
    params->info.header = file.header;
    params->blocks = blockList;
    params->info.numBlocks = params->blocks.size();
    params->info.size = file.data.size();

    return overwritten ? BackupFileImportResult::Overwritten : BackupFileImportResult::Imported;
}

bool BackupMemory::Delete(std::string_view filename) {
    RebuildFileList();

    // If file exists, clear the flag on the first block and update state
    for (auto it = m_fileParams.begin(); it != m_fileParams.end(); it++) {
        auto &params = *it;
        if (params.info.header.filename == filename) {
            m_backupRAM[params.blocks[0] * m_blockSize] &= ~0x80;

            // Free all blocks in the bitmap
            for (uint16 blockIndex : params.blocks) {
                m_blockBitmap[blockIndex / 64] &= ~(1ull << (blockIndex & 63ull));
            }

            // Remove from file list
            m_fileParams.erase(it);
            return true;
        }
    }

    return false;
}

void BackupMemory::RebuildFileList(bool force) {
    if (!force && !m_dirty) {
        return;
    }
    if (Size() == 0) {
        return;
    }

    m_dirty = false;

    m_headerValid = CheckHeader();

    const uint32 totalBlocks = GetTotalBlocks();

    // Mark blocks 0 and 1 as used
    std::fill(m_blockBitmap.begin(), m_blockBitmap.end(), 0);
    m_blockBitmap[0] |= (1ull << 0ull) | (1ull << 1ull);

    m_fileParams.clear();
    for (uint32 i = 2; i < totalBlocks; i++) {
        const uint32 offset = i * m_blockSize;

        if ((m_backupRAM[offset] & 0x80) == 0x00) {
            continue;
        }

        auto &params = m_fileParams.emplace_back();
        params.blocks = ReadBlockList(i);
        ReadHeader(i, params.info.header);
        params.info.size = util::ReadBE<uint32>((const uint8 *)&m_backupRAM[offset + 0x1E]);
        params.info.numBlocks = params.blocks.size();

        // Mark blocks as used
        for (uint16 block : params.blocks) {
            m_blockBitmap[block / 64] |= (1ull << (block & 63ull));
        }
    }
}

void BackupMemory::RebuildFileList(bool force) const {
    const_cast<BackupMemory *>(this)->RebuildFileList(force);
}

bool BackupMemory::CheckHeader() const {
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

    // Backup header is valid
    return true;
}

BackupMemory::BackupFileParams *BackupMemory::FindFile(std::string_view filename) {
    for (auto &params : m_fileParams) {
        if (params.info.header.filename == filename) {
            return &params;
        }
    }

    return nullptr;
}

const BackupMemory::BackupFileParams *BackupMemory::FindFile(std::string_view filename) const {
    return const_cast<BackupMemory *>(this)->FindFile(filename);
}

void BackupMemory::ReadHeader(uint16 blockIndex, BackupFileHeader &header) const {
    const uint32 offset = blockIndex * m_blockSize;
    header.filename.assign(&m_backupRAM[offset + 0x04], (size_t)11);
    header.comment.assign(&m_backupRAM[offset + 0x10], (size_t)10);
    header.language = static_cast<Language>(m_backupRAM[offset + 0x0F]);
    header.date = util::ReadBE<uint32>((const uint8 *)&m_backupRAM[offset + 0x1A]);
}

std::vector<uint16> BackupMemory::ReadBlockList(uint16 blockIndex) const {
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

BackupFile BackupMemory::BuildFile(const BackupFileParams &params) const {
    const BackupFileInfo &info = params.info;

    BackupFile file{};
    file.header = info.header;

    const auto &blockList = params.blocks;
    const uint32 blockListSize = blockList.size() * 2;
    uint32 blockListIndex = 0;

    uint32 blockListRemaining = blockListSize;
    uint32 remaining = info.size;

    while (remaining > 0) {
        if (blockListIndex >= blockList.size()) {
            // TODO: warn about file truncation
            break;
        }
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

} // namespace ymir::bup
