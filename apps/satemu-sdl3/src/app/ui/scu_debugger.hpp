#pragma once

#include <app/context.hpp>

namespace app {

class SCUDebugger {
public:
    SCUDebugger(Context &context);

    void Display();

    bool Open = false;

private:
    Context &m_context;
    satemu::scu::SCU &m_scu;
};

} // namespace app
