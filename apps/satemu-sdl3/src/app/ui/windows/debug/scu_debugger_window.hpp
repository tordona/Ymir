#pragma once

#include <app/ui/window_base.hpp>

namespace app::ui {

class SCUDebuggerWindow : public WindowBase {
public:
    SCUDebuggerWindow(SharedContext &context);

protected:
    void DrawContents() override;

private:
    satemu::scu::SCU &m_scu;
};

} // namespace app::ui
