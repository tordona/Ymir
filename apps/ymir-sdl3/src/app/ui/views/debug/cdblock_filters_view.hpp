#pragma once

#include <app/shared_context.hpp>

namespace app::ui {

class CDBlockFiltersView {
public:
    CDBlockFiltersView(SharedContext &context);

    void Display();

private:
    SharedContext &m_context;
    ymir::cdblock::CDBlock::Probe &m_probe;
};

} // namespace app::ui
