// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "caffeine.h"

#include <string>

namespace caff {
    // Broadcasting on the Caffeine platform after altering the values in this file may be deemed a violation of the
    // Caffeine Terms of Service (https://www.caffeine.tv/tos.html).


    // Network
    int constexpr maxBitsPerSecond = 2'000'000;

    // Video
    int32_t constexpr maxAspectWidth = 3;
    int32_t constexpr maxAspectHeight = 1;
    int32_t constexpr minAspectWidth = 1;
    int32_t constexpr minAspectHeight = 3;
    int32_t constexpr maxFrameHeight = 720;
    int32_t constexpr maxFrameWidth = 1280;
    int32_t constexpr minFrameDimension = 360;
    int32_t constexpr maxFps = 30;

    // Audio
    size_t constexpr channels = 2;
    size_t constexpr sampleRate = 48'000;
    size_t constexpr sampleSize = 2;

    // Stage
    char constexpr defaultTitle[] = "LIVE on Caffeine!";
    char constexpr seventeenPlusTag[] = "[17+] ";
    size_t constexpr maxTitleLength = 255;


    // Helpers
    caff_Result checkAspectRatio(int32_t width, int32_t height);

    std::string annotateTitle(std::string title, caff_Rating rating);
} // namespace caff
