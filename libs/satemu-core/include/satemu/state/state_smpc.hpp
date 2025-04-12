#pragma once

#include <satemu/core/types.hpp>

#include <array>

namespace satemu::state {

inline namespace v1 {

    struct SMPCState {
        std::array<uint8, 7> IREG;
        std::array<uint8, 32> OREG;
        uint8 COMREG;
        uint8 SR;
        bool SF;
        uint8 PDR1;
        uint8 PDR2;
        uint8 DDR1;
        uint8 DDR2;
        uint8 IOSEL;
        uint8 EXLE;

        struct INTBACK {
            bool getPeripheralData;
            bool optimize;
            uint8 port1mode;
            uint8 port2mode;

            std::vector<uint8> report;
            size_t reportOffset;
            bool inProgress;
        } intback;

        uint8 busValue;
        bool resetDisable;

        sint64 rtcTimestamp;
        uint64 rtcSysClockCount;
    };

} // namespace v1

} // namespace satemu::state
