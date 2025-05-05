#pragma once

#include <ymir/core/types.hpp>

#include <array>

namespace ymir::state {

namespace v1 {

    struct SCSPDSP {
        alignas(16) std::array<uint64, 128> MPRO;
        alignas(16) std::array<uint32, 128> TEMP;
        alignas(16) std::array<uint32, 32> MEMS;
        alignas(16) std::array<uint16, 64> COEF;
        alignas(16) std::array<uint16, 32> MADRS;
        alignas(16) std::array<sint32, 16> MIXS;
        alignas(16) std::array<sint16, 16> EFREG;
        alignas(16) std::array<sint16, 2> EXTS;

        uint8 RBP;
        uint8 RBL;

        uint32 INPUTS;

        uint32 SFT_REG;
        uint16 FRC_REG;
        uint32 Y_REG;
        uint16 ADRS_REG;

        uint16 MDEC_CT;

        bool readPending;
        bool readNOFL;
        uint32 readValue;

        bool writePending;
        uint16 writeValue;

        uint32 readWriteAddr;
    };

} // namespace v1

namespace v2 {

    using v1::SCSPDSP;

} // namespace v2

namespace v3 {

    using v2::SCSPDSP;

} // namespace v3

inline namespace v4 {

    using v3::SCSPDSP;

} // namespace v4

} // namespace ymir::state
