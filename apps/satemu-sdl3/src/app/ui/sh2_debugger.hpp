#pragma once

#include <app/shared_context.hpp>

namespace app {

class SH2Debugger {
public:
    SH2Debugger(SharedContext &context, bool master);

    void Display();

    bool Open = false;

private:
    SharedContext &m_context;
    bool m_master;
    satemu::sh2::SH2 &m_sh2;
};

} // namespace app
