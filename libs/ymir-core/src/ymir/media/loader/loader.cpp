#include <ymir/media/loader/loader.hpp>

#include <ymir/media/loader/loader_bin_cue.hpp>
#include <ymir/media/loader/loader_chd.hpp>
#include <ymir/media/loader/loader_img_ccd_sub.hpp>
#include <ymir/media/loader/loader_iso.hpp>
#include <ymir/media/loader/loader_mdf_mds.hpp>

namespace ymir::media {

bool LoadDisc(std::filesystem::path path, Disc &disc, bool preloadToRAM) {
    // Abuse short-circuiting to pick the first matching loader with less verbosity
    return loader::chd::Load(path, disc, preloadToRAM) ||    //
           loader::bincue::Load(path, disc, preloadToRAM) || //
           loader::mdfmds::Load(path, disc, preloadToRAM) || //
           loader::ccd::Load(path, disc, preloadToRAM) ||    //
           // NOTE: ISO must be the last to be tested since its detection is more lenient
           loader::iso::Load(path, disc, preloadToRAM);
}

} // namespace ymir::media
