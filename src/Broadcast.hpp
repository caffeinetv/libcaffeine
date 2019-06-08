// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <vector>

#include "ErrorLogging.hpp"
#include "StatsObserver.hpp"
#include "WebsocketApi.hpp"

#include "absl/types/optional.h"
#include "api/video/i420_buffer.h"
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
} // namespace webrtc

namespace caff {
    class AudioDevice;
    class VideoCapturer;

    // TODO: separate Broadcast & Feed/Stream functionality
    class Broadcast : public std::enable_shared_from_this<Broadcast> {
    public:
        Broadcast(
                SharedCredentials & sharedCredentials,
                std::string username,
                std::string title,
                caff_Rating rating,
                std::string gameId,
                AudioDevice * audioDevice,
                webrtc::PeerConnectionFactoryInterface * factory);

        virtual ~Broadcast();

        void start(std::function<void()> startedCallback, std::function<void(caff_Result)> failedCallback);
        void stop();

        void setTitle(std::string title);
        void setRating(caff_Rating rating);
        void setGameId(std::string id);

        void sendAudio(uint8_t const * samples, size_t samplesPerChannel);
        void sendVideo(
                caff_VideoFormat format,
                uint8_t const * frameData,
                size_t frameBytes,
                int32_t width,
                int32_t height,
                std::chrono::microseconds timestamp);

        caff_ConnectionQuality getConnectionQuality();

    private:
        enum class State { Offline, Starting, Online, Stopping };
        std::atomic<State> state{ State::Offline };
        static char const * stateString(State state);
        std::thread broadcastThread;
        WebsocketClient websocketClient;

        std::promise<ScreenshotData> screenshotPromise;
        std::atomic<bool> isScreenshotNeeded;
        std::mutex mutex;

        std::function<void(caff_Result)> failedCallback;

        SharedCredentials & sharedCredentials;
        std::string clientId;
        std::string username;
        std::string title;
        caff_Rating rating;
        std::string gameId;
        std::string feedId;
        caff_ConnectionQuality connectionQuality = caff_ConnectionQualityUnknown;
        optional<std::string> broadcastId;
        std::string streamUrl;
        optional<WebsocketClient::Connection> stageSubscription;
        std::atomic<bool> feedHasAppearedInSubscription;

        AudioDevice * audioDevice;
        VideoCapturer * videoCapturer;
        webrtc::PeerConnectionFactoryInterface * factory;
        rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection;
        rtc::scoped_refptr<StatsObserver> statsObserver;

        bool requireState(State expectedState) const;
        bool transitionState(State oldState, State newState);
        bool isOnline() const;

        variant<std::string, caff_Result> createFeed(std::string const & offer);
        void startHeartbeat();
        ScreenshotData createScreenshot(rtc::scoped_refptr<webrtc::I420Buffer> buffer);
        caffql::FeedInput currentFeedInput();
        std::string fullTitle();
        bool updateFeed();
        bool updateTitle();
        void setupSubscription();
        void endSubscription();
    };

} // namespace caff
