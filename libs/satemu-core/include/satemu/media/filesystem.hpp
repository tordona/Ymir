#pragma once

#include <satemu/core_types.hpp>
#include <satemu/util/bit_ops.hpp>

#include "disc.hpp"
#include "iso9660.hpp"

#include <cassert>
#include <vector>

namespace media::fs {

class FilesystemEntry {
public:
    FilesystemEntry(const iso9660::DirectoryRecord &dirRecord, uint16 parent)
        : m_frameAddress(dirRecord.extentPos)
        , m_size(dirRecord.dataSize)
        , m_parent(parent)
        , m_isDirectory(bit::extract<1>(dirRecord.flags)) {}

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

private:
    uint32 m_frameAddress;
    uint32 m_size;
    uint16 m_parent;
    bool m_isDirectory;
};

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

private:
    // Directories parsed from the path table records.
    std::vector<Directory> m_directories;

    bool ReadPathTableRecords(const Track &track, const media::iso9660::VolumeDescriptor &volDesc);
};

} // namespace media::fs
