#pragma once

#include <satemu/core_types.hpp>

#include <cmath>
#include <utility>

namespace satemu::vdp {

struct Slope {
    static constexpr sint64 kFracBits = 16;

    Slope(sint32 x1, sint32 y1, sint32 x2, sint32 y2)
        : x1(x1)
        , y1(y1)
        , x2(x2)
        , y2(y2) {
        dx = x2 - x1;
        dy = y2 - y1;
        dmaj = std::max(abs(dx), abs(dy));

        auto toFrac = [](sint32 value) -> sint64 { return static_cast<sint64>(value) << kFracBits; };
        auto toFracHalfBias = [](sint32 value) -> sint64 {
            return ((static_cast<sint64>(value) << 1) + 1) << (kFracBits - 1);
        };
        auto safeDiv = [](auto dividend, auto divisor) { return (divisor != 0) ? dividend / divisor : 0; };

        fx1 = toFracHalfBias(x1);
        fy1 = toFracHalfBias(y1);
        fxinc = safeDiv(toFracHalfBias(dx), dmaj);
        fyinc = safeDiv(toFracHalfBias(dy), dmaj);

        xmajor = abs(dx) >= abs(dy);
        const bool samesign = (dx >> 31) == (dy >> 31);
        if (xmajor) {
            aspect = safeDiv(toFrac(dy), abs(dx));
            dmajinc = dx >= 0 ? 1 : -1;
            aaxinc = samesign ? dmajinc : 0;
            aayinc = samesign ? 0 : (dy >= 0 ? 1 : -1);
            aamininc = aspect;
            majcounter = x1;
            majcounterend = x2;
            mincounter = fy1;
        } else {
            aspect = safeDiv(toFrac(dx), abs(dy));
            dmajinc = dy >= 0 ? 1 : -1;
            aaxinc = samesign ? 0 : (dx >= 0 ? -1 : 1);
            aayinc = samesign ? -dmajinc : 0;
            aamininc = -aspect;
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

    // Determines if the current step needs antialiasing
    bool NeedsAntiAliasing() const {
        // Antialiasing is needed when the coordinate on the minor axis has changed from the previous step
        return ((mincounter + aamininc) >> kFracBits) != (mincounter >> kFracBits);
    }

    // Returns the X coordinate of the antialiased pixel
    sint32 AAX() const {
        return X() + aaxinc;
    }

    // Returns the Y coordinate of the antialiased pixel
    sint32 AAY() const {
        return Y() + aayinc;
    }

    sint32 x1, y1; // starting coordinates
    sint32 x2, y2; // ending coordinates

    sint32 dx;      // width of the slope: x2 - x1
    sint32 dy;      // height of the slope: y2 - y1
    sint32 dmaj;    // major span of the slope: max(abs(dx), abs(dy))
    sint32 dmajinc; // increment on the major axis (+1 or -1)

    sint64 aspect; // slope aspect: dy/dx (x-major) or dx/dy (y-major) (with fractional bits)
    bool xmajor;   // true if abs(dx) >= abs(dy)

    sint32 aaxinc;   // X increment for antialiasing
    sint32 aayinc;   // Y increment for antialiasing
    sint64 aamininc; // Minor axis increment for antialiasing

    sint64 fx1, fy1;     // starting coordinates with fractional bits
    sint64 fxinc, fyinc; // slope interpolation increments with fractional bits

    sint32 majcounter;    // coordinate counter for the major axis (incremented by dmajinc per step)
    sint32 majcounterend; // final coordinate counter for the major axis
    sint64 mincounter;    // coordinate counter for the minor axis (fractional, incremented by aspect)
};

} // namespace satemu::vdp
