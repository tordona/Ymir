#pragma once

#include <app/shared_context.hpp>

#include <satemu/hw/sh2/sh2.hpp>

namespace app {

class SH2RegistersView {
public:
    SH2RegistersView(SharedContext &context, satemu::sh2::SH2 &sh2, bool master);

    void Display();

private:
    SharedContext &m_context;
    satemu::sh2::SH2 &m_sh2;
    bool m_master;
};

} // namespace app
