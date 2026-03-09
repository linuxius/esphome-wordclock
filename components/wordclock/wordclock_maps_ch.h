// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Peter Visser
//
// Part of the WordClock ESPHome component.

#pragma once

#include "wordclock.h"

namespace wordclock {

constexpr WordRange kMapChDots[4] = {
    {7, 7},  // +1
    {6, 6},  // +2
    {5, 5},  // +3
    {4, 4},  // +4
};

constexpr WordRange kMapChHours[12] = {
    {81, 83},  // 1
    {76, 79},  // 2
    {72, 74},  // 3
    {60, 64},  // 4
    {66, 69},  // 5
    {54, 59},  // 6
    {48, 52},  // 7
    {36, 40},  // 8
    {42, 45},  // 9
    {31, 35},  // 10
    {24, 27},  // 11
    {13, 18},  // 12
};

constexpr WordMap kMapCh = {
    {130, 131},  // IT
    {125, 128},  // IS
    {kInvalidIndex, kInvalidIndex},  // OCLOCK (not mapped in Swiss plate)
    {84, 85},    // PAST
    {96, 98},    // TO
    {kInvalidIndex, kInvalidIndex},  // MINUTES (not mapped in Swiss plate)
    {kInvalidIndex, kInvalidIndex},  // HALF
    {121, 123},  // FIVE
    {117, 119},  // TEN
    {109, 114},  // FIFTEEN
    {102, 107},  // TWENTY
    {88, 92},    // TWENTYFIVE
    {88, 92},    // THIRTY
    {kInvalidIndex, kInvalidIndex},  // HOUR_0
    {kMapChHours[0], kMapChHours[1], kMapChHours[2], kMapChHours[3],
     kMapChHours[4], kMapChHours[5], kMapChHours[6], kMapChHours[7],
     kMapChHours[8], kMapChHours[9], kMapChHours[10], kMapChHours[11]},
    {kMapChDots[0], kMapChDots[1], kMapChDots[2], kMapChDots[3]},
};

}  // namespace wordclock
