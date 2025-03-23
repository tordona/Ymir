#pragma once

#include <app/shared_context.hpp>

#include "views/sh2_interrupts_view.hpp"

namespace app {

class SH2Interrupts {
public:
    SH2Interrupts(SharedContext &context, bool master);

    void Display();

    bool Open = false;

private:
    SharedContext &m_context;
    bool m_master;
    satemu::sh2::SH2 &m_sh2;

    SH2InterruptsView m_intrView;
};

} // namespace app
