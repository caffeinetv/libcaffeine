// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include <atomic>
#include <functional>
#include <vector>

#include "caffeine.h"
#include "Api.hpp"
#include "CaffeineHelpers.hpp"

#include "absl/types/optional.h"
#include "common_types.h"
#include "rtc_base/scoped_ref_ptr.h"

ASSERT_MATCH(caff_VideoFormat_Unknown, webrtc::VideoType::kUnknown);
ASSERT_MATCH(caff_VideoFormat_I420, webrtc::VideoType::kI420);
ASSERT_MATCH(caff_VideoFormat_Iyuv, webrtc::VideoType::kIYUV);
ASSERT_MATCH(caff_VideoFormat_Rgb24, webrtc::VideoType::kRGB24);
ASSERT_MATCH(caff_VideoFormat_Abgr, webrtc::VideoType::kABGR);
ASSERT_MATCH(caff_VideoFormat_Argb, webrtc::VideoType::kARGB);
ASSERT_MATCH(caff_VideoFormat_Argb4444, webrtc::VideoType::kARGB4444);
ASSERT_MATCH(caff_VideoFormat_Rgb565, webrtc::VideoType::kRGB565);
ASSERT_MATCH(caff_VideoFormat_Argb1555, webrtc::VideoType::kARGB1555);
ASSERT_MATCH(caff_VideoFormat_Yuy2, webrtc::VideoType::kYUY2);
ASSERT_MATCH(caff_VideoFormat_Yv12, webrtc::VideoType::kYV12);
ASSERT_MATCH(caff_VideoFormat_Uyvy, webrtc::VideoType::kUYVY);
ASSERT_MATCH(caff_VideoFormat_Mjpeg, webrtc::VideoType::kMJPEG);
ASSERT_MATCH(caff_VideoFormat_Nv21, webrtc::VideoType::kNV21);
ASSERT_MATCH(caff_VideoFormat_Nv12, webrtc::VideoType::kNV12);
ASSERT_MATCH(caff_VideoFormat_Bgra, webrtc::VideoType::kBGRA);

namespace webrtc {
    class PeerConnectionFactoryInterface;
    class MediaStreamInterface;
    class PeerConnectionInterface;
}  // namespace webrtc

namespace caff {
    class AudioDevice;
    class VideoCapturer;

    // TODO: split functionality into Stage and Feed
    class Stream {
    public:
        static int const kMaxBitrateBps = 2000000;

        Stream(
            Credentials * credentials,
            std::string username,
            std::string title,
            caff_Rating rating,
            AudioDevice* audioDevice,
            webrtc::PeerConnectionFactoryInterface* factory);

        virtual ~Stream();

        void start(
            std::function<void()> startedCallback,
            std::function<void(caff_Error)> failedCallback);

        void sendAudio(uint8_t const* samples, size_t samplesPerChannel);
        void sendVideo(uint8_t const* frameData, size_t frameBytes, int32_t width, int32_t height, caff_VideoFormat format);

        caff_ConnectionQuality getConnectionQuality();

        //void SetTitle(std::string title);
        //void SetRating(std::string rating);

    private:
        enum class State { Offline, Starting, Online, Stopping };
        std::atomic<State> state{ State::Offline };
        static char const * stateString(State state);

        Credentials * credentials; // TODO: Maybe should be owned by the Interface instead of user of libcaffeine
        std::string username;
        std::string title;
        caff_Rating rating;

        AudioDevice* audioDevice;
        VideoCapturer* videoCapturer;
        webrtc::PeerConnectionFactoryInterface* factory;
        rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection;

        std::string streamUrl;
        std::string feedId;
        optional<StageRequest> nextRequest;
        //std::atomic<bool> isMutatingFeed{ false }; // TODO: heartbeat

        bool requireState(State expectedState) const;
        bool transitionState(State oldState, State newState);
        bool isOnline() const;

        optional<std::string> offerGenerated(std::string const & offer);
        bool iceGathered(std::vector<IceInfo> candidates);
    };

}  // namespace caff
