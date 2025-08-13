#pragma once

#include <app/audio_system.hpp>
#include <app/shared_context.hpp>

#include <app/ui/widgets/audio_widgets.hpp>

#include <app/debug/scsp_tracer.hpp>

namespace app::ui {

class SCSPOutputView {
public:
    SCSPOutputView(SharedContext &context);

    void Display(ImVec2 size = {0, 0});

private:
    SharedContext &m_context;
    SCSPTracer &m_tracer;

    std::array<Sample, 2048> m_audioBuffer;
    std::array<widgets::StereoSample, 2048> m_waveform;
};

} // namespace app::ui
