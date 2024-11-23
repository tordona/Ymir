#pragma once

#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/scu/scu.hpp>
#include <satemu/hw/sh2/sh2.hpp>
#include <satemu/hw/smpc/smpc.hpp>

#include <satemu/util/inline.hpp>

namespace satemu {

class Saturn {
public:
    Saturn();

    void Reset(bool hard);

    void LoadIPL(std::span<uint8, kIPLSize> ipl);

    void Step();

    ALWAYS_INLINE SH2 &MasterSH2() {
        return m_masterSH2;
    }

private:
    SH2Bus m_sh2bus;

    SH2 m_masterSH2;
    SH2 m_slaveSH2;

    SCU m_SCU;
    SMPC m_SMPC;
    SCSP m_SCSP;
    CDBlock m_CDBlock;
};

} // namespace satemu
