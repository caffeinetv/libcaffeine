// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

// TODO: Move more things in here (max BPS, max frame size, etc.)

#include "caffeine.h"

namespace caff {
    // Broadcasting on the Caffeine platform after altering the values in this file may be deemed a violation of the
    // Caffeine Terms of Service (https://www.caffeine.tv/tos.html).

    int32_t constexpr maxAspectWidth = 3;
    int32_t constexpr maxAspectHeight = 1;

    int32_t constexpr minAspectWidth = 1;
    int32_t constexpr minAspectHeight = 3;


    caff_Result checkAspectRatio(int32_t width, int32_t height);
} // namespace caff
