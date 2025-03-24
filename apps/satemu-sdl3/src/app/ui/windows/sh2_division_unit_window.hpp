#pragma once

#include <app/shared_context.hpp>

#include <app/ui/views/sh2_division_unit_traces_view.hpp>

namespace app::ui {

class SH2DivisionUnitWindow {
public:
    SH2DivisionUnitWindow(SharedContext &context, bool master);

    void Display();

    bool Open = false;

private:
    SharedContext &m_context;
    bool m_master;

    SH2DivisionUnitTracesView m_divisionUnitTracesView;
};

} // namespace app::ui
