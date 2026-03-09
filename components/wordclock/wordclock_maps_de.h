// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Peter Visser
//
// Part of the WordClock ESPHome component.

#pragma once

#include "wordclock.h"

namespace wordclock {

constexpr WordRange kMapDeDots[4] = {
    {kInvalidIndex, kInvalidIndex},
    {kInvalidIndex, kInvalidIndex},
    {kInvalidIndex, kInvalidIndex},
    {kInvalidIndex, kInvalidIndex},
};

constexpr WordRange kMapDeHours[12] = {
    {49, 52},  // 1
    {30, 33},  // 2
    {37, 40},  // 3
    {25, 28},  // 4
    {42, 45},  // 5
    {12, 16},  // 6
    {66, 71},  // 7
    {18, 21},  // 8
    {54, 57},  // 9
    {4, 7},    // 10
    {9, 11},   // 11
    {60, 64},  // 12
};

constexpr WordMap kMapDe = {
    {130, 131},  // IT
    {126, 128},  // IS
    {0, 2},      // OCLOCK
    {92, 95},    // PAST
    {79, 81},    // TO
    {84, 90},    // MINUTES
    {kInvalidIndex, kInvalidIndex},  // HALF
    {108, 111},  // FIVE
    {121, 124},  // TEN
    {113, 119},  // FIFTEEN
    {99, 105},   // TWENTY
    {74, 77},    // TWENTYFIVE
    {74, 77},    // THIRTY
    {50, 52},    // HOUR_0
    {kMapDeHours[0], kMapDeHours[1], kMapDeHours[2], kMapDeHours[3],
     kMapDeHours[4], kMapDeHours[5], kMapDeHours[6], kMapDeHours[7],
     kMapDeHours[8], kMapDeHours[9], kMapDeHours[10], kMapDeHours[11]},
    {kMapDeDots[0], kMapDeDots[1], kMapDeDots[2], kMapDeDots[3]},
};

}  // namespace wordclock
