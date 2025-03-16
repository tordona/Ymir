#pragma once

#include <satemu/debug/scu_tracer.hpp>

namespace app {

struct SCUTracer final : public satemu::debug::ISCUTracer {
    struct InterruptInfo {
        uint8 index;
        uint8 level; // 0xFF == acknowledge
    };

    void RaiseInterrupt(uint8 index, uint8 level) final;
    void AcknowledgeInterrupt(uint8 index) final;

    std::array<InterruptInfo, 16> interrupts;
    size_t interruptsPos = 0;
    size_t interruptsCount = 0;

private:
    void PushInterrupt(InterruptInfo info);
};

} // namespace app
