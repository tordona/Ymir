#pragma once

#include <app/shared_context.hpp>

#include <satemu/hw/sh2/sh2.hpp>

namespace app {

class SH2InterruptsView {
public:
    SH2InterruptsView(SharedContext &context, satemu::sh2::SH2 &sh2);

    void Display();

private:
    SharedContext &m_context;
    satemu::sh2::SH2 &m_sh2;

    uint8 m_extIntrVector = 0x0;
    uint8 m_extIntrLevel = 0x0;
};

} // namespace app
