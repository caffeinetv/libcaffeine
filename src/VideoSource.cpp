// Copyright 2019 Caffeine Inc. All rights reserved.

#include "VideoSource.hpp"

#include "ErrorLogging.hpp"

#include "libyuv.h"
#include "rtc_base/time_utils.h"

namespace caff {

    // Copied from old version of libwebrtc
    static libyuv::RotationMode convertRotationMode(webrtc::VideoRotation rotation) {
        switch (rotation) {
        case webrtc::kVideoRotation_0:
            return libyuv::kRotate0;
        case webrtc::kVideoRotation_90:
            return libyuv::kRotate90;
        case webrtc::kVideoRotation_180:
            return libyuv::kRotate180;
        case webrtc::kVideoRotation_270:
            return libyuv::kRotate270;
        }
    }

    // Copied from old version of libwebrtc
    static int convertToI420(
            webrtc::VideoType srcVideoType,
            const uint8_t * srcFrame,
            int cropX,
            int cropY,
            int srcWidth,
            int srcHeight,
            size_t sampleSize,
            webrtc::VideoRotation rotation,
            webrtc::I420Buffer * dstBuffer) {
        int dstWidth = dstBuffer->width();
        int dstHeight = dstBuffer->height();
        // LibYuv expects pre-rotation values for dst.
        // Stride values should correspond to the destination values.
        if (rotation == webrtc::kVideoRotation_90 || rotation == webrtc::kVideoRotation_270) {
            std::swap(dstWidth, dstHeight);
        }
        return libyuv::ConvertToI420(
                srcFrame,
                sampleSize,
                dstBuffer->MutableDataY(),
                dstBuffer->StrideY(),
                dstBuffer->MutableDataU(),
                dstBuffer->StrideU(),
                dstBuffer->MutableDataV(),
                dstBuffer->StrideV(),
                cropX,
                cropY,
                srcWidth,
                srcHeight,
                dstWidth,
                dstHeight,
                convertRotationMode(rotation),
                webrtc::ConvertVideoType(srcVideoType));
    }

    static int32_t const maxHeight = 720;
    static int32_t const minDimension = 360;

    // FPS limit is 30 (fudged a bit for variance)
    static int64_t const minFrameMicros = (1000000 / 32);

    rtc::scoped_refptr<webrtc::I420Buffer> VideoSource::sendVideo(
            uint8_t const * frameData, size_t frameByteCount, int32_t width, int32_t height, webrtc::VideoType format) {
        auto const now = rtc::TimeMicros();
        auto span = now - lastFrameMicros;
        if (span < minFrameMicros) {
            LOG_DEBUG("Dropping frame");
            return nullptr;
        }
        lastFrameMicros = now;

        int32_t adaptedWidth = minDimension;
        int32_t adaptedHeight = minDimension;
        int32_t cropWidth;
        int32_t cropHeight;
        int32_t cropX;
        int32_t cropY;

        if (!AdaptFrame(width, height, now, &adaptedWidth, &adaptedHeight, &cropWidth, &cropHeight, &cropX, &cropY)) {
            LOG_DEBUG("Adapter dropped the frame.");
            return nullptr;
        }

        // we will cap the minimum resolution to be 360 on the smaller of either width
        // or height depending on the orientation.

        if ((width >= height) && (adaptedHeight < minDimension)) {
            adaptedWidth = width * minDimension / height;
            adaptedHeight = minDimension;
        } else if ((height > width) && (adaptedWidth < minDimension)) {
            adaptedWidth = minDimension;
            adaptedHeight = height * minDimension / width;
        }

        // And cap the maximum vertical resolution to be 720
        if (adaptedHeight > maxHeight) {
            adaptedWidth = adaptedWidth * maxHeight / adaptedHeight;
            adaptedHeight = maxHeight;
        }

        // if the given input is a weird resolution that is an odd number, the adapted
        // may be odd too, and we need to ensure that it is even.
        adaptedWidth = (adaptedWidth + 1) & ~1;   // round up to even
        adaptedHeight = (adaptedHeight + 1) & ~1; // round up to even

        rtc::scoped_refptr<webrtc::I420Buffer> unscaledBuffer = webrtc::I420Buffer::Create(width, height);

        convertToI420(
                format, frameData, 0, 0, width, height, frameByteCount, webrtc::kVideoRotation_0, unscaledBuffer.get());

        rtc::scoped_refptr<webrtc::I420Buffer> scaledBuffer = unscaledBuffer;
        if (adaptedHeight != height) {
            scaledBuffer = webrtc::I420Buffer::Create(adaptedWidth, adaptedHeight);
            scaledBuffer->ScaleFrom(*unscaledBuffer);
        }

        webrtc::VideoFrame frame(scaledBuffer, webrtc::kVideoRotation_0, now);

        OnFrame(frame);

        return scaledBuffer;
    }

    bool VideoSource::is_screencast() const { return false; }

    absl::optional<bool> VideoSource::needs_denoising() const { return {}; }

    VideoSource::SourceState VideoSource::state() const { return SourceState::kLive; }

    bool VideoSource::remote() const { return false; }
} // namespace caff
