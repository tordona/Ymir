#include <satemu/media/loader/loader.hpp>

namespace media {

bool LoadDisc(std::filesystem::path path, Disc &disc) {
    if (loader::bincue::Load(path, disc)) {
        return true;
    }
    if (loader::mdfmds::Load(path, disc)) {
        return true;
    }
    if (loader::ccd::Load(path, disc)) {
        return true;
    }
    // NOTE: ISO must be the last to be tested since its detection is more lenient
    if (loader::iso::Load(path, disc)) {
        return true;
    }
    return false;
}

} // namespace media
