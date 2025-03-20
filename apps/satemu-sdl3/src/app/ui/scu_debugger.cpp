#include "scu_debugger.hpp"

#include <imgui.h>

namespace app {

SCUDebugger::SCUDebugger(SharedContext &context)
    : m_context(context)
    , m_scu(context.saturn.SCU) {}

void SCUDebugger::Display() {
    if (ImGui::Begin("SCU")) {
        ImGui::TextUnformatted("Interrupts");
        ImGui::Text("%08X mask", m_scu.GetInterruptMask().u32);
        ImGui::Text("%08X status", m_scu.GetInterruptStatus().u32);

        /*if (debugTrace) {
            auto &tracer = m_scuTracer;
            for (size_t i = 0; i < tracer.interruptsCount; i++) {
                size_t pos = (tracer.interruptsPos - tracer.interruptsCount + i) % tracer.interrupts.size();
                constexpr const char *kNames[] = {"VBlank IN",
                                                  "VBlank OUT",
                                                  "HBlank IN",
                                                  "Timer 0",
                                                  "Timer 1",
                                                  "DSP End",
                                                  "Sound Request",
                                                  "System Manager",
                                                  "PAD Interrupt",
                                                  "Level 2 DMA End",
                                                  "Level 1 DMA End",
                                                  "Level 0 DMA End",
                                                  "DMA-illegal",
                                                  "Sprite Draw End",
                                                  "(0E)",
                                                  "(0F)",
                                                  "External 0",
                                                  "External 1",
                                                  "External 2",
                                                  "External 3",
                                                  "External 4",
                                                  "External 5",
                                                  "External 6",
                                                  "External 7",
                                                  "External 8",
                                                  "External 9",
                                                  "External A",
                                                  "External B",
                                                  "External C",
                                                  "External D",
                                                  "External E",
                                                  "External F"};

                const auto &intr = tracer.interrupts[pos];
                if (intr.level == 0xFF) {
                    drawText(x, y + 50 + i * 10, fmt::format("{:15s}  ack", kNames[intr.index], intr.level));
                } else {
                    drawText(x, y + 50 + i * 10, fmt::format("{:15s}  {:02X}", kNames[intr.index], intr.level));
                }
            }
        } else {
            ImGui::TextUnformatted("(trace unavailable)");
        }*/
    }
    ImGui::End();
}

} // namespace app
