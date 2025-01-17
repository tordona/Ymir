#pragma once

#include <satemu/core_types.hpp>

namespace satemu::scsp {

// States: Attack, Decay 1, Decay 2, Release
//
// Starts from Attack on Key ON.
// While Key ON is held, goes through Attack -> Decay 1 -> Decay 2 and stays at the minimum value of Decay 2.
// On Key OFF, it will immediately skip to Release state, decrementing the envelope from whatever point it was.
//
// when EGHOLD=1:
//
// 0x000      /|\
//           / | \
//          /  |  +---+ DL
//         /   |  |   |\
// 0x3FF  /    |  |   | \_____...
//       |atk  |d1|d2 |release
// Key ON^     Key OFF^
//
// when EGHOLD=0:
//       _______
// 0x000 |     |\
//       |     | \
//       |     |  +---+ DL
//       |     |  |   |\
// 0x3FF |     |  |   | \_____...
//       |atk  |d1|d2 |release
// Key ON^        OFF^
//
// Note: attack takes the same amount of time it would take if going from 0x3FF to 0x000 normally
struct EnvelopeGenerator {
    EnvelopeGenerator() {
        Reset();
    }

    void Reset() {
        state = State::Release;

        attackRate = 0;
        decay1Rate = 0;
        decay2Rate = 0;
        releaseRate = 0;

        decayLevel = 0;

        keyRateScaling = 0;

        egHold = false;

        loopStateLink = false;

        currLevel = 0x3FF;
    }

    void Step() {
        switch (state) {
        case State::Attack:
            if (attackRate < currLevel) {
                currLevel -= attackRate;
            } else {
                currLevel = 0;
                if (!loopStateLink) {
                    state = State::Decay1;
                }
            }
            break;
        case State::Decay1:
            currLevel += decay1Rate;
            if (currLevel > 0x3FF) {
                currLevel = 0x3FF;
            }
            if ((currLevel >> 5u) >= decayLevel) {
                state = State::Decay2;
            }
            break;
        case State::Decay2:
            currLevel += decay2Rate;
            if (currLevel > 0x3FF) {
                currLevel = 0x3FF;
            }
            break;
        case State::Release:
            currLevel += releaseRate;
            if (currLevel > 0x3FF) {
                currLevel = 0x3FF;
            }
            break;
        }
    }

    uint16 GetLevel() const {
        if (state == State::Attack && !egHold) {
            return 0x000;
        } else {
            return currLevel;
        }
    }

    void TriggerKey(bool keyOn) {
        if (keyOn) {
            state = State::Attack;
            currLevel = 0x3FF;
        } else {
            state = State::Release;
        }
    }

    void TriggerLoopStart() {
        if (loopStateLink && state == State::Attack) {
            state = State::Decay1;
        }
    }

    enum class State { Attack, Decay1, Decay2, Release };
    State state;

    // Value ranges are from minimum to maximum.
    uint8 attackRate;  // (R/W) AR  - 0x00 to 0x1F
    uint8 decay1Rate;  // (R/W) D1R - 0x00 to 0x1F
    uint8 decay2Rate;  // (R/W) D2R - 0x00 to 0x1F
    uint8 releaseRate; // (R/W) RR  - 0x00 to 0x1F

    uint8 decayLevel; // (R/W) DL  - 0x1F to 0x00
                      //   specifies the MSB 5 bits of the EG value where to switch from decay 1 to decay 2

    uint8 keyRateScaling; // (R/W) KRS - 0x00 to 0x0E; 0x0F turns off scaling

    bool egHold; // (R/W) EGHOLD
                 //   true:  volume raises during attack state
                 //   false: volume is set to maximum during attack phase while maintaining the same duration

    bool loopStateLink; // (R/W) LPSLNK
                        //   true:  switches to decay 1 state on LSA
                        //          attack state is interrupted if too slow or held if too fast
                        //          if the state change happens below DL, decay 2 state is never reached
                        //   false: state changes are dictated by rates only

    // Current envelope level.
    // Ranges from 0x3FF (minimum) to 0x000 (maximum) - 10 bits.
    uint16 currLevel;
};

} // namespace satemu::scsp
