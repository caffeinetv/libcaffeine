// Copyright 2019 Caffeine Inc. All rights reserved.

#include "Policy.hpp"
#include "Utils.hpp"

namespace caff {
    inline bool isRatioLess(int64_t width1, int64_t height1, int64_t width2, int64_t height2) {
        return width1 * height2 < width2 * height1;
    }

    inline bool isRatioGreater(int64_t width1, int64_t height1, int64_t width2, int64_t height2) {
        return width1 * height2 > width2 * height1;
    }

    caff_Result checkAspectRatio(int32_t width, int32_t height) {
        if (isRatioGreater(width, height, maxAspectWidth, maxAspectHeight)) {
            return caff_ResultAspectTooWide;
        } else if (isRatioLess(width, height, minAspectWidth, minAspectHeight)) {
            return caff_ResultAspectTooNarrow;
        } else {
            return caff_ResultSuccess;
        }
    }

    std::string annotateTitle(std::string title) {
        trim(title);
        if (title.empty()) {
            title = defaultTitle;
        }

        if (title.length() > maxTitleLength)
            title.resize(maxTitleLength);

        return title;
    }
} // namespace caff
