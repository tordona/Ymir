#pragma once

#include "settings_view_base.hpp"

#include <app/ui/widgets/input_widgets.hpp>
#include <app/ui/widgets/unbound_actions_widget.hpp>

namespace app::ui {

class ArcadeRacerBindsView : public SettingsViewBase {
public:
    ArcadeRacerBindsView(SharedContext &context);

    void Display(Settings::Input::Port::ArcadeRacerBinds &binds, uint32 portIndex);

private:
    widgets::InputCaptureWidget m_inputCaptureWidget;
    widgets::UnboundActionsWidget m_unboundActionsWidget;
};

} // namespace app::ui
