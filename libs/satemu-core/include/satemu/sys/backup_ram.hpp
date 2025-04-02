#pragma once

#include <satemu/core/types.hpp>

#include <satemu/sys/bus.hpp>

#include <mio/mmap.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace satemu::bup {

enum class BackupMemorySize {
    _256Kbit, // Internal Backup RAM
    _4Mbit,   // 4 Mbit External Backup RAM
    _8Mbit,   // 8 Mbit External Backup RAM
    _16Mbit,  // 16 Mbit External Backup RAM
    _32Mbit,  // 32 Mbit External Backup RAM
};

enum class Language : uint8 {
    Japanese = 0x00,
    English = 0x01,
    French = 0x02,
    German = 0x03,
    Spanish = 0x04,
    Italian = 0x05,
};

struct BackupFileHeader {
    std::string filename; // max 11 chars
    std::string comment;  // max 10 chars
    Language language;
    uint32 date; // minutes since 1/1/1980
    uint32 size; // in bytes (including block list)
};

struct BackupFileInfo {
    BackupFileHeader header;
    uint32 blocks;
};

struct BackupFile {
    BackupFileHeader header;
    std::vector<uint8> data;
};

enum class BackupFileImportResult { Imported, Overwritten, FileExists, NoSpace };

class BackupMemory {
public:
    void MapMemory(sys::Bus &bus, uint32 start, uint32 end);

    uint8 ReadByte(uint32 address) const;
    uint16 ReadWord(uint32 address) const;
    uint32 ReadLong(uint32 address) const;

    void WriteByte(uint32 address, uint8 value);
    void WriteWord(uint32 address, uint16 value);
    void WriteLong(uint32 address, uint32 value);

    // Creates or replaces a backup memory file at the specified path with the given size.
    // If the file does not exist, it is created with the given size.
    // If the file exists with a different size, it is resized or truncated to match.
    // If the file had to be created, resized or did not contain a valid backup memory, it is formatted.
    //
    // `path` is the path to the backup memory file to create.
    // `size` is the total backup memory size.
    // `error` will contain any error that occurs while loading or manipulating the file.
    void LoadFrom(const std::filesystem::path &path, BackupMemorySize size, std::error_code &error);

    // Checks if the backup memory header is valid.
    bool IsHeaderValid() const;

    // Retrieves the total size in bytes of the backup memory.
    uint32 Size() const;

    // Retrieves the block size.
    uint32 GetBlockSize() const;

    // Retrieves the total number of blocks.
    uint32 GetTotalBlocks() const;

    // Computes the number of blocks used by backup files.
    uint32 GetUsedBlocks() const;

    // Formats the backup memory.
    void Format();

    // Retrieves a list of backup files stored in this backup memory.
    std::vector<BackupFileInfo> List() const;

    // Attempts to export the backup file with the specified name.
    //
    // Returns a BackupFile with the file's contents if it exists.
    // Returns std::nullopt if no such file exists.
    std::optional<BackupFile> Export(std::string_view filename) const;

    // Attempts to import the specified backup file, optionally overwriting an existing file with the same name as the
    // one being imported.
    //
    // Returns BackupFileImportResult::Imported if the file was newly imported.
    // Returns BackupFileImportResult::Overwritten if the overwrite flag is set and an existing file was overwritten.
    // Returns BackupFileImportResult::FileExists if the overwrite flag is clear and the file already exists.
    // Returns BackupFileImportResult::NoSpace if there is not enough space to import the file. The contents of the
    // backup memory are not modified if this happens.
    BackupFileImportResult Import(const BackupFile &data, bool overwrite) const;

    // Attempts to delete a backup file with the specified name.
    //
    // Returns true if the file was deleted.
    // Returns false if there was no file with the specified name.
    bool Delete(std::string_view filename);

private:
    // TODO: support three types
    // - in-memory (std::vector)
    // - memory-mapped file (mio::mmap_sink)
    // - memory-mapped copy-on-write file (mio::mmap_cow_sink)
    mio::mmap_sink m_backupRAM;

    size_t m_addressMask = 0;
    uint32 m_blockSize = 0;

    // Finds the block index of the file with the given filename.
    // Returns 0 if the file cannot be found.
    uint32 FindFile(std::string_view filename) const;

    // Reads in the backup file header from the given block.
    void ReadHeader(uint32 blockIndex, BackupFileHeader &header) const;

    // Reads the block list from the given block.
    // The list contains `blockIndex` as the first entry.
    std::vector<uint16> ReadBlockList(uint32 blockIndex) const;
};

} // namespace satemu::bup
