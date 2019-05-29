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
        VideoCapturer() {}
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

    private:
        std::chrono::microseconds lastTimestamp{ 0 };
    };

}  // namespace caff
