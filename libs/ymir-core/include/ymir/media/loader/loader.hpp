#pragma once

#include "loader_bin_cue.hpp"
#include "loader_img_ccd_sub.hpp"
#include "loader_iso.hpp"
#include "loader_mdf_mds.hpp"

#include <ymir/media/disc.hpp>

#include <filesystem>
#include <vector>

namespace ymir::media {

// Attempts to load a CD image from any of the supported file formats:
//   BIN/CUE     (if provided a .cue file)
//   MDF/MDS     (if provided a .mds file)
//   IMG/CCD/SUB (if provided a .ccd file)
//   ISO         (if provided a .iso file)
// Returns true if loading the file (and any auxiliary files) succeeded.
// If this function returns false, the Disc object is invalidated.
// preloadToRAM specifies if the entire disc image should be preloaded into memory.
bool LoadDisc(std::filesystem::path path, Disc &disc, bool preloadToRAM);

} // namespace ymir::media
