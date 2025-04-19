#pragma once

#include <string_view>

namespace ymir::peripheral {

enum class PeripheralType { None, StandardPad };

inline std::string_view GetPeripheralName(PeripheralType type) {
    switch (type) {
    case PeripheralType::None: return "None";
    case PeripheralType::StandardPad: return "Standard Saturn Pad";
    default: return "Invalid";
    }
}

inline constexpr PeripheralType kTypes[] = {PeripheralType::None, PeripheralType::StandardPad};

class NullPeripheral;
class StandardPad;

namespace detail {

    template <PeripheralType type>
    struct PeripheralTypeMeta {};

    template <>
    struct PeripheralTypeMeta<PeripheralType::None> {
        using type = NullPeripheral;
    };

    template <>
    struct PeripheralTypeMeta<PeripheralType::StandardPad> {
        using type = StandardPad;
    };

    template <PeripheralType type>
    using PeripheralType_t = typename PeripheralTypeMeta<type>::type;

} // namespace detail

} // namespace ymir::peripheral
