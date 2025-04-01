#include "emu_debug_event_factory.hpp"

#include "emu_event_factory.hpp"

#include <app/shared_context.hpp>

namespace app::events::emu::debug {

EmuEvent ExecuteSH2Division(bool master, bool div64) {
    if (div64) {
        return RunFunction([=](SharedContext &ctx) {
            auto &sh2 = master ? ctx.saturn.masterSH2 : ctx.saturn.slaveSH2;
            sh2.GetProbe().ExecuteDiv64();
        });
    } else {
        return RunFunction([=](SharedContext &ctx) {
            auto &sh2 = master ? ctx.saturn.masterSH2 : ctx.saturn.slaveSH2;
            sh2.GetProbe().ExecuteDiv32();
        });
    }
}

EmuEvent WriteMainMemory(uint32 address, uint8 value, bool enableSideEffects) {
    if (enableSideEffects) {
        return RunFunction([=](SharedContext &ctx) { ctx.saturn.mainBus.Write<uint8>(address, value); });
    } else {
        return RunFunction([=](SharedContext &ctx) { ctx.saturn.mainBus.Poke<uint8>(address, value); });
    }
}

EmuEvent WriteSH2Memory(uint32 address, uint8 value, bool enableSideEffects, bool master, bool bypassCache) {
    if (enableSideEffects) {
        return RunFunction([=](SharedContext &ctx) {
            auto &sh2 = master ? ctx.saturn.masterSH2 : ctx.saturn.slaveSH2;
            sh2.GetProbe().MemWriteByte(address, value, bypassCache);
        });
    } else {
        return RunFunction([=](SharedContext &ctx) {
            auto &sh2 = master ? ctx.saturn.masterSH2 : ctx.saturn.slaveSH2;
            sh2.GetProbe().MemPokeByte(address, value, bypassCache);
        });
    }
}

} // namespace app::events::emu::debug
