#pragma once

#include <app/context.hpp>

namespace app {

class SH2Debugger {
public:
    SH2Debugger(Context &context, bool master);

    void Display();

    bool Open = false;

private:
    Context &m_context;
    bool m_master;
    satemu::sh2::SH2 &m_sh2;
};

} // namespace app
