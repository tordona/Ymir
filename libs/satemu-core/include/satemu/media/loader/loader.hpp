#pragma once

#include "loader_bin_cue.hpp"
#include "loader_img_ccd_sub.hpp"
#include "loader_iso.hpp"
#include "loader_mdf_mds.hpp"

#include <satemu/media/disc.hpp>

#include <filesystem>
#include <vector>

namespace satemu::media {

// Attempts to load a CD image from any of the supported file formats:
//   BIN/CUE     (if provided a .cue file)
//   MDF/MDS     (if provided a .mds file)
//   IMG/CCD/SUB (if provided a .ccd file)
//   ISO         (if provided a .iso file)
// Returns true if loading the file (and any auxiliary files) succeeded.
// If this function returns false, the Disc object is invalidated.
bool LoadDisc(std::filesystem::path path, Disc &disc);

} // namespace satemu::media
