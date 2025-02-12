#pragma once

namespace satemu::debug {

// Interface for debug probes.
struct IProbe {
    virtual ~IProbe() = default;

    virtual void test() = 0;
};

} // namespace satemu::debug
