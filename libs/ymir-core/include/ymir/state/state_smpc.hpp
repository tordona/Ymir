#pragma once

#include <ymir/core/types.hpp>

#include <array>

namespace ymir::state {

namespace v1 {

    struct SMPCState {
        alignas(16) std::array<uint8, 7> IREG;
        alignas(16) std::array<uint8, 32> OREG;
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

namespace v2 {

    using v1::SMPCState;

} // namespace v2

namespace v3 {

    using v2::SMPCState;

} // namespace v3

inline namespace v4 {

    using v3::SMPCState;

} // namespace v4

} // namespace ymir::state
