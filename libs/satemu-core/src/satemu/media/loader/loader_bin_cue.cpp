#include <satemu/media/loader/loader_bin_cue.hpp>

#include <satemu/media/binary_reader/binary_reader_impl.hpp>
#include <satemu/media/frame_address.hpp>

#include <satemu/util/scope_guard.hpp>

#include <fmt/format.h>

#include <fstream>
#include <set>
#include <sstream>

namespace satemu::media::loader::bincue {

const std::set<std::string> kValidCueKeywords = {
    // General commands
    "CATALOG", "CD_DA", "CD_ROM", "CD_ROM_XA", "CDTEXTFILE", "FILE", "REM", "TRACK",
    // CD-Text commands
    "ARRANGER", "COMPOSER", "DISC_ID", "GENRE", "ISRC", "MESSAGE", "PERFORMER", "SIZE_INFO", "SONGWRITER", "TITLE",
    "TOC_INFO1", "TOC_INFO2", "UPC_EAN",
    // Track commands
    "COPY", "DATAFILE", "FLAGS", "FIFO", "FOUR_CHANNEL_AUDIO", "INDEX", "POSTGAP", "PREGAP", "PRE_EMPHASIS", "SILENCE",
    "START", "TWO_CHANNEL_AUDIO", "ZERO",
    "NO" // NO COPY, NO PRE_EMPHASIS
};

const std::set<std::string> kValidCueNOKeywords = {"COPY", "PRE_EMPHASIS"};

namespace fs = std::filesystem;

bool Load(std::filesystem::path cuePath, Disc &disc) {
    std::ifstream in{cuePath, std::ios::binary};

    util::ScopeGuard sgInvalidateDisc{[&] { disc.sessions.clear(); }};

    if (!in) {
        // fmt::println("BIN/CUE: Could not load CUE file");
        return false;
    }

    // Peek first line to check if this is really a CUE file
    // uint32 lineNum = 1;
    std::string line{};
    if (!std::getline(in, line)) {
        // fmt::println("BIN/CUE: Could not read file");
        return false;
    }

    {
        std::istringstream ins{line};
        std::string keyword{};
        ins >> keyword;
        if (!kValidCueKeywords.contains(keyword)) {
            // fmt::println("BIN/CUE: Not a valid CUE file");
            return false;
        }
        if (keyword == "NO") {
            // NO must be followed by COPY or PRE_EMPHASIS
            ins >> keyword;
            if (!kValidCueNOKeywords.contains(keyword)) {
                // fmt::println("BIN/CUE: Not a valid CUE file");
                return false;
            }
        }
    }

    // BIN/CUE files have only one session
    disc.sessions.clear();
    auto &session = disc.sessions.emplace_back();
    session.startFrameAddress = 0;

    // auto cueFileSize = fs::file_size(cuePath);
    // fmt::println("BIN/CUE: File size: {} bytes", cueFileSize);

    fs::path curFile{};

    uint32 nextTrackNum = 0;
    uint32 frameAddress = 150; // start with lead-in
    uint32 pregapFrames = 0;
    uint32 prevPregapFrames = 0;
    uint32 currTrackIndex = ~0;
    uintmax_t binFileOffset = 0;
    uintmax_t binFileSize = 0;
    uint32 prevM = 0;
    uint32 prevS = 0;
    uint32 prevF = 0;
    std::array<uintmax_t, 99> trackFileOffsets{};

    // File format validation flags
    bool hasFile = false;
    bool hasTrack = false;
    bool hasIndex = false;
    bool hasIndex0 = false;
    bool hasPregap = false;
    bool hasPostgap = false;

    std::shared_ptr<IBinaryReader> binaryReader{};

    do {
        std::istringstream ins{line};
        std::string keyword{};
        ins >> keyword;

        if (!kValidCueKeywords.contains(keyword)) {
            // fmt::println("BIN/CUE: Found invalid keyword {} (line {})", keyword, lineNum);
            return false;
        }
        if (keyword == "NO") {
            // NO must be followed by COPY or PRE_EMPHASIS
            ins >> keyword;
            if (!kValidCueNOKeywords.contains(keyword)) {
                // fmt::println("BIN/CUE: Found invalid keyword NO {} (line {})", keyword, lineNum);
                return false;
            }
            keyword = "NO " + keyword;
        }

        if (keyword == "FILE") {
            // FILE [filename] [format]
            std::string filename = line.substr(line.find("FILE") + 5);
            auto pos = filename.find_last_of(' ') + 1;
            if (pos == std::string::npos) {
                // fmt::println("BIN/CUE: Invalid FILE entry: {} (line {})", line, lineNum);
                return false;
            }

            filename = filename.substr(0, pos - 1);
            if (filename.starts_with('\"')) {
                filename = filename.substr(1);
            }
            if (filename.ends_with('\"')) {
                filename = filename.substr(0, filename.size() - 1);
            }

            fs::path binPath = cuePath.parent_path() / filename;
            if (!fs::is_regular_file(binPath)) {
                // fmt::println("BIN/CUE: File not found: {} (line {})", binPath.string(), lineNum);
                return false;
            }

            // Close current track
            if (hasTrack) {
                auto &track = session.tracks[currTrackIndex];

                const uintmax_t length = binFileSize - binFileOffset;
                const uint32 frames = length / track.sectorSize;
                track.endFrameAddress = track.startFrameAddress + frames - 1;
                track.binaryReader =
                    std::make_unique<SharedSubviewBinaryReader>(binaryReader, trackFileOffsets[currTrackIndex], length);
                frameAddress += frames;
            }

            // Reset pointer and load new file
            std::error_code err{};
            // TODO: dynamically choose implementation from configuration
            // binaryReader = std::make_shared<MemoryBinaryReader>(binPath, err);
            // binaryReader = std::make_shared<FileBinaryReader>(binPath, err);
            binaryReader = std::make_shared<MemoryMappedBinaryReader>(binPath, err);
            if (err) {
                // fmt::println("BIN/CUE: Failed to load file - {} (line {})", err.message(), lineNum);
                return false;
            }

            binFileOffset = 0;
            binFileSize = binaryReader->Size();
            prevM = 0;
            prevS = 0;
            prevF = 0;

            // fmt::println("BIN/CUE: File {} - {} bytes", filename, binFileSize);

            hasTrack = false;
            hasFile = true;
        } else if (keyword == "TRACK") {
            // TRACK [number] [datatype]
            if (!hasFile) {
                // fmt::println("BIN/CUE: Found TRACK without a FILE (line {})", lineNum);
                return false;
            }

            uint32 trackNum{};
            std::string format{};
            ins >> trackNum >> format;

            if (nextTrackNum == 0) {
                nextTrackNum = trackNum + 1;
                session.firstTrackIndex = trackNum - 1;
            } else if (trackNum < nextTrackNum) {
                // fmt::println("BIN/CUE: Unexpected track order: expected {} but found {} (line {})", nextTrackNum,
                //              trackNum, lineNum);
                return false;
            }
            session.lastTrackIndex = trackNum - 1;

            // fmt::println("BIN/CUE:   Track {:02d} - {}", trackNum, format);
            uint32 sectorSize{};
            uint32 controlADR{};
            if (format.starts_with("MODE")) {
                // Data track
                if (format.ends_with("_RAW")) {
                    // MODE1_RAW and MODE2_RAW
                    sectorSize = 2352;
                } else {
                    // Known modes:
                    // MODE1/2048   MODE2/2048
                    //              MODE2/2324
                    //              MODE2/2336
                    // MODE1/2352   MODE2/2352
                    sectorSize = std::stoi(format.substr(6));
                }
                controlADR = 0x41;
            } else if (format == "CDG") {
                // Karaoke CD+G track
                sectorSize = 2448;
                // TODO: control/ADR?
            } else if (format == "AUDIO") {
                // Audio track
                sectorSize = 2048;
                controlADR = 0x01;
            } else {
                // fmt::println("BIN/CUE: Unsupported track format (line {})", lineNum);
                return false;
            }
            // fmt::println("BIN/CUE:   Sector size: {} bytes", sectorSize);
            // fmt::println("BIN/CUE:   Control/ADR: {:02X}", controlADR);

            currTrackIndex = trackNum - 1;

            auto &track = session.tracks[currTrackIndex];
            track.SetSectorSize(sectorSize);
            track.controlADR = controlADR;
            track.interleavedSubchannel = false;

            session.numTracks++;
            if (session.numTracks > 99) {
                // fmt::println("BIN/CUE: Too many tracks");
                return false;
            }

            prevPregapFrames = pregapFrames;
            pregapFrames = 0;
            hasTrack = true;
            hasIndex = false;
            hasIndex0 = false;
            hasPregap = false;
            hasPostgap = false;
        } else if (keyword == "INDEX") {
            // INDEX [number] [mm:ss:ff]
            if (hasPostgap) {
                // fmt::println("BIN/CUE: Found INDEX after POSTGAP in a TRACK (line {})", lineNum);
                return false;
            }

            uint32 indexNum{};
            std::string msf{};
            ins >> indexNum >> msf;

            uint32 m = std::stoi(msf.substr(0, 2));
            uint32 s = std::stoi(msf.substr(3, 5));
            uint32 f = std::stoi(msf.substr(6, 8));
            // fmt::println("BIN/CUE:     Index {:d} - {:02d}:{:02d}:{:02d}", indexNum, m, s, f);

            if (!hasTrack) {
                // fmt::println("BIN/CUE: Found INDEX before TRACK (line {})", lineNum);
                return false;
            }

            // Look for the data index only
            auto &track = session.tracks[currTrackIndex];
            bool startTrack = false;
            if (indexNum == 0) {
                if (hasPregap) {
                    // fmt::println("BIN/CUE: Found INDEX 00 and PREGAP in the same TRACK (line {})", lineNum);
                    return false;
                }

                // Audio tracks start on pregap
                startTrack = track.controlADR == 0x01;

                hasPregap = true;
                hasIndex0 = true;
            } else if (indexNum == 1) {
                // Data tracks skip pregap
                // Audio tracks start here if there is no INDEX 00
                startTrack = track.controlADR == 0x41 || !hasIndex0;
            }
            // We don't care about subindices for now

            if (startTrack) {
                binFileOffset += static_cast<uintmax_t>(prevPregapFrames) * track.sectorSize;

                // Close previous track
                if (currTrackIndex > 0) {
                    auto &prevTrack = session.tracks[currTrackIndex - 1];
                    const uintmax_t binFileLength = TimestampToFileOffset(m, s, f, prevTrack.sectorSize) -
                                                    TimestampToFileOffset(prevM, prevS, prevF, prevTrack.sectorSize);
                    if (prevTrack.endFrameAddress < prevTrack.startFrameAddress) {
                        const uint32 frames = binFileLength / prevTrack.sectorSize;
                        prevTrack.endFrameAddress = prevTrack.startFrameAddress + frames - 1;
                        prevTrack.binaryReader = std::make_unique<SharedSubviewBinaryReader>(
                            binaryReader, trackFileOffsets[currTrackIndex - 1], binFileLength);
                        frameAddress += frames;
                    }
                    binFileOffset += binFileLength;
                }
                // fmt::println("BIN/CUE: Track {} offset = {:X}  {:02d}:{:02d}:{:02d}", currTrackIndex, binFileOffset,
                // m, s, f);

                // Start new track
                track.startFrameAddress = frameAddress;
                trackFileOffsets[currTrackIndex] = binFileOffset;
            }

            prevM = m;
            prevS = s;
            prevF = f;
            hasIndex = true;
        } else if (keyword == "PREGAP") {
            // PREGAP [mm:ss:ff]
            if (hasIndex) {
                // fmt::println("BIN/CUE: Found PREGAP after INDEX (line {})", lineNum);
                return false;
            }
            if (hasPregap) {
                // fmt::println("BIN/CUE: Found multiple PREGAPS in a TRACK (line {})", lineNum);
                return false;
            }

            std::string msf{};
            ins >> msf;

            uint32 m = std::stoi(msf.substr(0, 2));
            uint32 s = std::stoi(msf.substr(3, 5));
            uint32 f = std::stoi(msf.substr(6, 8));
            // fmt::println("BIN/CUE:     Pregap - {:02d}:{:02d}:{:02d}", m, s, f);

            pregapFrames = TimestampToFrameAddress(m, s, f);

            hasPregap = true;
        } else if (keyword == "POSTGAP") {
            // POSTGAP [mm:ss:ff]
            if (!hasIndex) {
                // fmt::println("BIN/CUE: Found POSTGAP before INDEX (line {})", lineNum);
                return false;
            }
            if (hasPostgap) {
                // fmt::println("BIN/CUE: Found multiple POSTGAPS in a TRACK (line {})", lineNum);
                return false;
            }

            std::string msf{};
            ins >> msf;
            // uint32 m = std::stoi(msf.substr(0, 2));
            // uint32 s = std::stoi(msf.substr(3, 5));
            // uint32 f = std::stoi(msf.substr(6, 8));
            //  fmt::println("BIN/CUE:     Postgap - {:02d}:{:02d}:{:02d}", m, s, f);

            hasPostgap = true;
        } else {
            // fmt::println("BIN/CUE: Skipping {}", keyword);
        }

        // lineNum++;
    } while (std::getline(in, line));

    // Sanity checks
    if (!hasFile) {
        // fmt::println("BIN/CUE: No FILE specified");
        return false;
    }

    // Finish last track
    if (hasTrack) {
        auto &track = session.tracks[currTrackIndex];

        const uintmax_t length = binFileSize - binFileOffset;
        const uint32 frames = length / track.sectorSize;
        track.endFrameAddress = track.startFrameAddress + frames - 1;
        track.binaryReader =
            std::make_unique<SharedSubviewBinaryReader>(binaryReader, trackFileOffsets[currTrackIndex], length);
        frameAddress += frames;
    }

    // Finish session
    session.endFrameAddress = frameAddress - 1;
    session.BuildTOC();

    sgInvalidateDisc.Cancel();

    return true;
}

} // namespace satemu::media::loader::bincue
