#include <satemu/hw/cart/cart_slot.hpp>

#include <satemu/hw/cart/cart_impl_none.hpp>

namespace satemu::cart {

static NoCartridge no_instance{};

CartridgeSlot::CartridgeSlot() {
    EjectCartridge();
}

void CartridgeSlot::EjectCartridge() {
    m_cart = std::make_unique<NoCartridge>();
}

} // namespace satemu::cart
