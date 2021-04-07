// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include <chrono>

#include "api/video/i420_buffer.h"
#include "common_types.h"
#include "media/base/videocapturer.h"
#include "rtc_base/refcountedobject.h"

namespace caff {

    class VideoCapturer : public cricket::VideoCapturer {
    public:
        VideoCapturer();
        VideoCapturer(VideoCapturer const &) = delete;
        VideoCapturer & operator=(VideoCapturer const &) = delete;
        virtual ~VideoCapturer() {}

        rtc::scoped_refptr<webrtc::I420Buffer> sendVideo(
                webrtc::VideoType format,
                uint8_t const * frame,
                size_t frameBytes,
                int32_t width,
                int32_t height,
                std::chrono::microseconds timestamp);

        virtual cricket::CaptureState Start(cricket::VideoFormat const & format) override;
        virtual void Stop() override;
        virtual bool IsRunning() override;
        virtual bool IsScreencast() const override;
        virtual bool GetPreferredFourccs(std::vector<uint32_t> * fourccs) override;

        void SetFramerateLimit(int32_t framerate);
        void SetFrameSizeLimit(int32_t width, int32_t height);

    private:
        std::chrono::microseconds lastTimestamp{ std::chrono::seconds::min() };
        std::chrono::microseconds interFrameLimit;
        int32_t frameWidthMax;
        int32_t frameHeightMax;
    };

}  // namespace caff
