#pragma once

#include "cart_base.hpp"

namespace satemu::cart {

class NoCartridge final : public BaseCartridge {
public:
    NoCartridge()
        : BaseCartridge(0xFFu) {}

    bool IsInitialized() const final {
        return false;
    }

    uint8 ReadByte(uint32 address) const final {
        return 0xFFu;
    }
    uint16 ReadWord(uint32 address) const final {
        return 0xFFFFu;
    }

    void WriteByte(uint32 address, uint8 value) final {}
    void WriteWord(uint32 address, uint16 value) final {}
};

} // namespace satemu::cart
