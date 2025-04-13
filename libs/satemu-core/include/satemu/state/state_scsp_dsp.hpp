#pragma once

#include <satemu/core/types.hpp>

#include <array>

namespace satemu::state {

inline namespace v1 {

    struct SCSPDSP {
        std::array<uint64, 128> MPRO;
        std::array<uint32, 128> TEMP;
        std::array<uint32, 32> MEMS;
        std::array<uint16, 64> COEF;
        std::array<uint16, 32> MADRS;
        std::array<sint32, 16> MIXS;
        std::array<sint16, 16> EFREG;
        std::array<sint16, 2> EXTS;

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

} // namespace satemu::state
