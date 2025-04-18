#include "ipl_rom_manager.hpp"

#include <satemu/sys/memory_defs.hpp>

#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using namespace satemu;

namespace app {

void IPLROMManager::Scan(fs::path path, bool append) {
    std::vector<char> buf{};
    buf.resize(sys::kIPLSize);

    if (!append) {
        m_infos.clear();
    }

    for (const fs::directory_entry &dirEntry : fs::recursive_directory_iterator(path)) {
        if (!dirEntry.is_regular_file()) {
            continue;
        }
        if (dirEntry.file_size() != sys::kIPLSize) {
            continue;
        }

        // Read file into buffer
        fs::path canonicalPath = fs::canonical(dirEntry.path());
        {
            std::ifstream in{canonicalPath, std::ios::binary};
            in.read(buf.data(), buf.size());
            if (!in) {
                continue;
            }
        }

        // Get database entry
        XXH128Hash hash = CalcHash128(buf.data(), buf.size(), sys::kIPLHashSeed);
        const db::IPLROMInfo *info = db::GetIPLROMInfo(hash);

        // Add it to the map (including unknown entries, in case the user has a modified image)
        m_infos[canonicalPath] = info;
    }
}

} // namespace app
