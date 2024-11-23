#pragma once

#include "vdp1_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <fmt/format.h>

namespace satemu {

class VDP1 {
public:
    VDP1();

    void Reset(bool hard);

    template <mem_access_type T>
    T ReadVRAM(uint32 address) {
        fmt::println("unhandled {}-bit VDP1 VRAM read from {:02X}", sizeof(T) * 8, address);
        return 0;
    }

    template <mem_access_type T>
    void WriteVRAM(uint32 address, T value) {
        fmt::println("unhandled {}-bit VDP1 VRAM write to {:02X} = {:X}", sizeof(T) * 8, address, value);
    }

    template <mem_access_type T>
    T ReadFB(uint32 address) {
        fmt::println("unhandled {}-bit VDP1 framebuffer read from {:02X}", sizeof(T) * 8, address);
        return 0;
    }

    template <mem_access_type T>
    void WriteFB(uint32 address, T value) {
        fmt::println("unhandled {}-bit VDP1 framebuffer write to {:02X} = {:X}", sizeof(T) * 8, address, value);
    }

    template <mem_access_type T>
    T ReadReg(uint32 address) {
        fmt::println("unhandled {}-bit VDP1 register read from {:02X}", sizeof(T) * 8, address);
        return 0;
    }

    template <mem_access_type T>
    void WriteReg(uint32 address, T value) {
        fmt::println("unhandled {}-bit VDP1 register write to {:02X} = {:X}", sizeof(T) * 8, address, value);
    }

private:
};

} // namespace satemu
