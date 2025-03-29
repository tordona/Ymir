#pragma once

#include <app/shared_context.hpp>

#include <app/debug/sh2_tracer.hpp>

#include <satemu/hw/sh2/sh2_dmac.hpp>

namespace app::ui {

class SH2DMAControllerChannelTraceView {
public:
    SH2DMAControllerChannelTraceView(SharedContext &context, satemu::sh2::DMAChannel &channel, int index,
                                     SH2Tracer &tracer);

    void Display();

private:
    SharedContext &m_context;
    satemu::sh2::DMAChannel &m_channel;
    const int m_index;
    SH2Tracer &m_tracer;

    void DisplayStatistics();
    void DisplayTrace();
};

} // namespace app::ui
