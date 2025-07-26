#pragma once

#include <app/shared_context.hpp>

#include <ymir/hw/sh2/sh2.hpp>

namespace app::ui {

class SH2BreakpointsView {
public:
    SH2BreakpointsView(SharedContext &context, ymir::sh2::SH2 &sh2);

    void Display();

private:
    SharedContext &m_context;
    ymir::sh2::SH2 &m_sh2;

    uint32 m_address = 0x00000000;
};

} // namespace app::ui
