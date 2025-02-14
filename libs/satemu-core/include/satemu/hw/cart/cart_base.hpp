#pragma once

#include <satemu/core/types.hpp>

namespace satemu::cart {

class BaseCartridge {
public:
    BaseCartridge(uint8 id)
        : m_id(id) {}

    virtual ~BaseCartridge() = default;

    virtual bool IsInitialized() const = 0;

    virtual void Reset(bool hard) {}

    uint8 GetID() const {
        return m_id;
    }

    virtual uint8 ReadByte(uint32 address) const = 0;
    virtual uint16 ReadWord(uint32 address) const = 0;

    virtual void WriteByte(uint32 address, uint8 value) = 0;
    virtual void WriteWord(uint32 address, uint16 value) = 0;

private:
    uint8 m_id;
};

} // namespace satemu::cart
