#include <satemu/hw/cart/cart_slot.hpp>

#include <satemu/hw/cart/cart_impl_none.hpp>

namespace satemu::cart {

CartridgeSlot::CartridgeSlot() {
    Eject();
}

void CartridgeSlot::Eject() {
    m_cart = std::make_unique<NoCartridge>();
}

} // namespace satemu::cart
