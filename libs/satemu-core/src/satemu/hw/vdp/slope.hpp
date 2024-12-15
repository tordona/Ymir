#pragma once

#include <satemu/core_types.hpp>
#include <satemu/util/inline.hpp>

#include <cmath>
#include <utility>

namespace satemu::vdp {

FORCE_INLINE auto SafeDiv(auto dividend, auto divisor) {
    return (divisor != 0) ? dividend / divisor : 0;
}

class Slope {
public:
    static constexpr sint64 kFracBits = 16;
    static constexpr sint64 kFracOne = 1ll << kFracBits;

    FORCE_INLINE static constexpr sint64 ToFrac(sint32 value) {
        return static_cast<sint64>(value) << kFracBits;
    }

    FORCE_INLINE static constexpr sint64 ToFracHalfBias(sint32 value) {
        return ((static_cast<sint64>(value) << 1) + 1) << (kFracBits - 1);
    }

    Slope(sint32 x1, sint32 y1, sint32 x2, sint32 y2) {
        const sint32 dx = x2 - x1;
        const sint32 dy = y2 - y1;

        dmaj = std::max(abs(dx), abs(dy));

        xmajor = abs(dx) >= abs(dy);
        if (xmajor) {
            majinc = dx >= 0 ? kFracOne : -kFracOne;
            mininc = SafeDiv(ToFracHalfBias(dy), dmaj);
            majcounter = ToFrac(x1);
            majcounterend = ToFrac(x2);
            mincounter = ToFracHalfBias(y1);
        } else {
            majinc = dy >= 0 ? kFracOne : -kFracOne;
            mininc = SafeDiv(ToFracHalfBias(dx), dmaj);
            majcounter = ToFrac(y1);
            majcounterend = ToFrac(y2);
            mincounter = ToFracHalfBias(x1);
        }
    }

    // Steps the slope to the next coordinate.
    // Should not be invoked when CanStep() returns false
    FORCE_INLINE void Step() {
        majcounter += majinc;
        mincounter += mininc;
    }

    // Determines if the slope can be stepped
    FORCE_INLINE bool CanStep() const {
        return majcounter != majcounterend;
    }

    // Retrieves the current X coordinate (no fractional bits)
    FORCE_INLINE sint32 X() const {
        return (xmajor ? majcounter : mincounter) >> kFracBits;
    }

    // Retrieves the current Y coordinate (no fractional bits)
    FORCE_INLINE sint32 Y() const {
        return (xmajor ? mincounter : majcounter) >> kFracBits;
    }

protected:
    sint32 dmaj;   // major span of the slope: max(abs(dx), abs(dy))
    sint64 majinc; // fractional increment on the major axis (+1 or -1)
    sint64 mininc; // fractional increment on the minor axis

    bool xmajor; // true if abs(dx) >= abs(dy)

    sint64 majcounter;    // coordinate counter for the major axis (fractional, incremented by majinc per step)
    sint64 majcounterend; // final coordinate counter for the major axis
    sint64 mincounter;    // coordinate counter for the minor axis (fractional, incremented by mininc per step)

    // Retrieves the current fractional X coordinate
    FORCE_INLINE sint32 FracX() const {
        return xmajor ? majcounter : mincounter;
    }

    // Retrieves the current fractional Y coordinate
    FORCE_INLINE sint32 FracY() const {
        return xmajor ? mincounter : majcounter;
    }

    friend class EdgeIterator;
};

class LinePlotter : public Slope {
public:
    LinePlotter(sint32 x1, sint32 y1, sint32 x2, sint32 y2)
        : Slope(x1, y1, x2, y2) {

        const bool samesign = (x1 > x2) == (y1 > y2);
        if (xmajor) {
            aaxinc = samesign ? 0 : majinc;
            aayinc = samesign ? (y1 <= y2 ? kFracOne : -kFracOne) : 0;
        } else {
            aaxinc = samesign ? 0 : (x1 <= x2 ? kFracOne : -kFracOne);
            aayinc = samesign ? majinc : 0;
        }
    }

    // Determines if the current step needs antialiasing
    FORCE_INLINE bool NeedsAntiAliasing() const {
        // Antialiasing is needed when the coordinate on the minor axis has changed from the previous step
        return ((mincounter - mininc) >> kFracBits) != (mincounter >> kFracBits);
    }

    // Returns the X coordinate of the antialiased pixel
    FORCE_INLINE sint32 AAX() const {
        return (FracX() - aaxinc) >> kFracBits;
    }

    // Returns the Y coordinate of the antialiased pixel
    FORCE_INLINE sint32 AAY() const {
        return (FracY() - aayinc) >> kFracBits;
    }

private:
    sint64 aaxinc; // X increment for antialiasing
    sint64 aayinc; // Y increment for antialiasing
};

class EdgeIterator {
public:
    // Builds an edge iterator for a quad with vertices A-B-C-D arranged in clockwise order from top-left:
    //
    //    A-->B
    //    ^   |
    //    |   v
    //    D<--C
    EdgeIterator(sint32 xa, sint32 ya, sint32 xb, sint32 yb, sint32 xc, sint32 yc, sint32 xd, sint32 yd)
        : majslope(xa, ya, xd, yd)
        , minslope(xb, yb, xc, yc) {

        // Ensure the major slope is the longest
        if (majslope.dmaj < minslope.dmaj) {
            std::swap(majslope, minslope);
        }

        minmajinc = SafeDiv(minslope.majinc * minslope.dmaj, majslope.dmaj);
        minmininc = SafeDiv(minslope.mininc * minslope.dmaj, majslope.dmaj);
    }

    // Determines if the edge can be stepped
    FORCE_INLINE bool CanStep() const {
        return majslope.CanStep();
    }

    // Retrieves the current X coordinate of the major slope
    FORCE_INLINE sint32 XMaj() const {
        return majslope.X();
    }

    // Retrieves the current Y coordinate of the major slope
    FORCE_INLINE sint32 YMaj() const {
        return majslope.Y();
    }

    // Retrieves the current X coordinate of the minor slope
    FORCE_INLINE sint32 XMin() const {
        return minslope.X();
    }

    // Retrieves the current Y coordinate of the minor slope
    FORCE_INLINE sint32 YMin() const {
        return minslope.Y();
    }

    // Steps both slopes of the edge to the next coordinate.
    // The major slope is stepped by a full pixel.
    // The minor slope is stepped in proportion to the major slope.
    // Should not be invoked when CanStep() returns false
    FORCE_INLINE void Step() {
        majslope.Step();
        // Step minor slope by a fraction proportional to majslope.dmaj / minslope.dmaj
        minslope.majcounter += minmajinc;
        minslope.mincounter += minmininc;
    }

    Slope majslope; // slope with the longest span
    Slope minslope; // slope with the shortest span

private:
    sint64 minmajinc, minmininc; // minor slope interpolation increments with fractional bits
};

} // namespace satemu::vdp
