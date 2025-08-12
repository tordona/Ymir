#pragma once

#include <app/shared_context.hpp>

#include <app/debug/scsp_tracer.hpp>

namespace app::ui {

class SCSPOutputView {
public:
    SCSPOutputView(SharedContext &context);

    void Display(ImVec2 size = {0, 0});

private:
    SharedContext &m_context;
    SCSPTracer &m_tracer;
};

} // namespace app::ui
