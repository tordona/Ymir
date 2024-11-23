#pragma once

#include "scu_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <fmt/format.h>

namespace satemu {

class SCU {
public:
    SCU() {
        Reset(true);
    }

    void Reset(bool hard) {}

    template <mem_access_type T>
    T Read(uint32 address) {
        fmt::println("unhandled {}-bit SCU read from {:02X}", sizeof(T) * 8, address);
        return 0;
    }

    template <mem_access_type T>
    void Write(uint32 address, T value) {
        fmt::println("unhandled {}-bit SCU write to {:02X} = {:X}", sizeof(T) * 8, address, value);
    }

private:
};

} // namespace satemu
