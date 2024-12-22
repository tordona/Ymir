#pragma once

#include <satemu/core_types.hpp>
#include <satemu/util/bit_ops.hpp>

#include "disc.hpp"
#include "filter.hpp"
#include "iso9660.hpp"

#include <cassert>
#include <vector>

namespace satemu::media::fs {

struct FileInfo {
    uint32 frameAddress = ~0;
    uint32 fileSize = ~0;
    uint8 unitSize = ~0;
    uint8 interleaveGapSize = ~0;
    uint8 fileNumber = ~0;
    uint8 attributes = ~0;
};
constexpr FileInfo kEmptyFileInfo = {};

// Represents a file or directory in a path table directory.
class FilesystemEntry {
public:
    FilesystemEntry(const iso9660::DirectoryRecord &dirRecord, uint16 parent, uint8 fileID)
        : m_frameAddress(dirRecord.extentPos)
        , m_size(dirRecord.dataSize)
        , m_parent(parent)
        , m_isDirectory(bit::extract<1>(dirRecord.flags)) {
        m_fileInfo = {
            .frameAddress = dirRecord.extentPos,
            .fileSize = dirRecord.dataSize,
            .unitSize = dirRecord.fileUnitSize,
            .interleaveGapSize = dirRecord.interleaveGapSize,
            .fileNumber = fileID,
            .attributes = dirRecord.flags,
        };
    }

    uint32 FrameAddress() const {
        return m_frameAddress;
    }

    uint32 Size() const {
        return m_size;
    }

    uint16 Parent() const {
        return m_parent;
    }

    bool IsFile() const {
        return !m_isDirectory;
    }

    bool IsDirectory() const {
        return m_isDirectory;
    }

    const FileInfo &GetFileInfo() const {
        return m_fileInfo;
    }

private:
    uint32 m_frameAddress;
    uint32 m_size;
    uint16 m_parent;
    bool m_isDirectory;
    FileInfo m_fileInfo;
};

// Represents a path table directory.
class Directory {
public:
    Directory(const iso9660::DirectoryRecord &dirRecord, uint16 parent)
        : m_frameAddress(dirRecord.extentPos)
        , m_parent(parent) {
        assert(bit::extract<1>(dirRecord.flags));
    }

    uint32 FrameAddress() const {
        return m_frameAddress;
    }

    uint16 Parent() const {
        return m_parent;
    }

    const std::vector<FilesystemEntry> &GetContents() const {
        return m_contents;
    }

private:
    uint32 m_frameAddress;
    uint16 m_parent;

    std::vector<FilesystemEntry> m_contents;

    std::vector<FilesystemEntry> &GetContents() {
        return m_contents;
    }

    friend class Filesystem;
};

class Filesystem {
public:
    // Attempts to read the filesystem structure from the specified disc.
    // Returns true if successful.
    // If this function returns false, the filesystem object is invalidated.
    bool Read(const Disc &disc);

    // Attempts to switch to the specified directory.
    // Returns true if succesful, false if fileID is not a directory or does not exist.
    // The filesystem state is not modified on failure.
    bool ChangeDirectory(uint32 fileID, const Filter &filter);

    // Determines if the file system is valid, i.e., there is at least one directory.
    bool IsValid() const {
        return !m_directories.empty();
    }

    // Determines if the file system has a valid current directory.
    bool HasCurrentDirectory() const {
        return m_currDirectory < m_directories.size();
    }

    // Returns the current file offset for file listings.
    uint32 GetFileOffset() const {
        return m_currFileOffset;
    }

    // Returns the number of files in the current directory, minus the self and parent directory references (. and ..)
    uint32 GetFileCount() const;

    // Retrieves the file info for the given file ID from the current directory, based on the current file offset.
    const FileInfo &GetFileInfo(uint8 fileID) const;

private:
    // Directories parsed from the path table records.
    std::vector<Directory> m_directories;

    uint32 m_currDirectory;
    uint32 m_currFileOffset;

    bool ReadPathTableRecords(const Track &track, const media::iso9660::VolumeDescriptor &volDesc);
};

} // namespace satemu::media::fs
