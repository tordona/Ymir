#include "memory_viewer.hpp"

namespace app {

uint32 MemoryViewer::s_index = 0;

MemoryViewer::MemoryViewer(SharedContext &context)
    : m_sharedCtx(context)
    , m_index(s_index++) {

    m_context.reset(new Context(context));

    // TODO: support 16-bit and 32-bit reads/writes
    m_context->memoryEditor.Open = false;
    m_context->memoryEditor.ReadFn = [](const ImU8 *mem, size_t off, void *user_data) {
        auto &ctx = *static_cast<const Context *>(user_data);
        return ctx.sharedCtx.saturn.mainBus.Peek<uint8>(off);
    };
    m_context->memoryEditor.WriteFn = [](ImU8 *mem, size_t off, ImU8 d, void *user_data) {
        auto &ctx = *static_cast<Context *>(user_data);
        ctx.sharedCtx.eventQueues.emulator.enqueue(EmuEvent::DebugWrite(off, d, ctx.enableSideEffects));
    };
    m_context->memoryEditor.UserData = m_context.get();
}

void MemoryViewer::Display() {
    if (!Open) {
        return;
    }

    if (m_requestFocus) {
        ImGui::SetNextWindowFocus();
        m_requestFocus = false;
    }

    MemoryEditor::Sizes sizes{};
    m_context->memoryEditor.CalcSizes(sizes, 0x8000000, 0x0);

    ImGui::SetNextWindowSizeConstraints(ImVec2(sizes.WindowWidth, 245), ImVec2(sizes.WindowWidth, FLT_MAX));
    if (ImGui::Begin(fmt::format("Memory viewer #{}", m_index + 1).c_str(), &Open, ImGuiWindowFlags_NoScrollbar)) {
        static int region = 0;
        static constexpr const char *regions[] = {"Global", "IPL ROM"};

        if (ImGui::BeginCombo("Region", regions[region],
                              ImGuiComboFlags_HeightLarge | ImGuiComboFlags_WidthFitPreview)) {
            for (int i = 0; i < std::size(regions); i++) {
                bool selected = i == region;
                if (ImGui::Selectable(regions[i], &selected)) {
                    region = i;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Checkbox("Enable side-effects", &m_context->enableSideEffects);
        ImGui::Separator();
        ImGui::PushFont(m_sharedCtx.fonts.monospaceMedium);
        m_context->memoryEditor.DrawContents(this, 0x8000000, 0x0);
        ImGui::PopFont();
    }
    ImGui::End();
}

void MemoryViewer::RequestFocus() {
    m_requestFocus = true;
}

} // namespace app
