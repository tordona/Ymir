#pragma once

#include <app/shared_context.hpp>

#include <satemu/hw/sh2/sh2.hpp>

namespace app::ui {

class SH2WatchdogTimerView {
public:
    SH2WatchdogTimerView(SharedContext &context, satemu::sh2::SH2 &sh2);

    void Display();

private:
    SharedContext &m_context;
    satemu::sh2::SH2 &m_sh2;
};

} // namespace app::ui
