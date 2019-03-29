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

    // TODO: split functionality into Stage and Feed
    class Stream {
    public:
        static int const kMaxBitrateBps = 2000000;

        Stream(
            Credentials * credentials,
            std::string username,
            std::string title,
            caff_rating rating,
            AudioDevice* audioDevice,
            webrtc::PeerConnectionFactoryInterface* factory);

        virtual ~Stream();

        void Start(
            std::function<void()> startedCallback,
            std::function<void(caff_error)> failedCallback);

        void SendAudio(uint8_t const* samples, size_t samplesPerChannel);
        void SendVideo(uint8_t const* frameData, size_t frameBytes, int32_t width, int32_t height, caff_format format);

        caff_connection_quality GetConnectionQuality();

        //void SetTitle(std::string title);
        //void SetRating(std::string rating);

    private:
        enum class State { Offline, Starting, Online, Stopping };
        std::atomic<State> state{ State::Offline };
        static char const * StateString(State state);

        Credentials * credentials; // TODO: Maybe should be owned by the Interface instead of user of libcaffeine
        std::string username;
        std::string title;
        caff_rating rating;

        AudioDevice* audioDevice;
        VideoCapturer* videoCapturer;
        webrtc::PeerConnectionFactoryInterface* factory;
        rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection;

        std::string streamUrl;
        std::string feedId;
        std::unique_ptr<StageRequest> nextRequest;
        //std::atomic<bool> isMutatingFeed{ false }; // TODO: heartbeat

        bool RequireState(State expectedState) const;
        bool TransitionState(State oldState, State newState);
        bool IsOnline() const;

        optional<std::string> OfferGenerated(std::string const & offer);
        bool IceGathered(std::vector<IceInfo> candidates);
    };

}  // namespace caff
