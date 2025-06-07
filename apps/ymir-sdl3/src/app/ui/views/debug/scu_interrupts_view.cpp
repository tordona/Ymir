#include "scu_interrupts_view.hpp"

namespace app::ui {

SCUInterruptsView::SCUInterruptsView(SharedContext &context)
    : m_context(context)
    , m_scu(context.saturn.SCU) {}

void SCUInterruptsView::Display() {
    if (ImGui::BeginTable("main", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthFixed, 265 * m_context.displayScale);
        ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        if (ImGui::TableNextColumn()) {
            DisplayInternalInterrupts();
        }
        if (ImGui::TableNextColumn()) {
            DisplayExternalInterrupts();
        }

        ImGui::EndTable();
    }

    auto &probe = m_scu.GetProbe();
    const uint8 pendingIntrLevel = probe.GetPendingInterruptLevel();
    if (pendingIntrLevel > 0) {
        const uint8 index = probe.GetPendingInterruptIndex();
        if (index < 16) {
            static constexpr const char *kNames[] = {
                "VDP2 VBlank IN",     "VDP2 VBlank OUT",      "VDP2 HBlank IN",      "SCU Timer 0",
                "SCU Timer 1",        "SCU DSP End",          "SCSP Sound Request",  "SMPC System Manager",
                "SMPC PAD Interrupt", "SCU Level 2 DMA End",  "SCU Level 1 DMA End", "SCU Level 0 DMA End",
                "SCU DMA-illegal",    "VDP1 Sprite Draw End", "Unknown (14)",        "Unknown (15)",
            };
            ImGui::Text("Pending interrupt: %s, level %X", kNames[index], pendingIntrLevel);
        } else {
            ImGui::Text("Pending interrupt: External %X, level %X", index, pendingIntrLevel - 16);
        }
    } else {
        ImGui::TextDisabled("No pending interrupt");
    }
}

void SCUInterruptsView::DisplayInternalInterrupts() {
    auto &probe = m_scu.GetProbe();
    auto &intrStatus = probe.GetInterruptStatus();
    auto &intrMask = probe.GetInterruptMask();

    bool flag = intrMask.ABus_ExtIntrs;
    if (ImGui::Checkbox("A-Bus external interrupt mask", &flag)) {
        intrMask.ABus_ExtIntrs = flag;
    }
    ImGui::Checkbox("A-Bus interrupt acknowledge", &probe.GetABusInterruptAcknowledge());

    ImGui::Separator();

    ImGui::PushFont(m_context.fonts.sansSerif.medium.bold);
    ImGui::TextUnformatted("Internal");
    ImGui::PopFont();

    if (ImGui::BeginTable("internal_intrs", 6, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("St");
        ImGui::TableSetupColumn("Msk");
        ImGui::TableSetupColumn("Source");
        ImGui::TableSetupColumn("Event");
        ImGui::TableSetupColumn("Vec");
        ImGui::TableSetupColumn("Lv");
        ImGui::TableHeadersRow();

        auto drawRow = [&](uint32 bit, std::string_view source, std::string_view name, uint8 vector, uint8 level) {
            const uint32 bitVal = 1u << bit;
            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                bool flag = intrStatus.u32 & bitVal;
                if (ImGui::Checkbox(fmt::format("##sts_{}_{}", source, name).c_str(), &flag)) {
                    intrStatus.u32 &= ~bitVal;
                    intrStatus.u32 |= static_cast<uint32>(flag) << bit;
                }
            }
            if (ImGui::TableNextColumn()) {
                bool flag = intrMask.u32 & bitVal;
                if (ImGui::Checkbox(fmt::format("##msk_{}_{}", source, name).c_str(), &flag)) {
                    intrMask.u32 &= ~bitVal;
                    intrMask.u32 |= static_cast<uint32>(flag) << bit;
                }
            }
            if (ImGui::TableNextColumn()) {
                ImGui::TextUnformatted(source.data());
            }
            if (ImGui::TableNextColumn()) {
                ImGui::TextUnformatted(name.data());
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                ImGui::Text("%X", vector);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                ImGui::Text("%X", level);
                ImGui::PopFont();
            }
        };

        drawRow(0, "VDP2", "VBlank IN", 0x40, 0xF);
        drawRow(1, "VDP2", "VBlank OUT", 0x41, 0xE);
        drawRow(2, "VDP2", "HBlank IN", 0x42, 0xD);
        drawRow(3, "SCU", "Timer 0", 0x43, 0xC);
        drawRow(4, "SCU", "Timer 1", 0x44, 0xB);
        drawRow(5, "SCU", "DSP End", 0x45, 0xA);
        drawRow(6, "SCSP", "Sound Request", 0x46, 0x9);
        drawRow(7, "SMPC", "System Manager", 0x47, 0x8);
        drawRow(8, "SMPC", "PAD Interrupt", 0x48, 0x8);
        drawRow(9, "SCU", "Level 2 DMA End", 0x49, 0x6);
        drawRow(10, "SCU", "Level 1 DMA End", 0x4A, 0x6);
        drawRow(11, "SCU", "Level 0 DMA End", 0x4B, 0x5);
        drawRow(12, "SCU", "DMA-illegal", 0x4C, 0x3);
        drawRow(13, "VDP1", "Sprite Draw End", 0x4D, 0x2);

        ImGui::EndTable();
    }
}

void SCUInterruptsView::DisplayExternalInterrupts() {
    ImGui::PushFont(m_context.fonts.sansSerif.medium.bold);
    ImGui::TextUnformatted("External (A-Bus)");
    ImGui::PopFont();

    if (ImGui::BeginTable("external_intrs", 4, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("St");
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn("Vec");
        ImGui::TableSetupColumn("Lv");
        ImGui::TableHeadersRow();

        auto &probe = m_scu.GetProbe();
        auto &intrStatus = probe.GetInterruptStatus();

        for (uint32 i = 0; i < 16; i++) {
            const uint32 bit = i + 16;
            const uint32 bitVal = 1u << bit;
            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                bool flag = intrStatus.u32 & bitVal;
                if (ImGui::Checkbox(fmt::format("##sts_ext_{}", i).c_str(), &flag)) {
                    intrStatus.u32 &= ~bitVal;
                    intrStatus.u32 |= static_cast<uint32>(flag) << bit;
                }
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                ImGui::Text("%X", i);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                ImGui::Text("%X", 0x50 + i);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                ImGui::Text("%X", (i < 4) ? 7 : (i < 8) ? 4 : 1);
                ImGui::PopFont();
            }
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui
