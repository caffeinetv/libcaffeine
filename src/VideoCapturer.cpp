// Copyright 2019 Caffeine Inc. All rights reserved.

#include "VideoCapturer.hpp"

#include "api/video/i420_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/logging.h"
#include "libyuv.h"

namespace caff {

    // Copied from old version of libwebrtc
    libyuv::RotationMode ConvertRotationMode(webrtc::VideoRotation rotation) {
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
    static int ConvertToI420(
        webrtc::VideoType src_video_type,
        const uint8_t* src_frame,
        int crop_x,
        int crop_y,
        int src_width,
        int src_height,
        size_t sample_size,
        webrtc::VideoRotation rotation,
        webrtc::I420Buffer* dst_buffer) {

        int dst_width = dst_buffer->width();
        int dst_height = dst_buffer->height();
        // LibYuv expects pre-rotation values for dst.
        // Stride values should correspond to the destination values.
        if (rotation == webrtc::kVideoRotation_90 ||
            rotation == webrtc::kVideoRotation_270) {
            std::swap(dst_width, dst_height);
        }
        return libyuv::ConvertToI420(
            src_frame, sample_size,
            dst_buffer->MutableDataY(), dst_buffer->StrideY(),
            dst_buffer->MutableDataU(), dst_buffer->StrideU(),
            dst_buffer->MutableDataV(), dst_buffer->StrideV(),
            crop_x, crop_y,
            src_width, src_height,
            dst_width, dst_height,
            ConvertRotationMode(rotation),
            webrtc::ConvertVideoType(src_video_type));
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

    void VideoCapturer::SendVideo(
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

        int32_t adapted_width = minDimension;
        int32_t adapted_height = minDimension;
        int32_t crop_width;
        int32_t crop_height;
        int32_t crop_x;
        int32_t crop_y;
        int64_t translated_camera_time;

        if (!AdaptFrame(width, height, now, now,
            &adapted_width, &adapted_height, &crop_width, &crop_height,
            &crop_x, &crop_y, &translated_camera_time)) {
            RTC_LOG(LS_INFO) << "Adapter dropped the frame.";
            return;
        }

        // we will cap the minimum resolution to be 360 on the smaller of either width
        // or height depending on the orientation.

        if ((width >= height) && (adapted_height < minDimension)) {
            adapted_width = width * minDimension / height;
            adapted_height = minDimension;
        }
        else if ((height > width) && (adapted_width < minDimension)) {
            adapted_width = minDimension;
            adapted_height = height * minDimension / width;
        }

        // And cap the maximum vertical resolution to be 720
        if (adapted_height > maxHeight) {
            adapted_width = adapted_width * maxHeight / adapted_height;
            adapted_height = maxHeight;
        }

        // if the given input is a weird resolution that is an odd number, the adapted
        // may be odd too, and we need to ensure that it is even.
        adapted_width = (adapted_width + 1) & ~1;    // round up to even
        adapted_height = (adapted_height + 1) & ~1;  // round up to even

        rtc::scoped_refptr<webrtc::I420Buffer> unscaledBuffer = webrtc::I420Buffer::Create(width, height);

        ConvertToI420(
            format, frameData, 0, 0, width, height, frameByteCount, webrtc::kVideoRotation_0, unscaledBuffer.get());

        rtc::scoped_refptr<webrtc::I420Buffer> scaledBuffer = unscaledBuffer;
        if (adapted_height != height) {
            scaledBuffer = webrtc::I420Buffer::Create(adapted_width, adapted_height);
            scaledBuffer->ScaleFrom(*unscaledBuffer);
        }

        webrtc::VideoFrame frame(scaledBuffer, webrtc::kVideoRotation_0, translated_camera_time);

        OnFrame(frame, adapted_width, adapted_height);
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
