#pragma once

#include <app/shared_context.hpp>

#include <app/ui/views/sh2_disassembly_view.hpp>
#include <app/ui/views/sh2_registers_view.hpp>

namespace app::ui {

class SH2DebuggerWindow {
public:
    SH2DebuggerWindow(SharedContext &context, bool master);

    void Display();

    bool Open = false;

private:
    SharedContext &m_context;
    bool m_master;
    satemu::sh2::SH2 &m_sh2;

    SH2RegistersView m_regsView;
    SH2DisassemblyView m_disasmView;
};

} // namespace app::ui
