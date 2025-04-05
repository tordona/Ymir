#pragma once

#include <satemu/core/types.hpp>

namespace satemu::cart {

enum class CartType { None, BackupMemory, DRAM8Mbit, DRAM32Mbit };

class BaseCartridge {
public:
    BaseCartridge(uint8 id, CartType type)
        : m_id(id)
        , m_type(type) {}

    virtual ~BaseCartridge() = default;

    virtual void Reset(bool hard) {}

    uint8 GetID() const {
        return m_id;
    }

    CartType GetType() const {
        return m_type;
    }

    virtual uint8 ReadByte(uint32 address) const = 0;
    virtual uint16 ReadWord(uint32 address) const = 0;

    virtual void WriteByte(uint32 address, uint8 value) = 0;
    virtual void WriteWord(uint32 address, uint16 value) = 0;

    virtual uint8 PeekByte(uint32 address) const = 0;
    virtual uint16 PeekWord(uint32 address) const = 0;

    virtual void PokeByte(uint32 address, uint8 value) = 0;
    virtual void PokeWord(uint32 address, uint16 value) = 0;

protected:
    void ChangeID(uint8 id) {
        m_id = id;
    }

private:
    uint8 m_id;
    CartType m_type;
};

} // namespace satemu::cart
