#pragma once

#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/scu/scu.hpp>
#include <satemu/hw/sh2/sh2.hpp>
#include <satemu/hw/smpc/smpc.hpp>

namespace satemu {

class Saturn {
public:
    Saturn()
        : m_sh2bus(m_SCU, m_SMPC, m_SCSP, m_CDBlock)
        , m_masterSH2(m_sh2bus, true)
        , m_slaveSH2(m_sh2bus, false) {
        Reset(true);
    }

    void Reset(bool hard) {
        m_sh2bus.Reset(hard);
        m_masterSH2.Reset(hard);
        m_SCU.Reset(hard);
        m_SMPC.Reset(hard);
        m_SCSP.Reset(hard);
        m_CDBlock.Reset(hard);
    }

    void LoadIPL(std::span<uint8, kIPLSize> ipl) {
        m_sh2bus.LoadIPL(ipl);
    }

    void Step() {
        m_masterSH2.Step();
    }

    SH2 &MasterSH2() {
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
