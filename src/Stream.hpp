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

ASSERT_MATCH(caff_VideoFormatUnknown, webrtc::VideoType::kUnknown);
ASSERT_MATCH(caff_VideoFormatI420, webrtc::VideoType::kI420);
ASSERT_MATCH(caff_VideoFormatIyuv, webrtc::VideoType::kIYUV);
ASSERT_MATCH(caff_VideoFormatRgb24, webrtc::VideoType::kRGB24);
ASSERT_MATCH(caff_VideoFormatAbgr, webrtc::VideoType::kABGR);
ASSERT_MATCH(caff_VideoFormatArgb, webrtc::VideoType::kARGB);
ASSERT_MATCH(caff_VideoFormatArgb4444, webrtc::VideoType::kARGB4444);
ASSERT_MATCH(caff_VideoFormatRgb565, webrtc::VideoType::kRGB565);
ASSERT_MATCH(caff_VideoFormatArgb1555, webrtc::VideoType::kARGB1555);
ASSERT_MATCH(caff_VideoFormatYuy2, webrtc::VideoType::kYUY2);
ASSERT_MATCH(caff_VideoFormatYv12, webrtc::VideoType::kYV12);
ASSERT_MATCH(caff_VideoFormatUyvy, webrtc::VideoType::kUYVY);
ASSERT_MATCH(caff_VideoFormatMjpeg, webrtc::VideoType::kMJPEG);
ASSERT_MATCH(caff_VideoFormatNv21, webrtc::VideoType::kNV21);
ASSERT_MATCH(caff_VideoFormatNv12, webrtc::VideoType::kNV12);
ASSERT_MATCH(caff_VideoFormatBgra, webrtc::VideoType::kBGRA);

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
            SharedCredentials & sharedCredentials,
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

        SharedCredentials & sharedCredentials;
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
