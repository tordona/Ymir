#pragma once

#include "sh2.hpp"
#include "sh2_bus.hpp"

namespace satemu::sh2 {

class SH2Block {
public:
    SH2Block(scu::SCU &scu, smpc::SMPC &smpc)
        : bus(scu, smpc)
        , master(bus, true)
        , slave(bus, false) {}

    SH2Bus bus;
    SH2 master;
    SH2 slave;

    void Reset(bool hard) {
        master.Reset(hard);
        slave.Reset(hard);
    }
};

} // namespace satemu::sh2
