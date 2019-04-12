// Copyright 2019 Caffeine Inc. All rights reserved.

#include "Broadcast.hpp"

#include <thread>
#include <ctime>
#include <random>
#include <chrono>

#include "caffeine.h"
#include "Api.hpp"
#include "AudioDevice.hpp"
#include "PeerConnectionObserver.hpp"
#include "SessionDescriptionObserver.hpp"
#include "VideoCapturer.hpp"

#include "libyuv.h"

//#define WRITE_SCREENSHOT

#ifndef WRITE_SCREENSHOT
#define STBI_WRITE_NO_STDIO
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"
#include "rtc_base/logging.h"

using namespace std::chrono_literals;

namespace caff {

    Broadcast::Broadcast(
        SharedCredentials & sharedCredentials,
        std::string username,
        std::string title,
        caff_Rating rating,
        AudioDevice* audioDevice,
        webrtc::PeerConnectionFactoryInterface* factory)
        : isScreenshotNeeded(true)
        , sharedCredentials(sharedCredentials)
        , username(std::move(username))
        , title(std::move(title))
        , rating(rating)
        , audioDevice(audioDevice)
        , factory(factory)
    {}

    Broadcast::~Broadcast() {}

    // TODO something better?
    static std::string createUniqueId()
    {
        static char const charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
        size_t const idLength = 12;

        static std::default_random_engine generator(std::time(0));
        static std::uniform_int_distribution<size_t> distribution(0, sizeof(charset) - 1);

        std::string id;

        for (size_t i = 0; i < idLength; ++i) {
            id += charset[distribution(generator)];
        }

        return id;
    }

    char const * Broadcast::stateString(Broadcast::State state)
    {
        switch (state) {
        case State::Offline:
            return "Offline";
        case State::Starting:
            return "Starting";
        case State::Online:
            return "Online";
        case State::Stopping:
            return "Stopping";
        default:
            return "Unknown";
        }
    }

    bool Broadcast::requireState(State expectedState) const
    {
        State currentState = state;
        if (currentState != expectedState) {
            RTC_LOG(LS_ERROR) << "In state " << stateString(currentState)
                << " when expecting " << stateString(expectedState);
            return false;
        }
        return true;
    }

    bool Broadcast::transitionState(State oldState, State newState)
    {
        bool result = state.compare_exchange_strong(oldState, newState);
        if (!result)
            RTC_LOG(LS_ERROR) << "Transitioning to state " << stateString(newState)
                << " expects state " << stateString(oldState);
        return result;
    }

    bool Broadcast::isOnline() const
    {
        return state == State::Online;
    }

    static std::string annotateTitle(std::string title, caff_Rating rating)
    {
        size_t const maxTitleLength = 60; // TODO: policy- should be somewhere else
        static std::string const ratingStrings[] = { "", "[17+] " }; // TODO maybe same here

        auto fullTitle = ratingStrings[rating] + title;
        if (fullTitle.length() > maxTitleLength)
            fullTitle.resize(maxTitleLength);

        return fullTitle;
    }

    variant<std::string, caff_Result> Broadcast::createFeed(std::string const & offer)
    {
        feedId = createUniqueId();

        if (!requireState(State::Starting)) {
            return caff_ResultBroadcastFailed;
        }

        auto fullTitle = annotateTitle(title, rating);
        auto clientId = createUniqueId();

        // Make initial stage request to get a cursor

        StageRequest request(username, clientId);

        if (!requestStageUpdate(request, sharedCredentials, NULL, NULL)) {
            return caff_ResultBroadcastFailed;
        }

        // Make request to add our feed and get a new broadcast id
        request.stage->title = std::move(fullTitle);
        request.stage->upsertBroadcast = true;
        request.stage->broadcastId.reset();
        request.stage->live = false;

        FeedStream stream{};
        stream.sdpOffer = offer;

        Feed feed{};
        feed.id = feedId;
        feed.clientId = clientId;
        feed.role = "primary";
        feed.volume = 1.0;
        feed.capabilities = { true, true };
        feed.stream = std::move(stream);

        request.stage->feeds = { {feedId, std::move(feed)} };

        bool isOutOfCapacity = false;
        if (!requestStageUpdate(request, sharedCredentials, NULL, &isOutOfCapacity)) {
            if (isOutOfCapacity) {
                return caff_ResultOutOfCapacity;
            }
            else {
                return caff_ResultBroadcastFailed;
            }
        }

        // Get broadcast details
        auto feedIt = request.stage->feeds->find(feedId);
        if (feedIt == request.stage->feeds->end()) {
            return caff_ResultBroadcastFailed;
        }

        auto & responseStream = feedIt->second.stream;

        if (!responseStream
            || !responseStream->sdpAnswer
            || responseStream->sdpAnswer->empty()
            || !responseStream->url
            || responseStream->url->empty()) {
            return caff_ResultRequestFailed;
        }

        streamUrl = *responseStream->url;
        nextRequest = std::move(request);

        return *responseStream->sdpAnswer;
    }

