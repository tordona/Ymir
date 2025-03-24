#pragma once

#include <app/shared_context.hpp>

namespace app::ui {

class SCUDebuggerWindow {
public:
    SCUDebuggerWindow(SharedContext &context);

    void Display();

    bool Open = false;

private:
    SharedContext &m_context;
    satemu::scu::SCU &m_scu;
};

} // namespace app::ui
