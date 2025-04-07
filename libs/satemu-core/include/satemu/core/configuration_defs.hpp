#pragma once

namespace satemu::config {

namespace sys {
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

    enum class VideoStandard { NTSC, PAL };
} // namespace sys

namespace rtc {
    enum class Mode {
        // Syncs RTC to host clock. Uses an offset to adjust time.
        Host,

        // Emulates RTC time, syncing to the main bus clock.
        // Behavior on hard reset/power on can be configured to one of:
        // - Resync to host clock
        // - Resync to a fixed time point (for deterministic behavior)
        // - Preserve current time
        Virtual,
    };

    // Emulated RTC behavior on hard reset.
    enum class HardResetStrategy {
        // Sync emulated RTC to host clock.
        SyncToHost,

        // Reset emulated RTC to a fixed timestamp. Useful for TAS since it has deterministic behavior.
        ResetToFixedTime,

        // Preserve current RTC timestamp.
        Preserve,
    };
} // namespace rtc

namespace audio {
    enum class SampleInterpolationMode { NearestNeighbor, Linear };
} // namespace audio

} // namespace satemu::config
