#pragma once

#include "vdp1_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <satemu/util/data_ops.hpp>
#include <satemu/util/size_ops.hpp>

#include <fmt/format.h>

#include <array>

namespace satemu {

class VDP1 {
public:
    VDP1();

    void Reset(bool hard);

    template <mem_access_type T>
    T ReadVRAM(uint32 address) {
        return util::ReadBE<T>(&m_VRAM[address & 0x7FFFF]);
    }

    template <mem_access_type T>
    void WriteVRAM(uint32 address, T value) {
        util::WriteBE<T>(&m_VRAM[address & 0x7FFFF], value);
    }

    template <mem_access_type T>
    T ReadFB(uint32 address) {
        return util::ReadBE<T>(&m_framebuffers[m_drawFB][address & 0x3FFFF]);
    }

    template <mem_access_type T>
    void WriteFB(uint32 address, T value) {
        util::WriteBE<T>(&m_framebuffers[m_drawFB][address & 0x3FFFF], value);
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
    std::array<uint8, kVDP1VRAMSize> m_VRAM;
    std::array<std::array<uint8, kFramebufferRAMSize>, 2> m_framebuffers;
    size_t m_drawFB;
};

} // namespace satemu
