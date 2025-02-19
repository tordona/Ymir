#pragma once

#include <satemu/debug/debug_tracer.hpp>

#include "sh2.hpp"
#include "sh2_bus.hpp"

namespace satemu::sh2 {

class SH2Block {
public:
    SH2Block(scu::SCU &scu, smpc::SMPC &smpc, debug::TracerContext &debugTracer)
        : bus(master, slave, scu, smpc)
        , master(bus, true, debugTracer)
        , slave(bus, false, debugTracer) {}

    SH2Bus bus;
    SH2 master;
    SH2 slave;

    bool slaveEnabled;

    void Reset(bool hard) {
        master.Reset(hard);
        slave.Reset(hard);

        slaveEnabled = false;
    }
};

} // namespace satemu::sh2
