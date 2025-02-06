#include <satemu/media/loader/loader_iso.hpp>

#include <satemu/media/binary_reader/binary_reader_impl.hpp>

#include <satemu/util/scope_guard.hpp>

#include <fmt/format.h>

#include <fstream>

namespace fs = std::filesystem;

namespace satemu::media::loader::iso {

bool Load(std::filesystem::path isoPath, Disc &disc) {
    std::ifstream in{isoPath, std::ios::binary};

    util::ScopeGuard sgInvalidateDisc{[&] { disc.Invalidate(); }};

    if (!in) {
        // fmt::println("ISO: Could not load ISO file");
        return false;
    }

    // Peek first 12 bytes to see if we have synchronization bytes, indicating 2352 byte sectors.
    // If the sync bytes are not found, assume 2048 byte sectors.
    uint32 sectorSize;
    {
        std::array<uint8, 12> start{};
        in.read(reinterpret_cast<char *>(start.data()), start.size());
        if (!in) {
            if (in.gcount() < 12) {
                // fmt::println("ISO: File too small");
            } else {
                std::error_code err{errno, std::generic_category()};
                // fmt::println("ISO: File could not be read: {}", err.message());
            }
            return false;
        }
        in.seekg(0, std::ios::beg);

        static constexpr std::array<uint8, 12> syncBytes = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
                                                            0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
        sectorSize = start == syncBytes ? 2352 : 2048;
    }
    // fmt::println("ISO: Sector size: {} bytes", sectorSize);

    // Sanity check: ensure file contains an exact multiple of the sector size
    const uintmax_t fileSize = fs::file_size(isoPath);
    if (fileSize % sectorSize != 0) {
        // fmt::println("ISO: File is truncated");
        return false;
    }
    const uint32 frames = fileSize / sectorSize;

    // Build disc structure: one session with one track encompassing the whole file
    disc.sessions.clear();
    auto &session = disc.sessions.emplace_back();
    session.numTracks = 1;
    session.firstTrackIndex = 0;
    session.lastTrackIndex = 0;
    session.startFrameAddress = 0;
    session.endFrameAddress = session.startFrameAddress + frames + 150;

    auto &track = session.tracks[0];
    track.SetSectorSize(sectorSize);
    track.controlADR = 0x41; // always a data track
    track.interleavedSubchannel = false;
    track.startFrameAddress = session.startFrameAddress + 150;
    track.endFrameAddress = session.endFrameAddress;

    std::error_code err{};
    // TODO: dynamically choose implementation from configuration
    // track.binaryReader = std::make_unique<MemoryBinaryReader>(isoPath, err);
    // track.binaryReader = std::make_unique<FileBinaryReader>(isoPath, err);
    track.binaryReader = std::make_unique<MemoryMappedBinaryReader>(isoPath, err);
    if (err) {
        // fmt::println("ISO: Could not create file reader: {}", err.message());
        return false;
    }

    // Read the header
    {
        std::array<uint8, 256> header{};
        const uintmax_t readSize = track.binaryReader->Read(sectorSize == 2352 ? 16 : 0, 256, header);
        if (readSize < 256) {
            // fmt::println("ISO: Image file truncated");
            return false;
        }

        disc.header.ReadFrom(header);
    }

    session.BuildTOC();

    sgInvalidateDisc.Cancel();

    return true;
}

} // namespace satemu::media::loader::iso
