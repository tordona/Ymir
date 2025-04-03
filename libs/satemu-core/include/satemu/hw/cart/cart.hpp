#pragma once

#include "cart_impl_bup.hpp"
#include "cart_impl_dram.hpp"

#include <satemu/util/inline.hpp>

namespace satemu::cart {

namespace detail {

    template <CartType type>
    struct CartTypeHelper {};

    template <>
    struct CartTypeHelper<CartType::BackupMemory> {
        using type = BackupMemoryCartridge;
    };

    template <>
    struct CartTypeHelper<CartType::DRAM8Mbit> {
        using type = DRAM8MbitCartridge;
    };

    template <>
    struct CartTypeHelper<CartType::DRAM32Mbit> {
        using type = DRAM32MbitCartridge;
    };

} // namespace detail

// If the given pointer to a cartridge object has the specified CartType, casts it to the corresponding concrete type.
// Returns nullptr otherwise.
template <CartType type>
FORCE_INLINE typename detail::CartTypeHelper<type>::type *As(BaseCartridge *cart) {
    if (cart->GetType() == type) {
        return static_cast<detail::CartTypeHelper<type>::type *>(cart);
    } else {
        return nullptr;
    }
}

// If the given reference to a cartridge object has the specified CartType, casts it to the corresponding concrete type.
// Returns nullptr otherwise.
template <CartType type>
FORCE_INLINE typename detail::CartTypeHelper<type>::type *As(BaseCartridge &cart) {
    return As<type>(&cart);
}

} // namespace satemu::cart