    /*

    // Falls back to obs_id if no foreground game detected
    static char const * get_running_game_id(
        caff_GameList * games, const char * fallback_id)
    {
        char * foreground_process = get_foreground_process_name();
        char const * id = get_game_id(games, foreground_process);
        bfree(foreground_process);
        return id ? id : fallback_id;
    }

    // Returns `true` if the feed's game id changed
    static bool caffeine_update_game_id(char const * game_id, Feed * feed)
    {
        if (!feed) {
            return false;
        }

        bool did_change = false;

        if (game_id) {
            if (!feed->content.id || strcmp(feed->content.id, game_id) != 0) {
                caff_set_string(&feed->content.id, game_id);
                did_change = true;
            }

            if (!feed->content.type) {
                caff_set_string(&feed->content.type, "game");
                did_change = true;
            }
        } else if (feed->content.id || feed->content.type) {
            caff_set_string(&feed->content.id, NULL);
            caff_set_string(&feed->content.type, NULL);
            did_change = true;
            }

        return did_change;
    }

    // Returns `true` if the feed's connection quality changed
    static bool caffeine_update_connection_quality(
        char const * quality, Feed * feed)
    {
        if (!quality) {
            return false;
        }

        if (!feed->sourceConnectionQuality
            || strcmp(quality, feed->sourceConnectionQuality) != 0)
        {
            caff_set_string(&feed->sourceConnectionQuality, quality);
            return true;
        }

        return false;
    }
    */

