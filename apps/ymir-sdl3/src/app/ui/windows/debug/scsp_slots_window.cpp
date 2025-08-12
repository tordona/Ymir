#include "scsp_slots_window.hpp"

#include <app/ui/widgets/audio_widgets.hpp>

namespace app::ui {

SCSPSlotsWindow::SCSPSlotsWindow(SharedContext &context)
    : SCSPWindowBase(context)
    , m_slotsView(context) {

    m_windowConfig.name = "SCSP slots";
    m_windowConfig.flags = ImGuiWindowFlags_AlwaysAutoResize;
}

void SCSPSlotsWindow::DrawContents() {
    m_slotsView.Display();

    // TODO: move this to a view and into a separate window

    const uint32 count = m_tracer.output.Count();
    std::vector<widgets::StereoSample> waveform{};
    waveform.resize(count);
    for (uint32 i = 0; i < count; ++i) {
        // The conversion to float is slightly assymetric, but unnoticeable in practice
        const auto sample = m_tracer.output.Read(i);
        waveform[i].left = static_cast<float>(sample.left) / 32768.0f;
        waveform[i].right = static_cast<float>(sample.right) / 32768.0f;
    }
    widgets::Oscilloscope(m_context, waveform);
}

} // namespace app::ui
