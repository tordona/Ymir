#pragma once

namespace satemu::config {

namespace sys {
    enum class Region {
        Japan = 0x1,        // (J) Domestic NTSC - Japan
        AsiaNTSC = 0x2,     // (T) Asia NTSC - Asia Region (Taiwan, Philippines, South Korea)
        NorthAmerica = 0x4, // (U) North America NTSC - North America (US, Canada), Latin America (Brazil only)
        EuropePAL = 0xC,    // (E) PAL - Europe, Southeast Asia (China, Middle East), Latin America

        CentralSouthAmericaNTSC = 0x5, // (B) (obsolete -> U) Central/South America NTSC
        Korea = 0x6,                   // (K) (obsolete -> T) South Korea
        AsiaPAL = 0xA,                 // (A) (obsolete -> E) Asia PAL
        CentralSouthAmericaPAL = 0xD,  // (L) (obsolete -> E) Central/South America PAL
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
