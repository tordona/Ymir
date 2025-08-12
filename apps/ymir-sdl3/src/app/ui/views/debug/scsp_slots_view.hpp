#pragma once

#include <app/shared_context.hpp>

namespace app::ui {

class SCSPSlotsView {
public:
    SCSPSlotsView(SharedContext &context);

    void Display();

private:
    SharedContext &m_context;
    ymir::scsp::SCSP &m_scsp;

    bool m_colorSlotsBySA = true;
};

} // namespace app::ui
