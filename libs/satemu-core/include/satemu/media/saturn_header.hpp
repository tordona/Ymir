#pragma once

#include <satemu/util/bitmask_enum.hpp>

#include <satemu/core/types.hpp>

#include <span>
#include <string>

namespace satemu::media {

enum class AreaCode : uint16 {
    None = 0,

    Japan = 1u << 0x1,
    AsiaNTSC = 1u << 0x2,
    NorthAmerica = 1u << 0x4,
    CentralSouthAmericaNTSC = 1u << 0x5,
    Korea = 1u << 0x6,
    AsiaPAL = 1u << 0xA,
    EuropePAL = 1u << 0xC,
    CentralSouthAmericaPAL = 1u << 0xD,
};

enum class PeripheralCode : uint16 {
    None = 0,

    ControlPad = 1u << 0u,
    AnalogPad = 1u << 1u,
    Mouse = 1u << 2u,
    Keyboard = 1u << 3u,
    SteeringWheel = 1u << 4u,
    Multitap = 1u << 5u,
    VirtuaGun = 1u << 6u,
};

struct SaturnHeader {
    static constexpr std::string kExpectedHwId = "SEGA SEGASATURN";

    SaturnHeader() {
        Invalidate();
    }

    SaturnHeader(const SaturnHeader &) = delete;
    SaturnHeader(SaturnHeader &&) = default;

    SaturnHeader &operator=(const SaturnHeader &) = delete;
    SaturnHeader &operator=(SaturnHeader &&) = default;

    void Swap(SaturnHeader &&header) {
        hwID.swap(header.hwID);
        makerID.swap(header.makerID);
        productNumber.swap(header.productNumber);
        version.swap(header.version);
        releaseDate.swap(header.releaseDate);
        deviceInfo.swap(header.deviceInfo);
        std::swap(compatAreaCode, header.compatAreaCode);
        std::swap(compatPeripherals, header.compatPeripherals);
        gameTitle.swap(header.gameTitle);
        std::swap(ipSize, header.ipSize);
        std::swap(masterStackSize, header.masterStackSize);
        std::swap(slaveStackSize, header.slaveStackSize);
        std::swap(firstReadAddress, header.firstReadAddress);
        std::swap(firstReadSize, header.firstReadSize);
    }

    void Invalidate() {
        hwID.clear();
        makerID.clear();
        productNumber.clear();
        version.clear();
        releaseDate.clear();
        deviceInfo.clear();
        compatAreaCode = AreaCode::None;
        compatPeripherals = PeripheralCode::None;
        gameTitle.clear();
        ipSize = 0;
        masterStackSize = 0;
        slaveStackSize = 0;
        firstReadAddress = 0;
        firstReadSize = 0;
    }

    std::string hwID;                 // [00-0F] Hardware identifier
    std::string makerID;              // [10-1F] Maker identifier
    std::string productNumber;        // [20-29] Product number
    std::string version;              // [2A-2F] Version (usually "XX-#####")
    std::string releaseDate;          // [30-37] Release date (YYYYMMDD)
    std::string deviceInfo;           // [38-3F] Device information (usually "CD-#/#")
    AreaCode compatAreaCode;          // [40-49] Compatible area symbols
                                      //           A = Asia PAL
                                      //           B = Central/South America NTSC
                                      //           E = Europe PAL
                                      //           J = Japan
                                      //           K = Korea
                                      //           L = Central/South America PAL
                                      //           T = Asia NTSC
                                      //           U = North America
    PeripheralCode compatPeripherals; // [50-5F] Compatible peripherals
                                      //           A = Analog controller
                                      //           G = Virtua Gun
                                      //           J = Standard controller
                                      //           K = Keyboard
                                      //           M = Mouse
                                      //           S = Steering controller
                                      //           T = Multitap
    std::string gameTitle;            // [60-CF] Game title
    uint32 ipSize;                    // [E0-E3] Initial Program size
    uint32 masterStackSize;           // [E8-EB] Master SH-2 stack size
    uint32 slaveStackSize;            // [EC-EF] Slave SH-2 stack size
    uint32 firstReadAddress;          // [F0-F3] 1st read address
    uint32 firstReadSize;             // [F4-F7] 1st read size

    bool ReadFrom(std::span<uint8, 256> data);
};

} // namespace satemu::media

ENABLE_BITMASK_OPERATORS(satemu::media::AreaCode);
ENABLE_BITMASK_OPERATORS(satemu::media::PeripheralCode);
