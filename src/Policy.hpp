// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

// TODO: Move some other things in here

#include "caffeine.h"

namespace caff {
    int32_t constexpr maxAspectWidth = 3;
    int32_t constexpr maxAspectHeight = 1;

    int32_t constexpr minAspectWidth = 1;
    int32_t constexpr minAspectHeight = 3;


    caff_Result checkAspectRatio(int32_t width, int32_t height);
} // namespace caff
