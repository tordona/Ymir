#pragma once

#include <satemu/core_types.hpp>

#include <satemu/media/binary_reader/binary_reader.hpp>

#include <array>
#include <filesystem>
#include <memory>
#include <vector>

namespace media {

struct Track {
    std::unique_ptr<IBinaryReader> binaryReader;
    uint32 sectorSize;
    uint8 controlADR;

    uint32 startFrameAddress;
    uint32 endFrameAddress;
    bool interleavedSubchannel; // true=96-byte PW subchannel, interleaved
};

struct Session {
    std::array<Track, 99> tracks;
    uint32 numTracks;

    uint32 startFrameAddress;
    uint32 endFrameAddress;

    // The table of contents contains the following entries:
    // (partially from https://www.ecma-international.org/wp-content/uploads/ECMA-394_1st_edition_december_2010.pdf)
    //
    // 0-98: One entry per track in the following format:
    //   31-24  track control/ADR
    //   23-0   track start frame address
    // Unused tracks contain 0xFFFFFFFF
    //
    // 99: Point A0
    //   31-24  first track control/ADR
    //   23-16  first track number (PMIN)
    //   15-8   program area format (PSEC):
    //            0x00: CD-DA and CD-ROM
    //            0x10: CD-i
    //            0x20: CD-ROM-XA
    //    7-0   PFRAME - always zero
    //
    // 100: Point A1
    //   31-24  last track control/ADR
    //   23-16  last track number (PMIN)
    //   15-8   PSEC - always zero
    //    7-0   PFRAME - always zero
    //
    // 101: Point A2
    //   31-24  leadout track control/ADR
    //   23-0   leadout frame address

    std::array<uint32, 99 + 3> toc;

    // Build table of contents using track information
    void BuildTOC() {
        uint32 firstTrackNum = 0;
        uint32 lastTrackNum = 0;
        for (int i = 0; i < 99; i++) {
            auto &track = tracks[i];
            if (track.controlADR != 0x00) {
                toc[i] = (track.controlADR << 24u) | track.startFrameAddress;
                if (firstTrackNum == 0) {
                    firstTrackNum = i + 1;
                }
                lastTrackNum = i + 1;
            } else {
                toc[i] = 0xFFFFFFFF;
            }
        }

        if (firstTrackNum != 0) {
            toc[99] = (tracks[0].controlADR << 24u) | (firstTrackNum << 16u);
            toc[100] = (tracks[lastTrackNum - 1].controlADR << 24u) | (lastTrackNum << 16u);
            toc[101] = (tracks[lastTrackNum - 1].controlADR << 24u) | endFrameAddress;
        } else {
            toc[99] = toc[100] = toc[101] = 0xFFFFFFFF;
        }
    }
};

struct Disc {
    std::vector<Session> sessions;
};

} // namespace media
