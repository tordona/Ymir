#pragma once

#include "satemu/sh2/sh2.hpp"

namespace satemu {

class Saturn {
public:
    Saturn()
        : m_sh2bus(m_SMPC)
        , m_masterSH2(m_sh2bus, true) {
        Reset(true);
    }

    void Reset(bool hard) {
        m_sh2bus.Reset(hard);
        m_masterSH2.Reset(hard);
        m_SMPC.Reset(hard);
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
    SMPC m_SMPC;
};

} // namespace satemu
