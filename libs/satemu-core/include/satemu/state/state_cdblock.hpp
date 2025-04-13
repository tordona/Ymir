#pragma once

#include <satemu/hw/cdblock/cdblock_defs.hpp>

#include <satemu/core/hash.hpp>
#include <satemu/core/types.hpp>

#include <array>

namespace satemu::state {

inline namespace v1 {

    struct CDBlockState {
        Hash128 discHash;

        std::array<uint16, 4> CR;
        uint16 HIRQ;
        uint16 HIRQMASK;

        struct StatusState {
            uint8 statusCode;
            uint32 frameAddress;
            uint8 flags;
            uint8 repeatCount;
            uint8 controlADR;
            uint8 track;
            uint8 index;
        } status;

        bool readyForPeriodicReports;

        uint32 currDriveCycles;
        uint32 targetDriveCycles;

        uint32 playStartParam;
        uint32 playEndParam;
        uint8 playRepeatParam;
        bool scanDirection;
        uint8 scanCounter;

        uint32 playStartPos;
        uint32 playEndPos;
        uint8 playMaxRepeat;
        bool playFile;
        bool bufferFullPause;

        uint8 readSpeed;

        uint8 discAuthStatus;
        uint8 mpegAuthStatus;

        enum class TransferType { None, TOC, GetSector, GetThenDeleteSector, FileInfo, Subcode };
        TransferType xferType;
        uint32 xferPos;
        uint32 xferLength;
        uint32 xferCount;

        uint32 xferSectorPos;
        uint32 xferSectorEnd;
        uint8 xferPartition;

        uint32 xferCurrFileID;

        std::array<uint8, 24> xferSubcodeBuffer;
        uint32 xferSubcodeFrameAddress;
        uint32 xferSubcodeGroup;

        uint32 xferExtraCount;

        struct BufferState {
            std::array<uint8, 2352> data;
            uint16 size;
            uint32 frameAddress;
            uint8 fileNum;
            uint8 chanNum;
            uint8 submode;
            uint8 codingInfo;

            uint8 partitionIndex;
        };
        std::array<BufferState, cdblock::kNumBuffers> buffers;
        BufferState scratchBuffer;

        struct FilterState {
            uint32 startFrameAddress;
            uint32 frameAddressCount;

            uint8 mode;

            uint8 fileNum;
            uint8 chanNum;

            uint8 submodeMask;
            uint8 submodeValue;

            uint8 codingInfoMask;
            uint8 codingInfoValue;

            uint8 trueOutput;
            uint8 falseOutput;
        };
        std::array<FilterState, cdblock::kNumFilters> filters;

        uint8 cdDeviceConnection;
        uint8 lastCDWritePartition;

        uint32 calculatedPartitionSize;

        uint32 getSectorLength;
        uint32 putSectorLength;

        bool processingCommand;
    };

} // namespace v1

} // namespace satemu::state
