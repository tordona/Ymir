#include <satemu/hw/cart/cart_slot.hpp>

#include <satemu/hw/cart/cart_impl_none.hpp>

namespace satemu::cart {

static NoCartridge no_instance{};

CartridgeSlot::CartridgeSlot() {
    RemoveCartridge();
}

void CartridgeSlot::RemoveCartridge() {
    m_cart = std::make_unique<NoCartridge>();
}

} // namespace satemu::cart
