#include "ipl_rom_manager.hpp"

#include <satemu/sys/memory_defs.hpp>

#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using namespace satemu;

namespace app {

void IPLROMManager::Scan(fs::path path) {
    std::vector<char> buf{};
    buf.resize(sys::kIPLSize);

    m_entries.clear();

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

        // Build entry
        IPLROMEntry entry{};
        entry.path = canonicalPath;

        // Get database entry
        XXH128Hash hash = CalcHash128(buf.data(), buf.size(), sys::kIPLHashSeed);
        entry.info = db::GetIPLROMInfo(hash);
        entry.hash = hash;

        // Get version string
        entry.versionString.assign(buf.begin() + 0x800, buf.begin() + 0x810);

        // Add it to the list (including unknown entries, in case the image is modified)
        m_entries.insert({canonicalPath, entry});
    }
}

} // namespace app
