#pragma once

#include <satemu/debug/scu_tracer_base.hpp>

#include <util/ring_buffer.hpp>

#include <string>
#include <string_view>

namespace app {

struct SCUTracer final : satemu::debug::ISCUTracer {
    std::string_view GetDebugMessageBuffer() const {
        return m_debugMessageBuffer;
    }

    void ClearDebugMessages() {
        debugMessages.Clear();
        m_debugMessageBuffer.clear();
    }

    struct InterruptInfo {
        uint8 index;
        uint8 level; // 0xFF == acknowledge
    };

    util::RingBuffer<InterruptInfo, 1024> interrupts;
    util::RingBuffer<std::string, 1024> debugMessages;

private:
    void PushInterrupt(InterruptInfo info);

    std::string m_debugMessageBuffer;

    // -------------------------------------------------------------------------
    // ISCUTracer implementation

    void RaiseInterrupt(uint8 index, uint8 level) final;
    void AcknowledgeInterrupt(uint8 index) final;

    void DebugPortWrite(uint8 ch) final;

    void BeginDSPDMA(bool toD0, uint32 addrD0, uint8 addrDSP, uint8 count, uint8 addrInc, bool hold, uint8 pc) final;
    void DSPDMATransfer(uint32 addrD0, uint8 offsetDSP, uint32 value) final;
    void EndDSPDMA() final;
};

} // namespace app
