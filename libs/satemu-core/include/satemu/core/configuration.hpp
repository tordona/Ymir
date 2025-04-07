#pragma once

#include "configuration_defs.hpp"

#include <satemu/util/date_time.hpp>
#include <satemu/util/observable.hpp>

#include <satemu/core/types.hpp>

#include <vector>

namespace satemu::core {

// Emulator core configuration.
//
// Thread-safety
// -------------
// Unless otherwise noted:
// - Simple (primitive) types can be safely modified from any thread.
// - Complex types (such as containers and observables) cannot be safely modified from any thread.
//
// If you plan to run the emulator core in a dedicated thread, make sure to modify non-thread-safe values exclusively on
// that thread. You may add observers to observable values (both functions and value references), but be aware that
// functions will also run on the emulator thread.
struct Configuration {
    struct System {
        // Automatically change SMPC area code based on compatible regions from loaded discs.
        bool autodetectRegion = true;

        // Preferred region order when autodetecting area codes.
        // If none of these regions is supported by the disc, the first region listed on the disc is used.
        util::Observable<std::vector<config::sys::Region>> preferredRegionOrder =
            std::vector<config::sys::Region>{config::sys::Region::NorthAmerica, config::sys::Region::Japan};
    } system;

    struct RTC {
        // The RTC emulation mode.
        //
        // This value is thread-safe.
        util::Observable<config::rtc::Mode> mode = config::rtc::Mode::Host;

        // The virtual RTC hard reset strategy.
        config::rtc::HardResetStrategy virtHardResetStrategy = config::rtc::HardResetStrategy::Preserve;

        // The virtual RTC hard reset timestamp.
        sint64 virtHardResetTimestamp = util::datetime::to_timestamp(
            util::datetime::DateTime{.year = 1994, .month = 1, .day = 1, .hour = 0, .minute = 0, .second = 0});
    } rtc;

    struct Video {
        // TODO: renderer backend options

        // Runs the VDP1 renderer in a dedicated thread.
        // TODO: Doom graphics breaks or the game freezes with this
        // TODO: move VDP1 rendering to a dedicated thread of its own
        util::Observable<bool> threadedVDP1 = true;

        // Runs the VDP2 renderer in a dedicated thread.
        util::Observable<bool> threadedVDP2 = true;
    } video;

    struct Audio {
        // Sample interpolation method.
        // The Sega Saturn uses linear interpolation.
        //
        // This value is thread-safe.
        util::Observable<config::audio::SampleInterpolationMode> interpolation =
            config::audio::SampleInterpolationMode::Linear;

        // Runs the SCSP and MC68EC000 CPU in a dedicated thread.
        util::Observable<bool> threadedSCSP = false;
    } audio;

    // Notifies all observers registered with all observables.
    // This is useful if you wish to apply the default values instead of replacing them with a configuration system.
    void NotifyObservers();
};

} // namespace satemu::core