    void Broadcast::start(
        std::function<void()> startedCallback,
        std::function<void(caff_Result)> failedCallback)
    {
        this->failedCallback = failedCallback;
        transitionState(State::Offline, State::Starting);

        broadcastThread = std::thread([=] {
            videoCapturer = new VideoCapturer;
            auto videoSource = factory->CreateVideoSource(videoCapturer);
            auto videoTrack = factory->CreateVideoTrack("external_video", videoSource);

            cricket::AudioOptions audioOptions;
            audioOptions.echo_cancellation = false;
            audioOptions.noise_suppression = false;
            audioOptions.highpass_filter = false;
            audioOptions.stereo_swapping = false;
            audioOptions.typing_detection = false;
            audioOptions.aecm_generate_comfort_noise = false;
            audioOptions.experimental_agc = false;
            audioOptions.extended_filter_aec = false;
            audioOptions.delay_agnostic_aec = false;
            audioOptions.experimental_ns = false;
            audioOptions.intelligibility_enhancer = false;
            audioOptions.residual_echo_detector = false;
            audioOptions.tx_agc_limiter = false;

            auto audioSource = factory->CreateAudioSource(audioOptions);
            auto audioTrack = factory->CreateAudioTrack("external_audio", audioSource);

            auto mediaStream = factory->CreateLocalMediaStream("caffeine_stream");
            mediaStream->AddTrack(videoTrack);
            mediaStream->AddTrack(audioTrack);

            webrtc::PeerConnectionInterface::RTCConfiguration config;
            auto observer = new PeerConnectionObserver;
            peerConnection = factory->CreatePeerConnection(
                config, webrtc::PeerConnectionDependencies(observer));

            peerConnection->AddStream(mediaStream);

            webrtc::BitrateSettings bitrateOptions;
            bitrateOptions.start_bitrate_bps = kMaxBitrateBps;
            bitrateOptions.max_bitrate_bps = kMaxBitrateBps;
            peerConnection->SetBitrate(bitrateOptions);

            rtc::scoped_refptr<CreateSessionDescriptionObserver> creationObserver =
                new rtc::RefCountedObject<CreateSessionDescriptionObserver>;
            webrtc::PeerConnectionInterface::RTCOfferAnswerOptions answerOptions;
            peerConnection->CreateOffer(creationObserver, answerOptions);

            auto offer = creationObserver->getFuture().get();
            if (!offer) {
                // Logged by the observer
                failedCallback(caff_ResultRequestFailed);
                state.store(State::Offline);
                return;
            }

            if (offer->type() != webrtc::SessionDescriptionInterface::kOffer) {
                RTC_LOG(LS_ERROR)
                    << "Expected " << webrtc::SessionDescriptionInterface::kOffer
                    << " but got " << offer->type();
                failedCallback(caff_ResultRequestFailed);
                return;
            }

            std::string offerSdp;
            if (!offer->ToString(&offerSdp)) {
                RTC_LOG(LS_ERROR) << "Error serializing SDP offer";
                failedCallback(caff_ResultRequestFailed);
                return;
            }

            webrtc::SdpParseError offerError;
            auto localDesc = webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, offerSdp, &offerError);
            if (!localDesc) {
                RTC_LOG(LS_ERROR) << "Error parsing SDP offer: " << offerError.description;
                failedCallback(caff_ResultRequestFailed);
                return;
            }

            rtc::scoped_refptr<SetSessionDescriptionObserver> setLocalObserver =
                new rtc::RefCountedObject<SetSessionDescriptionObserver>;

            peerConnection->SetLocalDescription(setLocalObserver, localDesc.release());
            auto setLocalSuccess = setLocalObserver->getFuture().get();
            if (!setLocalSuccess) {
                failedCallback(caff_ResultRequestFailed);
                return;
            }

            auto result = createFeed(offerSdp);
            auto error = get_if<caff_Result>(&result);
            if (error) {
                RTC_LOG(LS_ERROR) << "Failed to create feed";
                failedCallback(*error);
                return;
            }

            webrtc::SdpParseError answerError;
            auto remoteDesc = webrtc::CreateSessionDescription(
                webrtc::SdpType::kAnswer,
                get<std::string>(result),
                &answerError);
            if (!remoteDesc) {
                RTC_LOG(LS_ERROR) << "Error parsing SDP answer: " << answerError.description;
                failedCallback(caff_ResultRequestFailed);
                return;
            }

            auto & candidates = observer->getFuture().get();
            if (!trickleCandidates(candidates, streamUrl, sharedCredentials)) {
                RTC_LOG(LS_ERROR) << "Failed to negotiate ICE";
                failedCallback(caff_ResultRequestFailed);
                return;
            }

            rtc::scoped_refptr<SetSessionDescriptionObserver> setRemoteObserver =
                new rtc::RefCountedObject<SetSessionDescriptionObserver>;

            peerConnection->SetRemoteDescription(setRemoteObserver, remoteDesc.release());
            auto setRemoteSuccess = setRemoteObserver->getFuture().get();
            if (!setRemoteSuccess) {
                // Logged by the observer
                failedCallback(caff_ResultRequestFailed);
                return;
            }

            state.store(State::Online);
            startedCallback();

            startHeartbeat();

            if (longpollThread.joinable()) {
                longpollThread.join();
            }
        });
    }

    /*
    static char const * getGameId(caff_GameList * games, char const * const processName)
    {
        if (games && processName) {
            for (size_t gameIndex = 0; gameIndex < games->numGames; ++gameIndex) {
                caff_GameInfo * info = games->gameInfos[gameIndex];
                if (!info) {
                    continue;
                }
                for (size_t processNameIndex = 0; processNameIndex < info->numProcessNames; ++processNameIndex) {
                    char const * currentName = info->processNames[processNameIndex];
                    if (!currentName) {
                        continue;
                    }
                    if (strcmp(processName, currentName) == 0) {
                        return info->id;
                    }
                }
            }
        }

        return NULL;
    }
    */

    void Broadcast::startHeartbeat()
    {
        if (!requireState(State::Online)) {
            return;
        }

        optional<StageRequest> request{};
        {
            std::lock_guard<std::mutex> lock(mutex);
            std::swap(request, nextRequest);
        }

        //caff_GameList * games = caff_getGameList();
        //std::string obsGameId = getGameId(games, "obs");
        //caff_freeGameList(&games);

        // Obtain broadcast id

        auto broadcastId = request->stage->broadcastId;

        for (int broadcastIdRetryCount = 0; !broadcastId && broadcastIdRetryCount < 3; ++broadcastIdRetryCount) {
            request->stage->upsertBroadcast = true;
            if (!requestStageUpdate(*request, sharedCredentials, NULL, NULL)
                || !request->stage->feeds
                || request->stage->feeds->find(feedId) == request->stage->feeds->end())
            {
                failedCallback(caff_ResultBroadcastFailed);
                return;
            }

            broadcastId = request->stage->broadcastId;
        }

        if (!broadcastId) {
            failedCallback(caff_ResultBroadcastFailed);
            return;
        }

        auto screenshotFuture = screenshotPromise.get_future();
        try {
            auto screenshotData = screenshotFuture.get();
            if (!updateScreenshot(*broadcastId, screenshotData, sharedCredentials)) {
                RTC_LOG(LS_ERROR) << "Failed to send screenshot";
                failedCallback(caff_ResultBroadcastFailed);
                return;
            }
        }
        catch (...) {
            // Already logged
            failedCallback(caff_ResultBroadcastFailed);
            return;
        }

        // Set stage live with current game content

        //caffeine_update_game_id(
        //    get_running_game_id(games, obs_game_id),
        //    caff_get_stage_feed(request->stage, feedId));
        request->stage->live = true;

        if (!requestStageUpdate(*request, sharedCredentials, NULL, NULL)
            || !request->stage->live
            || !request->stage->feeds
            || request->stage->feeds->find(feedId) == request->stage->feeds->end())
        {
            failedCallback(caff_ResultBroadcastFailed);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            std::swap(request, nextRequest);
        }

        startLongpollThread();

        auto constexpr heartbeatInterval = 5000ms;
        auto constexpr checkInterval = 100ms;

        auto interval = heartbeatInterval;

        int const max_failures = 5;
        int failures = 0;

        for (; state == State::Online; std::this_thread::sleep_for(checkInterval))
        {
            // TODO: use wall time?
            interval += checkInterval;
            if (interval < heartbeatInterval)
                continue;

            interval = 0ms;

            {
                std::lock_guard<std::mutex> lock(mutex);
                request = nextRequest;
            }

            if (!request
                || !request->stage
                || !request->stage->feeds) {
                failedCallback(caff_ResultBroadcastFailed);
                return;
            }

            auto feedIt = request->stage->feeds->find(feedId);
            if (feedIt == request->stage->feeds->end()) {
                failedCallback(caff_ResultTakeover);
                return;
            }

            bool shouldMutateFeed = false;

            // Heartbeat broadcast

            auto heartbeatResponse = heartbeatStream(streamUrl, sharedCredentials);

            auto & feed = feedIt->second;
            if (heartbeatResponse) {
                if (feed.sourceConnectionQuality != heartbeatResponse->connectionQuality) {
                    shouldMutateFeed = true;
                    feed.sourceConnectionQuality = heartbeatResponse->connectionQuality;
                }
                failures = 0;
            }
            else {
                RTC_LOG(LS_ERROR) << "Heartbeat failed";
                ++failures;
                if (failures > max_failures) {
                    RTC_LOG(LS_ERROR) << "Heartbeat failed " << failures << "times; ending broadcast.";
                    failedCallback(caff_ResultDisconnected);
                    break;
                }
            }

            //shouldMutateFeed =
            //    caffeine_update_game_id(
            //        get_running_game_id(games, obs_game_id), feed)
            //    || shouldMutateFeed;

            if (!shouldMutateFeed) {
                continue;
            }

            // Mutate the feed

            isMutatingFeed = true;

            if (!requestStageUpdate(*request, sharedCredentials, NULL, NULL)) {
                isMutatingFeed = false;
                // If we have a broadcast going but can't talk to
                // the stage endpoint, retry the mutation next loop
                continue;
            }

            if (!request->stage->live
                || !request->stage->feeds
                || request->stage->feeds->find(feedId) == request->stage->feeds->end())
            {
                failedCallback(caff_ResultTakeover);
                return;
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                std::swap(request, nextRequest);
            }
            isMutatingFeed = false;
        }

        isMutatingFeed = true;

        {
            std::lock_guard<std::mutex> lock(mutex);
            request.reset();
            std::swap(request, nextRequest);
        }

        // Only set the stage offline if it contains our feed
        if (request && request->stage && request->stage->feeds) {
            auto feedIt = request->stage->feeds->find(feedId);
            if (feedIt != request->stage->feeds->end()) {
                request->stage->live = false;
                request->stage->feeds->clear();

                // If this fails, the broadcast is either offline or failing anyway. Ignore result
                requestStageUpdate(*request, sharedCredentials, NULL, NULL);
            }
        }
    }

    void Broadcast::startLongpollThread()
    {
        longpollThread = std::thread([=] {
            // This thread is purely for hearbeating our feed.
            // If the broadcast thread is making a mutation, this thread waits.
            auto retryInterval = 0ms;
            auto const checkInterval = 100ms;

            auto interval = retryInterval;

            for (; state == State::Online; std::this_thread::sleep_for(checkInterval))
            {
                interval += checkInterval;
                if (interval < retryInterval || isMutatingFeed)
                    continue;

                optional<StageRequest> request{};
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    request = nextRequest;
                }

                if (!request) {
                    break;
                }

                auto retryIn = 0ms;

                if (!requestStageUpdate(*request, sharedCredentials, &retryIn, NULL)) {
                    //If we have a broadcast going but can't talk to the stage endpoint,
                    // just continually retry with some waiting
                    retryInterval = 5000ms;
                    continue;
                }

                bool isLiveFeedPresent =
                    request->stage->live
                    && request->stage->feeds
                    && request->stage->feeds->find(feedId) != request->stage->feeds->end();

                {
                    std::lock_guard<std::mutex> lock(mutex);
                    std::swap(nextRequest, request);
                }

                if (!isLiveFeedPresent) {
                    break;
                }

                interval = 0ms;
                retryInterval = retryIn;
            }
        });
    }

    void Broadcast::stop()
    {
        state = State::Stopping;
        if (broadcastThread.joinable()) {
            broadcastThread.join();
        }
        state = State::Offline;
    }

    void Broadcast::sendAudio(uint8_t const* samples, size_t samplesPerChannel)
    {
        if (isOnline()) {
            audioDevice->sendAudio(samples, samplesPerChannel);
        }
    }

    void Broadcast::sendVideo(uint8_t const* frameData,
        size_t frameBytes,
        int32_t width,
        int32_t height,
        caff_VideoFormat format)
    {
        if (isOnline()) {
            auto rtcFormat = static_cast<webrtc::VideoType>(format);
            auto i420frame = videoCapturer->sendVideo(frameData, frameBytes, width, height, rtcFormat);

            bool expected = true;
            if (isScreenshotNeeded.compare_exchange_strong(expected, false)) {
                try {
                    screenshotPromise.set_value(createScreenshot(i420frame));
                }
                catch (std::exception ex) {
                    RTC_LOG(LS_ERROR) << "Failed to create screenshot: " << ex.what();
                    screenshotPromise.set_exception(std::current_exception());
                }
                catch (...) {
                    RTC_LOG(LS_ERROR) << "Failed to create screenshot: (unknown exception)";
                    screenshotPromise.set_exception(std::current_exception());
                }
            }
        }
    }

    static void writeFunc(void * context, void * data, int size)
    {
        auto screenshot = reinterpret_cast<ScreenshotData *>(context);
        auto pixels = reinterpret_cast<uint8_t *>(data);
        screenshot->insert(screenshot->end(), pixels, pixels + size);
    }

    ScreenshotData Broadcast::createScreenshot(rtc::scoped_refptr<webrtc::I420Buffer> buffer)
    {
        auto const width = buffer->width();
        auto const height = buffer->height();
        auto constexpr channels = 3;
        auto const destStride = width * channels;

        std::vector<uint8_t> raw;
        raw.resize(destStride * height);

        auto ret = libyuv::I420ToRAW(
            buffer->DataY(),
            buffer->StrideY(),
            buffer->DataU(),
            buffer->StrideU(),
            buffer->DataV(),
            buffer->StrideV(),
            &raw[0],
            destStride,
            width,
            height);
        if (ret != 0) {
            throw std::exception("Failed to convert I420 to RAW");
        }
#ifdef WRITE_SCREENSHOT
        ret = stbi_write_png(R"(screenshot.png)", width, height, channels, &raw[0], destStride);
        ret = stbi_write_jpg(R"(screenshot.jpg)", width, height, channels, &raw[0], 95);
#endif
        ScreenshotData screenshot;
        ret = stbi_write_jpg_to_func( writeFunc, &screenshot, width, height, channels, &raw[0], 95);
        if (ret == 0) {
            throw std::exception("Failed to convert RAW to JPEG");
        }
        return screenshot;
    }

    caff_ConnectionQuality Broadcast::getConnectionQuality() const
    {
        if (nextRequest && nextRequest->stage->feeds) {
            auto feedIt = nextRequest->stage->feeds->find(feedId);
            if (feedIt != nextRequest->stage->feeds->end() && feedIt->second.sourceConnectionQuality) {
                return *feedIt->second.sourceConnectionQuality;
            }
        }

        return caff_ConnectionQualityUnknown;
    }

}  // namespace caff
