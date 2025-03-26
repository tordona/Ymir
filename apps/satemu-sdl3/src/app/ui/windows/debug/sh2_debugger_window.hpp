#pragma once

#include <app/ui/window_base.hpp>

#include <app/ui/views/debug/sh2_disassembly_view.hpp>
#include <app/ui/views/debug/sh2_registers_view.hpp>

namespace app::ui {

class SH2DebuggerWindow : public WindowBase {
public:
    SH2DebuggerWindow(SharedContext &context, bool master);

protected:
    void DrawContents() override;

private:
    SH2RegistersView m_regsView;
    SH2DisassemblyView m_disasmView;
};

} // namespace app::ui
