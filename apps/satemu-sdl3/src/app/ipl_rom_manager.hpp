#pragma once

#include <satemu/db/ipl_db.hpp>

#include <satemu/core/types.hpp>

#include <filesystem>
#include <unordered_map>

namespace app {

struct IPLROMEntry {
    std::filesystem::path path;
    const satemu::db::IPLROMInfo *info;
    satemu::XXH128Hash hash;
    std::string versionString;
};

class IPLROMManager {
public:
    // Scans the given path recursively for IPL ROM files.
    void Scan(std::filesystem::path path);

    // Retrieves all scanned IPL ROMs.
    const std::unordered_map<std::filesystem::path, IPLROMEntry> &GetROMs() const {
        return m_entries;
    }

private:
    std::unordered_map<std::filesystem::path, IPLROMEntry> m_entries;
};

} // namespace app
