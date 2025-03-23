#pragma once

#include <app/shared_context.hpp>

#include <imgui.h>

#include <imgui_memory_editor.h>

#include <memory>

namespace app {

class MemoryViewer {
public:
    MemoryViewer(SharedContext &context);

    void Display();
    void RequestFocus();

    bool Open = false;

private:
    static uint32 s_index;
    SharedContext &m_sharedCtx;
    uint32 m_index;
    bool m_requestFocus = false;

    struct Context {
        Context(SharedContext &sharedCtx)
            : sharedCtx(sharedCtx) {}

        SharedContext &sharedCtx;
        MemoryEditor memoryEditor;
        bool enableSideEffects = false;
    };
    std::unique_ptr<Context> m_context;
};

} // namespace app
