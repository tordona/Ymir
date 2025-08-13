#pragma once

#include <ymir/debug/scsp_tracer_base.hpp>

#include <util/ring_buffer.hpp>

#include <array>

namespace app {

struct SCSPTracer final : ymir::debug::ISCSPTracer {
    std::array<util::RingBuffer<sint16, 2048>, 32> slotOutputs;

private:
    // -------------------------------------------------------------------------
    // ISCSPTracer implementation

    void SlotSample(uint32 index, sint16 output) final;
};

} // namespace app
