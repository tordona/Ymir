#include <ymir/media/loader/loader.hpp>

namespace ymir::media {

bool LoadDisc(std::filesystem::path path, Disc &disc, bool preloadToRAM) {
    if (loader::bincue::Load(path, disc, preloadToRAM)) {
        return true;
    }
    if (loader::mdfmds::Load(path, disc, preloadToRAM)) {
        return true;
    }
    if (loader::ccd::Load(path, disc, preloadToRAM)) {
        return true;
    }
    // NOTE: ISO must be the last to be tested since its detection is more lenient
    if (loader::iso::Load(path, disc, preloadToRAM)) {
        return true;
    }
    return false;
}

} // namespace ymir::media
