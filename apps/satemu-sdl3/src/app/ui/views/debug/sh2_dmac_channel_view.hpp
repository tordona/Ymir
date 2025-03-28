#pragma once

#include <app/shared_context.hpp>

#include <satemu/hw/sh2/sh2_dmac.hpp>

namespace app::ui {

class SH2DMAControllerChannelView {
public:
    SH2DMAControllerChannelView(SharedContext &context, satemu::sh2::DMAChannel &channel, int index);

    void Display();

private:
    SharedContext &m_context;
    satemu::sh2::DMAChannel &m_channel;
    const int m_index;
};

} // namespace app::ui
