#pragma once

#include <satemu/db/ipl_db.hpp>

#include <filesystem>
#include <map>

namespace app {

class IPLROMManager {
public:
    // Scans the given path recursively for IPL ROM files.
    // If `append` is true, the files are added to the existing list, otherwise the list is replaced with IPL ROM files
    // found in this path.
    void Scan(std::filesystem::path path, bool append = true);

    // Retrieves all scanned IPL ROMs.
    const std::map<std::filesystem::path, const satemu::db::IPLROMInfo *> &Get() const {
        return m_infos;
    }

private:
    std::map<std::filesystem::path, const satemu::db::IPLROMInfo *> m_infos;
};

} // namespace app
