// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include <mutex>

#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "media/base/adapted_video_track_source.h"
#include "rtc_base/ref_counted_object.h"

namespace caff {

    class VideoSource : public rtc::AdaptedVideoTrackSource {
    public:
        VideoSource() {}
        VideoSource(VideoSource const &) = delete;
        VideoSource & operator=(VideoSource const &) = delete;
        virtual ~VideoSource() {}

        virtual bool is_screencast() const override;
        virtual absl::optional<bool> needs_denoising() const override;
        virtual SourceState state() const override;
        virtual bool remote() const override;

        rtc::scoped_refptr<webrtc::I420Buffer> sendVideo(
                uint8_t const * frame, size_t frameBytes, int32_t width, int32_t height, webrtc::VideoType format);

    private:
        int64_t lastFrameMicros = 0; // TODO: remove this when we get timestamp from client code
    };

} // namespace caff
