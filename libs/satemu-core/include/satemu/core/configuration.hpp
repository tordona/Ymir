#pragma once

#include <satemu/util/observable.hpp>

#include <satemu/core/types.hpp>

#include <vector>

namespace satemu::core {

enum class Region {
    Japan = 0x1,
    AsiaNTSC = 0x2,
    NorthAmerica = 0x4,
    CentralSouthAmericaNTSC = 0x5,
    Korea = 0x6,
    AsiaPAL = 0xA,
    EuropePAL = 0xC,
    CentralSouthAmericaPAL = 0xD,
};

// Emulator core configuration.
//
// Simple (primitive) types can be safely modified from any thread, unless otherwise noted.
// Complex types (such as containers and observables) cannot be safely modified from any thread.
//
// If you plan to run the emulator core in a dedicated thread, make sure to modify non-thread-safe values exclusively on
// that thread. You may add observer functions to observable values, but be aware that the functions will also run on
// the emulator thread.
struct Configuration {
    struct System {
        // Automatically change SMPC area code based on compatible regions from loaded discs.
        bool autodetectRegion = true;

        // Preferred region order when autodetecting area codes.
        // If none of these regions is supported by the disc, the first region listed on the disc is used.
        util::Observable<std::vector<Region>> preferredRegionOrder =
            std::vector<Region>{Region::NorthAmerica, Region::Japan};
    } system;
};

} // namespace satemu::core
