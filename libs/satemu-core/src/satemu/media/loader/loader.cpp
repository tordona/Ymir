#include <satemu/media/loader/loader.hpp>

namespace media {

bool LoadDisc(std::filesystem::path path, Disc &disc) {
    if (LoadBinCue(path, disc)) {
        return true;
    }
    if (LoadMdfMds(path, disc)) {
        return true;
    }
    if (LoadImgCcdSub(path, disc)) {
        return true;
    }
    if (LoadIso(path, disc)) { // NOTE: must be the last to be tested
        return true;
    }
    return false;
}

} // namespace media
