#pragma once

#include "configuration_defs.hpp"

#include <satemu/sys/clocks.hpp>

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
// that thread. You may add observers to observable values (both functions and value references), but be aware that the
// functions will also run on the emulator thread.
struct Configuration {
    struct System {
        // Automatically change SMPC area code based on compatible regions from loaded discs.
        bool autodetectRegion = true;

        // Preferred region order when autodetecting area codes.
        // If none of these regions is supported by the disc, the first region listed on the disc is used.
        util::Observable<std::vector<config::sys::Region>> preferredRegionOrder =
            std::vector<config::sys::Region>{config::sys::Region::NorthAmerica, config::sys::Region::Japan};

        // Specifies the video standard for the system, which affects video timings and clock rates.
        util::Observable<config::sys::VideoStandard> videoStandard = config::sys::VideoStandard::NTSC;

        // Enables SH-2 cache emulation.
        //
        // Most games work fine without this. Enable it to improve accuracy and compatibility with specific games.
        //
        // Enabling this option incurs a small performance penalty and purges all SH-2 caches.
        util::Observable<bool> emulateSH2Cache = false;
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

        // Runs the VDP renderer in a dedicated thread.
        util::Observable<bool> threadedVDP = true;
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

    struct CDBlock {
        // Read speed factor for high-speed mode.
        // Accepted values range from 2 to 200.
        // The default is 2, matching the real Saturn CD drive's speed.
        //
        // This value is thread-safe.
        util::Observable<uint8> readSpeedFactor = 2;
    } cdblock;

    // Notifies all observers registered with all observables.
    // This is useful if you wish to apply the default values instead of replacing them with a configuration system.
    void NotifyObservers();
};

} // namespace satemu::core
