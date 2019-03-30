// Copyright 2019 Caffeine Inc. All rights reserved.

#include "VideoCapturer.hpp"

#include "api/video/i420_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/logging.h"
#include "libyuv.h"

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
        RTC_NOTREACHED();
        return libyuv::kRotate0;
    }

    // Copied from old version of libwebrtc
    static int convertToI420(
        webrtc::VideoType srcVideoType,
        const uint8_t* srcFrame,
        int cropX,
        int cropY,
        int srcWidth,
        int srcHeight,
        size_t sampleSize,
        webrtc::VideoRotation rotation,
        webrtc::I420Buffer* dstBuffer) {

        int dstWidth = dstBuffer->width();
        int dstHeight = dstBuffer->height();
        // LibYuv expects pre-rotation values for dst.
        // Stride values should correspond to the destination values.
        if (rotation == webrtc::kVideoRotation_90 ||
            rotation == webrtc::kVideoRotation_270) {
            std::swap(dstWidth, dstHeight);
        }
        return libyuv::ConvertToI420(
            srcFrame, sampleSize,
            dstBuffer->MutableDataY(), dstBuffer->StrideY(),
            dstBuffer->MutableDataU(), dstBuffer->StrideU(),
            dstBuffer->MutableDataV(), dstBuffer->StrideV(),
            cropX, cropY,
            srcWidth, srcHeight,
            dstWidth, dstHeight,
            convertRotationMode(rotation),
            webrtc::ConvertVideoType(srcVideoType));
    }

    cricket::CaptureState VideoCapturer::Start(cricket::VideoFormat const& format) {
        SetCaptureFormat(&format);
        SetCaptureState(cricket::CS_RUNNING);
        return cricket::CS_STARTING;
    }

    static int32_t const maxHeight = 720;
    static int32_t const minDimension = 360;

    // FPS limit is 30 (fudged a bit for variance)
    static int64_t const minFrameMicros = (1000000 / 32);

    void VideoCapturer::sendVideo(
        uint8_t const* frameData,
        size_t frameByteCount,
        int32_t width,
        int32_t height,
        webrtc::VideoType format) {

        auto const now = rtc::TimeMicros();
        auto span = now - lastFrameMicros;
        if (span < minFrameMicros) {
            RTC_LOG(LS_INFO) << "Dropping frame";
            return;
        }
        lastFrameMicros = now;

        int32_t adaptedWidth = minDimension;
        int32_t adaptedHeight = minDimension;
        int32_t cropWidth;
        int32_t cropHeight;
        int32_t cropX;
        int32_t cropY;
        int64_t translatedCameraTime;

        if (!AdaptFrame(width, height, now, now,
            &adaptedWidth, &adaptedHeight, &cropWidth, &cropHeight,
            &cropX, &cropY, &translatedCameraTime)) {
            RTC_LOG(LS_INFO) << "Adapter dropped the frame.";
            return;
        }

        // we will cap the minimum resolution to be 360 on the smaller of either width
        // or height depending on the orientation.

        if ((width >= height) && (adaptedHeight < minDimension)) {
            adaptedWidth = width * minDimension / height;
            adaptedHeight = minDimension;
        }
        else if ((height > width) && (adaptedWidth < minDimension)) {
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
        adaptedWidth = (adaptedWidth + 1) & ~1;    // round up to even
        adaptedHeight = (adaptedHeight + 1) & ~1;  // round up to even

        rtc::scoped_refptr<webrtc::I420Buffer> unscaledBuffer = webrtc::I420Buffer::Create(width, height);

        convertToI420(
            format, frameData, 0, 0, width, height, frameByteCount, webrtc::kVideoRotation_0, unscaledBuffer.get());

        rtc::scoped_refptr<webrtc::I420Buffer> scaledBuffer = unscaledBuffer;
        if (adaptedHeight != height) {
            scaledBuffer = webrtc::I420Buffer::Create(adaptedWidth, adaptedHeight);
            scaledBuffer->ScaleFrom(*unscaledBuffer);
        }

        webrtc::VideoFrame frame(scaledBuffer, webrtc::kVideoRotation_0, translatedCameraTime);

        OnFrame(frame, adaptedWidth, adaptedHeight);
    }

    void VideoCapturer::Stop() {}

    bool VideoCapturer::IsRunning() {
        return true;
    }

    bool VideoCapturer::IsScreencast() const {
        return false;
    }

    bool VideoCapturer::GetPreferredFourccs(std::vector<uint32_t>* fourccs) {
        // ignore preferred formats
        if (fourccs == nullptr)
            return false;

        fourccs->clear();
        return true;
    }

}  // namespace caff
