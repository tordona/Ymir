#pragma once

#include <satemu/core/types.hpp>

#include <satemu/util/inline.hpp>

namespace satemu::cart {

enum class CartType { None, BackupMemory, DRAM8Mbit, DRAM32Mbit };

class BackupMemoryCartridge;
class DRAM8MbitCartridge;
class DRAM32MbitCartridge;

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

    // If this cartridge object has the specified CartType, casts it to the corresponding concrete type.
    // Returns nullptr otherwise.
    template <CartType type>
    FORCE_INLINE typename detail::CartTypeHelper<type>::type *As() {
        if (m_type == type) {
            return static_cast<detail::CartTypeHelper<type>::type *>(this);
        } else {
            return nullptr;
        }
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
