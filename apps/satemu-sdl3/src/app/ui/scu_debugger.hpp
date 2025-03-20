#pragma once

#include <app/shared_context.hpp>

namespace app {

class SCUDebugger {
public:
    SCUDebugger(SharedContext &context);

    void Display();

    bool Open = false;

private:
    SharedContext &m_context;
    satemu::scu::SCU &m_scu;
};

} // namespace app
