// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Peter Visser
//
// Part of the WordClock ESPHome component.

#pragma once

#include "wordclock.h"

namespace wordclock {

constexpr WordRange kMapEnDots[4] = {
    {kInvalidIndex, kInvalidIndex},
    {kInvalidIndex, kInvalidIndex},
    {kInvalidIndex, kInvalidIndex},
    {kInvalidIndex, kInvalidIndex},
};

constexpr WordRange kMapEnHours[12] = {
    {20, 22},  // 1
    {45, 47},  // 2
    {15, 19},  // 3
    {67, 70},  // 4
    {40, 43},  // 5
    {12, 14},  // 6
    {55, 59},  // 7
    {31, 35},  // 8
    {36, 39},  // 9
    {9, 11},   // 10
    {24, 29},  // 11
    {48, 53},  // 12
};

constexpr WordMap kMapEn = {
    {130, 131},  // IT
    {127, 128},  // IS
    {0, 5},      // OCLOCK
    {60, 63},    // PAST
    {63, 64},    // TO
    {77, 83},    // MINUTES
    {kInvalidIndex, kInvalidIndex},  // HALF
    {98, 101},   // FIVE
    {91, 93},    // TEN
    {110, 116},  // FIFTEEN
    {102, 107},  // TWENTY
    {98, 107},   // TWENTYFIVE
    {84, 89},    // THIRTY
    {kInvalidIndex, kInvalidIndex},  // HOUR_0
    {kMapEnHours[0], kMapEnHours[1], kMapEnHours[2], kMapEnHours[3],
     kMapEnHours[4], kMapEnHours[5], kMapEnHours[6], kMapEnHours[7],
     kMapEnHours[8], kMapEnHours[9], kMapEnHours[10], kMapEnHours[11]},
    {kMapEnDots[0], kMapEnDots[1], kMapEnDots[2], kMapEnDots[3]},
};

}  // namespace wordclock
