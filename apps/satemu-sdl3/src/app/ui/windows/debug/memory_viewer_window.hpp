#pragma once

#include <app/shared_context.hpp>

#include <app/ui/window_base.hpp>

#include <imgui_memory_editor.h>

#include <fmt/format.h>

#include <memory>
#include <span>

namespace app::ui {

class MemoryViewerWindow : public WindowBase {
public:
    MemoryViewerWindow(SharedContext &context);

    uint32 Index() const {
        return m_index;
    }

protected:
    void PrepareWindow() override;
    void DrawContents() override;

private:
    struct Region;
    struct RegionGroup;
    struct RegionDefs; // defined in memory_viewer_region_defs.hpp

    struct Context {
        Context(SharedContext &sharedCtx)
            : sharedCtx(sharedCtx) {}

        SharedContext &sharedCtx;
        MemoryEditor memoryEditor;
        bool enableSideEffects = false;
        const Region *selectedRegion = nullptr;
    };

    struct Region {
        using ReadFn = ImU8 (*)(const ImU8 *mem, size_t off, void *user_data);
        using WriteFn = void (*)(ImU8 *mem, size_t off, ImU8 d, void *user_data);
        using BgColorFn = ImU32 (*)(const ImU8 *mem, size_t off, void *user_data);
        using ParamsFn = void (*)(Context *ctx);

        const char *name;
        const char *addressBlockName;
        uint32 baseAddress;
        uint32 size;
        ReadFn readFn;
        WriteFn writeFn;
        BgColorFn bgColorFn;
        ParamsFn paramsFn;

        std::string ToString() const {
            return fmt::format("[{}:{:08X}..{:08X}] {}", addressBlockName, baseAddress, baseAddress + size - 1, name);
        }
    };

    struct RegionGroup {
        const char *name;
        std::span<const Region> regions;
    };

    static uint32 s_index;
    uint32 m_index;

    std::unique_ptr<Context> m_memViewCtx;
};

} // namespace app::ui
