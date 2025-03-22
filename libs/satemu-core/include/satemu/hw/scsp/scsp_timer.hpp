#pragma once

#include <satemu/core/types.hpp>

#include <satemu/util/bit_ops.hpp>

namespace satemu::scsp {

struct Timer {
    Timer() {
        Reset();
    }

    void Reset() {
        incrementInterval = 0;
        reload = 0;
        incrementMask = 0;
        doReload = false;
        counter = 0;
    }

    bool Tick() {
        if (doReload) {
            counter = reload;
            doReload = false;
        } else {
            counter++;
        }
        return counter == 0xFF;
    }

    uint8 ReadTIMx() const {
        return reload;
    }

    void WriteTIMx(uint8 value) {
        reload = value;
        doReload = true;
    }

    uint8 ReadTxCTL() const {
        uint8 value = 0;
        bit::deposit_into<0, 2>(value, incrementInterval);
        return value;
    }

    void WriteTxCTL(uint8 value) {
        incrementInterval = bit::extract<0, 2>(value);
        incrementMask = (1ull << incrementInterval) - 1;
    }

    // -------------------------------------------------------------------------
    // Registers

    uint8 incrementInterval; // (W) TxCTL - 0 to 7 - increment every (1 << N) samples
    uint8 reload;            // (W) TIMx - resets the timer counter on the next tick

    // -------------------------------------------------------------------------
    // State

    uint64 incrementMask; // computed from TxCTL
    bool doReload;        // whether to reload the counter on the next tick
    uint8 counter;        // counts up to 0xFF, then raises an interrupt
};

} // namespace satemu::scsp