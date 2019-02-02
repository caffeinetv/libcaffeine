// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include <atomic>
#include <functional>
#include <vector>

#include "iceinfo.hpp"

#include "caffeine-helpers.hpp"

#include "common_types.h"
#include "rtc_base/scoped_ref_ptr.h"

ASSERT_MATCH(CAFF_FORMAT_UNKNOWN, webrtc::VideoType::kUnknown);
ASSERT_MATCH(CAFF_FORMAT_I420, webrtc::VideoType::kI420);
ASSERT_MATCH(CAFF_FORMAT_IYUV, webrtc::VideoType::kIYUV);
ASSERT_MATCH(CAFF_FORMAT_RGB24, webrtc::VideoType::kRGB24);
ASSERT_MATCH(CAFF_FORMAT_ABGR, webrtc::VideoType::kABGR);
ASSERT_MATCH(CAFF_FORMAT_ARGB, webrtc::VideoType::kARGB);
ASSERT_MATCH(CAFF_FORMAT_ARGB4444, webrtc::VideoType::kARGB4444);
ASSERT_MATCH(CAFF_FORMAT_RGB565, webrtc::VideoType::kRGB565);
ASSERT_MATCH(CAFF_FORMAT_ARGB1555, webrtc::VideoType::kARGB1555);
ASSERT_MATCH(CAFF_FORMAT_YUY2, webrtc::VideoType::kYUY2);
ASSERT_MATCH(CAFF_FORMAT_YV12, webrtc::VideoType::kYV12);
ASSERT_MATCH(CAFF_FORMAT_UYVY, webrtc::VideoType::kUYVY);
ASSERT_MATCH(CAFF_FORMAT_MJPEG, webrtc::VideoType::kMJPEG);
ASSERT_MATCH(CAFF_FORMAT_NV21, webrtc::VideoType::kNV21);
ASSERT_MATCH(CAFF_FORMAT_NV12, webrtc::VideoType::kNV12);
ASSERT_MATCH(CAFF_FORMAT_BGRA, webrtc::VideoType::kBGRA);

namespace webrtc {
    class PeerConnectionFactoryInterface;
    class MediaStreamInterface;
    class PeerConnectionInterface;
}  // namespace webrtc

namespace caff {
    class AudioDevice;
    class VideoCapturer;

    class Stream {
    public:
        static int const kMaxBitrateBps = 2000000;

        Stream(AudioDevice* audioDevice, webrtc::PeerConnectionFactoryInterface* factory);
        virtual ~Stream();

        void Start(
            std::function<std::string(std::string const&)> offerGeneratedCallback,
            std::function<bool(std::vector<IceInfo> const&)> iceGatheredCallback,
            std::function<void()> startedCallback,
            std::function<void(caff_error)> failedCallback);

        void SendAudio(uint8_t const* samples, size_t samples_per_channel);
        void SendVideo(uint8_t const* frameData, size_t frameBytes, int32_t width, int32_t height, caff_format format);

    private:
        std::atomic_bool started{ false };
        AudioDevice* audioDevice;
        VideoCapturer* videoCapturer;
        webrtc::PeerConnectionFactoryInterface* factory;
        rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection;
    };

}  // namespace caff
