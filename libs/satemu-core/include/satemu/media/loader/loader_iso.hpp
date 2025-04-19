#pragma once

#include <satemu/media/disc.hpp>

#include <filesystem>

// An ISO file contains raw sectors of a data track. There's no specific format to the file itself, but the contained
// data must use a sector-based filesystem such as IS0 9660.
//
// ISO files do not store track or session information, therefore the entire file describes a disc with a single session
// and one data track.
//
// The sector size must be derived from the total file size, which must be a multiple of 2048 or 2352.

namespace satemu::media::loader::iso {

// Attempts to load an ISO file from isoPath into the specified Disc object.
// Returns true if loading the file succeeded.
// If this function returns false, the Disc object is invalidated.
// preloadToRAM specifies if the entire disc image should be preloaded into memory.
bool Load(std::filesystem::path isoPath, Disc &disc, bool preloadToRAM);

} // namespace satemu::media::loader::iso
