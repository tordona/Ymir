#pragma once

#include <satemu/core_types.hpp>
#include <satemu/util/inline.hpp>

#include <cmath>
#include <utility>

namespace satemu::vdp {

FORCE_INLINE auto SafeDiv(auto dividend, auto divisor) {
    return (divisor != 0) ? dividend / divisor : 0;
}

struct Slope {
    static constexpr sint64 kFracBits = 16;

    static constexpr sint64 ToFrac(sint32 value) {
        return static_cast<sint64>(value) << kFracBits;
    }

    static constexpr sint64 ToFracHalfBias(sint32 value) {
        return ((static_cast<sint64>(value) << 1) + 1) << (kFracBits - 1);
    }

    Slope(sint32 x1, sint32 y1, sint32 x2, sint32 y2)
        : x1(x1)
        , y1(y1)
        , x2(x2)
        , y2(y2) {
        dx = x2 - x1;
        dy = y2 - y1;
        dmaj = std::max(abs(dx), abs(dy));

        fx1 = ToFracHalfBias(x1);
        fy1 = ToFracHalfBias(y1);
        fxinc = SafeDiv(ToFracHalfBias(dx), dmaj);
        fyinc = SafeDiv(ToFracHalfBias(dy), dmaj);

        xmajor = abs(dx) >= abs(dy);
        if (xmajor) {
            aspect = SafeDiv(ToFrac(dy), abs(dx));
            dmajinc = dx >= 0 ? 1 : -1;
            majcounter = x1;
            majcounterend = x2;
            mincounter = fy1;
        } else {
            aspect = SafeDiv(ToFrac(dx), abs(dy));
            dmajinc = dy >= 0 ? 1 : -1;
            majcounter = y1;
            majcounterend = y2;
            mincounter = fx1;
        }
    }

    // Steps the slope to the next coordinate.
    // Should not be invoked when CanStep() returns false
    void Step() {
        majcounter += dmajinc;
        mincounter += aspect;
    }

    // Determines if the slope can be stepped
    bool CanStep() const {
        return majcounter != majcounterend;
    }

    // Retrieves the current X coordinate (no fractional bits)
    sint32 X() const {
        return xmajor ? majcounter : (mincounter >> kFracBits);
    }

    // Retrieves the current Y coordinate (no fractional bits)
    sint32 Y() const {
        return xmajor ? (mincounter >> kFracBits) : majcounter;
    }

    sint32 x1, y1; // starting coordinates
    sint32 x2, y2; // ending coordinates

    sint32 dx;      // width of the slope: x2 - x1
    sint32 dy;      // height of the slope: y2 - y1
    sint32 dmaj;    // major span of the slope: max(abs(dx), abs(dy))
    sint32 dmajinc; // increment on the major axis (+1 or -1)

    sint64 aspect; // slope aspect: dy/dx (x-major) or dx/dy (y-major) (with fractional bits)
    bool xmajor;   // true if abs(dx) >= abs(dy)

    sint64 fx1, fy1;     // starting coordinates with fractional bits
    sint64 fxinc, fyinc; // slope interpolation increments with fractional bits

    sint32 majcounter;    // coordinate counter for the major axis (incremented by dmajinc per step)
    sint32 majcounterend; // final coordinate counter for the major axis
    sint64 mincounter;    // coordinate counter for the minor axis (fractional, incremented by aspect)
};

struct LinePlotter : public Slope {
    LinePlotter(sint32 x1, sint32 y1, sint32 x2, sint32 y2)
        : Slope(x1, y1, x2, y2) {

        const bool samesign = (x1 > x2) == (y1 > y2);
        if (xmajor) {
            aaxinc = samesign ? 0 : dmajinc;
            aayinc = samesign ? (y1 <= y2 ? 1 : -1) : 0;
        } else {
            aaxinc = samesign ? 0 : (x1 <= x2 ? 1 : -1);
            aayinc = samesign ? dmajinc : 0;
        }
    }

    // Determines if the current step needs antialiasing
    bool NeedsAntiAliasing() const {
        // Antialiasing is needed when the coordinate on the minor axis has changed from the previous step
        return ((mincounter - aspect) >> kFracBits) != (mincounter >> kFracBits);
    }

    // Returns the X coordinate of the antialiased pixel
    sint32 AAX() const {
        return X() - aaxinc;
    }

    // Returns the Y coordinate of the antialiased pixel
    sint32 AAY() const {
        return Y() - aayinc;
    }

    sint32 aaxinc; // X increment for antialiasing
    sint32 aayinc; // Y increment for antialiasing
};

struct EdgeIterator {
    // TODO: build from two slopes
};

} // namespace satemu::vdp
