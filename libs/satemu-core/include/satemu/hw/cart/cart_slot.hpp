#pragma once

#include "cart_base.hpp"

#include <concepts>
#include <memory>
#include <type_traits>

namespace satemu::cart {

class CartridgeSlot {
public:
    CartridgeSlot();

    void Reset(bool hard) {
        m_cart->Reset(hard);
    }

    template <typename T, typename... Args>
        requires std::derived_from<T, cart::BaseCartridge>
    [[nodiscard]] bool Insert(Args &&...args) {
        auto cart = std::make_unique<T>(std::forward<Args>(args)...);
        if (!cart->IsInitialized()) [[unlikely]] {
            return false;
        }
        m_cart.reset(cart.release());
        return true;
    }

    void Eject();

    uint8 GetID() const {
        return m_cart->GetID();
    }

    uint8 ReadByte(uint32 address) const {
        return m_cart->ReadByte(address);
    }

    uint16 ReadWord(uint32 address) const {
        return m_cart->ReadWord(address);
    }

    void WriteByte(uint32 address, uint8 value) {
        m_cart->WriteByte(address, value);
    }

    void WriteWord(uint32 address, uint16 value) {
        m_cart->WriteWord(address, value);
    }

private:
    std::unique_ptr<BaseCartridge> m_cart;
};

} // namespace satemu::cart
