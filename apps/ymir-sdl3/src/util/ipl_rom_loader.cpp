#include "ipl_rom_loader.hpp"

#include <ymir/sys/saturn.hpp>

#include "file_loader.hpp"

namespace util {

IPLROMLoadResult LoadIPLROM(std::filesystem::path path, ymir::Saturn &saturn) {
    if (path.empty()) {
        return IPLROMLoadResult::Fail("No IPL ROM provided");
    }

    constexpr auto iplSize = ymir::sys::kIPLSize;
    auto rom = util::LoadFile(path);
    if (rom.size() == iplSize) {
        saturn.LoadIPL(std::span<uint8, iplSize>(rom));
        return IPLROMLoadResult::Success();
    } else {
        return IPLROMLoadResult::Fail(
            fmt::format("IPL ROM size mismatch: expected {} bytes, got {} bytes", iplSize, rom.size()));
    }
}

} // namespace util
