#pragma once

#include <satemu/core_types.hpp>

#include <satemu/util/bit_ops.hpp>

#include <span>

namespace satemu::media {

struct Filter {
    static constexpr uint8 kDisconnected = 0xFF;

    Filter() {
        Reset();
    }

    void Reset() {
        ResetConditions();

        trueOutput = 0; // TODO: should == index
        falseOutput = kDisconnected;
    }

    void ResetConditions() {
        startFrameAddress = 0;
        frameAddressCount = 0;

        mode = 0;

        fileNum = 0;
        chanNum = 0;

        submodeMask = 0;
        submodeValue = 0;

        codingInfoMask = 0;
        codingInfoValue = 0;
    }

    bool Test(std::span<uint8> data) const {
        // TODO: implement
        // const bool invertSubheaderConds = bit::extract<4>(mode);
        if (bit::extract<0>(mode)) {
            // TODO: filter by file number; honor invert flag
        }
        if (bit::extract<1>(mode)) {
            // TODO: filter by channel number; honor invert flag
        }
        if (bit::extract<2>(mode)) {
            // TODO: filter by submode; honor invert flag
        }
        if (bit::extract<3>(mode)) {
            // TODO: filter by coding information; honor invert flag
        }
        if (bit::extract<6>(mode)) {
            // TODO: filter by frame address range
        }
        return true;
    }

    // Frame address filters
    uint32 startFrameAddress;
    uint32 frameAddressCount;

    // Filter mode
    //   0   Filter by file number
    //   1   Filter by channel number
    //   2   Filter by submode
    //   3   Filter by coding information
    //   4   Invert subheader conditions (all but frame address range)
    //   6   Filter by frame address range
    uint8 mode;

    // File and channel number subheader filters
    uint8 fileNum;
    uint8 chanNum;

    // Submode subheader filters
    // (submode & submodeMask) == submodeValue
    uint8 submodeMask;
    uint8 submodeValue;

    // Coding information subheader filters
    // (codingInfo & codingInfoMask) == codingInfoValue
    uint8 codingInfoMask;
    uint8 codingInfoValue;

    // Output connectors
    uint8 trueOutput;  // buffer index;  0xFF = disconnected
    uint8 falseOutput; // filter number; 0xFF = disconnected
};

} // namespace satemu::media
