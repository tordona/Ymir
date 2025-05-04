#pragma once

#include "settings_view_base.hpp"

#include <app/ui/widgets/input_widgets.hpp>
#include <app/ui/widgets/unbound_actions_widget.hpp>

namespace app::ui {

class StandardPadBindsView : public SettingsViewBase {
public:
    StandardPadBindsView(SharedContext &context);

    void Display(Settings::Input::Port::StandardPadBinds &binds, void *context);

private:
    widgets::InputCaptureWidget m_inputCaptureWidget;
    widgets::UnboundActionsWidget m_unboundActionsWidget;
};

} // namespace app::ui
