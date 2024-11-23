#pragma once

#include "vdp2_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <satemu/util/data_ops.hpp>

#include <fmt/format.h>

#include <array>

namespace satemu {

class VDP2 {
public:
    VDP2();

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
    T ReadCRAM(uint32 address) {
        return util::ReadBE<T>(&m_CRAM[address & 0xFFF]);
    }

    template <mem_access_type T>
    void WriteCRAM(uint32 address, T value) {
        util::WriteBE<T>(&m_CRAM[address & 0xFFF], value);
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
    std::array<uint8, kVDP2VRAMSize> m_VRAM; // 4x 128 KiB banks: A0, A1, B0, B1
    std::array<uint8, kCRAMSize> m_CRAM;
};

} // namespace satemu
