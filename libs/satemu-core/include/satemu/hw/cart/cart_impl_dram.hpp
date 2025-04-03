#pragma once

#include "cart_base.hpp"

#include <satemu/util/data_ops.hpp>
#include <satemu/util/size_ops.hpp>

#include <array>

namespace satemu::cart {

// Base class for DRAM cartridges.
template <uint8 id, size_t size, CartType type>
class BaseDRAMCartridge : public BaseCartridge {
    static_assert(size != 0 && (size & (size - 1)) == 0, "size must be a power of two");

public:
    BaseDRAMCartridge()
        : BaseCartridge(id, type) {
        Reset(true);
    }

    void Reset(bool hard) final {
        m_ram.fill(0);
    }

protected:
    std::array<uint8, size> m_ram;
};

// ---------------------------------------------------------------------------------------------------------------------

// 8 Mbit (1 MiB) DRAM cartridge.
// Lower 512 KiB mapped to 0x240'0000..0x24F'FFFF, mirrored twice
// Upper 512 KiB mapped to 0x260'0000..0x26F'FFFF, mirrored twice
class DRAM8MbitCartridge final : public BaseDRAMCartridge<0x5A, 1_MiB, CartType::DRAM8Mbit> {
public:
    uint8 ReadByte(uint32 address) const final {
        switch (address >> 20) {
        case 0x24: return m_ram[address & 0x7FFFF];
        case 0x26: return m_ram[(address & 0x7FFFF) | 0x80000];
        default: return 0xFFu;
        }
    }

    uint16 ReadWord(uint32 address) const final {
        switch (address >> 20) {
        case 0x24: return util::ReadBE<uint16>(&m_ram[address & 0x7FFFF]);
        case 0x26: return util::ReadBE<uint16>(&m_ram[(address & 0x7FFFF) | 0x80000]);
        default: return 0xFFFFu;
        }
    }

    void WriteByte(uint32 address, uint8 value) final {
        switch (address >> 20) {
        case 0x24: m_ram[address & 0x7FFFF] = value; break;
        case 0x26: m_ram[(address & 0x7FFFF) | 0x80000] = value; break;
        }
    }

    void WriteWord(uint32 address, uint16 value) final {
        switch (address >> 20) {
        case 0x24: util::WriteBE<uint16>(&m_ram[address & 0x7FFFF], value); break;
        case 0x26: util::WriteBE<uint16>(&m_ram[(address & 0x7FFFF) | 0x80000], value); break;
        }
    }

    uint8 PeekByte(uint32 address) const final {
        return ReadByte(address);
    }
    uint16 PeekWord(uint32 address) const final {
        return ReadWord(address);
    }

    void PokeByte(uint32 address, uint8 value) final {
        WriteByte(address, value);
    }
    void PokeWord(uint32 address, uint16 value) final {
        WriteWord(address, value);
    }
};

// ---------------------------------------------------------------------------------------------------------------------

// 32 Mbit (4 MiB) DRAM cartridge.
// Mapped to 0x240'0000..0x27F'FFFF
class DRAM32MbitCartridge final : public BaseDRAMCartridge<0x5C, 4_MiB, CartType::DRAM32Mbit> {
public:
    uint8 ReadByte(uint32 address) const final {
        switch (address >> 20) {
        case 0x24 ... 0x27: return m_ram[address & 0x3FFFFF];
        default: return 0xFFu;
        }
    }

    uint16 ReadWord(uint32 address) const final {
        switch (address >> 20) {
        case 0x24 ... 0x27: return util::ReadBE<uint16>(&m_ram[address & 0x3FFFFF]);
        default: return 0xFFFFu;
        }
    }

    void WriteByte(uint32 address, uint8 value) final {
        switch (address >> 20) {
        case 0x24 ... 0x27: m_ram[address & 0x3FFFFF] = value; break;
        }
    }

    void WriteWord(uint32 address, uint16 value) final {
        switch (address >> 20) {
        case 0x24 ... 0x27: util::WriteBE<uint16>(&m_ram[address & 0x3FFFFF], value); break;
        }
    }

    uint8 PeekByte(uint32 address) const final {
        return ReadByte(address);
    }
    uint16 PeekWord(uint32 address) const final {
        return ReadWord(address);
    }

    void PokeByte(uint32 address, uint8 value) final {
        WriteByte(address, value);
    }
    void PokeWord(uint32 address, uint16 value) final {
        WriteWord(address, value);
    }
};

} // namespace satemu::cart
