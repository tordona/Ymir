#pragma once

#include <ymir/debug/scsp_tracer_base.hpp>

#include <util/ring_buffer.hpp>

#include <array>

namespace app {

struct SCSPTracer final : ymir::debug::ISCSPTracer {
    struct Sample {
        sint16 left;
        sint16 right;
    };
    util::RingBuffer<Sample, 1024> output;

private:
    // -------------------------------------------------------------------------
    // ISCSPTracer implementation

    void Sample(sint16 left, sint16 right) final;
};

} // namespace app
