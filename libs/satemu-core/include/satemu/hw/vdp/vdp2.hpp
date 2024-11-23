#pragma once

#include "vdp2_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <fmt/format.h>

namespace satemu {

class VDP2 {
public:
    VDP2();

    void Reset(bool hard);

    template <mem_access_type T>
    T ReadVRAM(uint32 address) {
        fmt::println("unhandled {}-bit VDP2 VRAM read from {:02X}", sizeof(T) * 8, address);
        return 0;
    }

    template <mem_access_type T>
    void WriteVRAM(uint32 address, T value) {
        fmt::println("unhandled {}-bit VDP2 VRAM write to {:02X} = {:X}", sizeof(T) * 8, address, value);
    }

    template <mem_access_type T>
    T ReadCRAM(uint32 address) {
        fmt::println("unhandled {}-bit VDP2 CRAM read from {:02X}", sizeof(T) * 8, address);
        return 0;
    }

    template <mem_access_type T>
    void WriteCRAM(uint32 address, T value) {
        fmt::println("unhandled {}-bit VDP2 CRAM write to {:02X} = {:X}", sizeof(T) * 8, address, value);
    }

    template <mem_access_type T>
    T ReadReg(uint32 address) {
        fmt::println("unhandled {}-bit VDP2 register read from {:02X}", sizeof(T) * 8, address);
        return 0;
    }

    template <mem_access_type T>
    void WriteReg(uint32 address, T value) {
        fmt::println("unhandled {}-bit VDP2 register write to {:02X} = {:X}", sizeof(T) * 8, address, value);
    }

private:
};

} // namespace satemu
