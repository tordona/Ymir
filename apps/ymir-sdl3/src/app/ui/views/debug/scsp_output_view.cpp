#include "scsp_output_view.hpp"

#include <app/ui/widgets/audio_widgets.hpp>

namespace app::ui {

SCSPOutputView::SCSPOutputView(SharedContext &context)
    : m_context(context)
    , m_tracer(context.tracers.SCSP) {}

void SCSPOutputView::Display(ImVec2 size) {
    const uint32 count = m_tracer.output.Count();
    std::vector<widgets::StereoSample> waveform{};
    waveform.resize(count);
    for (uint32 i = 0; i < count; ++i) {
        // The conversion to float is slightly assymetric, but unnoticeable in practice
        const auto sample = m_tracer.output.Read(i);
        waveform[i].left = static_cast<float>(sample.left) / 32768.0f;
        waveform[i].right = static_cast<float>(sample.right) / 32768.0f;
    }
    widgets::Oscilloscope(m_context, waveform, size);
}

} // namespace app::ui
