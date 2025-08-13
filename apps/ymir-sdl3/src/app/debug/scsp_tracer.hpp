#pragma once

#include <ymir/debug/scsp_tracer_base.hpp>

#include <util/ring_buffer.hpp>

namespace app {

struct SCSPTracer final : ymir::debug::ISCSPTracer {

private:
    // -------------------------------------------------------------------------
    // ISCSPTracer implementation

    // TODO: void SlotSample(uint32 index, sint16 left, sint16 right) final;
};

} // namespace app
