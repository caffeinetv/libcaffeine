// Copyright 2019 Caffeine Inc. All rights reserved.

#include "Broadcast.hpp"

#include <chrono>
#include <ctime>
#include <random>
#include <thread>

#include "AudioDevice.hpp"
#include "PeerConnectionObserver.hpp"
#include "Policy.hpp"
#include "RestApi.hpp"
#include "SessionDescriptionObserver.hpp"
#include "Utils.hpp"
#include "VideoCapturer.hpp"
#include "caffeine.h"

#include "libyuv.h"

// Uncomment this to save png & jpg copies of the screenshot to the working directory
//#define SAVE_SCREENSHOT

#ifndef SAVE_SCREENSHOT
#    define STBI_WRITE_NO_STDIO
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"

using namespace std::chrono_literals;

namespace caff {

    Broadcast::Broadcast(
            SharedCredentials & sharedCredentials,
            std::string username,
            std::string title,
            caff_Rating rating,
            std::string gameId,
            AudioDevice * audioDevice,
            webrtc::PeerConnectionFactoryInterface * factory)
        : isScreenshotNeeded(true)
        , sharedCredentials(sharedCredentials)
        , clientId(rtc::CreateRandomUuid())
        , username(std::move(username))
        , title(std::move(title))
        , rating(rating)
        , gameId(gameId)
        , feedId(rtc::CreateRandomUuid())
        , audioDevice(audioDevice)
        , factory(factory) {}

    Broadcast::~Broadcast() {}

    char const * Broadcast::stateString(Broadcast::State state) {
        switch (state) {
        case State::Offline:
            return "Offline";
        case State::Starting:
            return "Starting";
        case State::Streaming:
            return "Streaming";
        case State::Live:
            return "Live";
        case State::Stopping:
            return "Stopping";
        default:
            return "Unknown";
        }
    }

    bool Broadcast::requireState(State expectedState) const {
        State currentState = state;
        if (currentState != expectedState) {
            LOG_ERROR("In state %s when expecting %s", stateString(currentState), stateString(expectedState));
            return false;
        }
        return true;
    }

    bool Broadcast::transitionState(State const expected, State newState) {
        State oldState = expected;
        bool result = state.compare_exchange_strong(oldState, newState);
        if (!result)
            LOG_ERROR(
                    "Transitioning to state %s expects state %s but was in state %s",
                    stateString(newState),
                    stateString(expected),
                    stateString(oldState));
        return result;
    }

    bool Broadcast::isOnline() const {
        switch (state) {
        case State::Offline:
        case State::Starting:
        case State::Stopping:
            return false;
        case State::Streaming:
        case State::Live:
            return true;
        }
    }

    variant<std::string, caff_Result> Broadcast::createFeed(std::string const & offer) {
        if (!requireState(State::Starting)) {
            return caff_ResultBroadcastFailed;
        }

        LOG_DEBUG("Creating feed: %s", feedId.c_str());
        auto feed = currentFeedInput();
        feed.sdpOffer = offer;

        auto payload = graphqlRequest<caffql::Mutation::AddFeedField>(
                sharedCredentials, clientId, caffql::ClientType::Capture, feed);
        if (!payload) {
            LOG_ERROR("Request failed adding feed");
            return caff_ResultBroadcastFailed;
        }

        if (payload->error) {
            if (holds_alternative<caffql::OutOfCapacityError>(payload->error->implementation)) {
                return caff_ResultOutOfCapacity;
            } else {
                LOG_ERROR("Error adding feed: %s", payload->error->message().c_str());
                return caff_ResultBroadcastFailed;
            }
        }

        if (payload->stage.feeds.empty() || payload->stage.feeds[0].id != feedId) {
            LOG_ERROR("Expected feed in AddFeedPayload");
        }

        if (!payload->stage.broadcastId) {
            LOG_ERROR("Expected broadcast id in AddFeedPayload");
            return caff_ResultBroadcastFailed;
        }

        broadcastId = payload->stage.broadcastId;

        auto stream = get_if<caffql::BroadcasterStream>(&payload->feed.stream.implementation);
        if (!stream) {
            LOG_ERROR("Expected broadcaster stream in AddFeedPayload");
            return caff_ResultBroadcastFailed;
        }

        streamUrl = stream->url;
        LOG_DEBUG("Feed added to stage");

        return stream->sdpAnswer;
    }

    void Broadcast::start(std::function<void()> startedCallback, std::function<void(caff_Result)> failedCallback) {
        this->failedCallback = failedCallback;
        transitionState(State::Offline, State::Starting);

        auto info = getEncoderInfo(sharedCredentials);
        int targetBitrate = info.has_value() && info->setting.targetBitrate > 0 ? info->setting.targetBitrate : maxBitsPerSecond;
        LOG_DEBUG("Setting target bitrate: %d", targetBitrate);

        broadcastThread = std::thread([=] {
            setupSubscription();

            LOG_DEBUG("Creating video track");
            videoCapturer = new VideoCapturer;
            auto videoSource = factory->CreateVideoSource(videoCapturer);
            auto videoTrack = factory->CreateVideoTrack("external_video", videoSource);

            LOG_DEBUG("Creating audio track");
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

            LOG_DEBUG("Creating peer connection");
            webrtc::PeerConnectionInterface::RTCConfiguration config;
            auto observer = new PeerConnectionObserver(failedCallback);
            peerConnection = factory->CreatePeerConnection(config, webrtc::PeerConnectionDependencies(observer));

            peerConnection->AddStream(mediaStream);

            webrtc::BitrateSettings bitrateOptions;
            bitrateOptions.start_bitrate_bps = targetBitrate;
            bitrateOptions.max_bitrate_bps = targetBitrate;
            bitrateOptions.min_bitrate_bps = targetBitrate;
            peerConnection->SetBitrate(bitrateOptions);

            rtc::scoped_refptr<CreateSessionDescriptionObserver> creationObserver =
                    new rtc::RefCountedObject<CreateSessionDescriptionObserver>;
            webrtc::PeerConnectionInterface::RTCOfferAnswerOptions answerOptions;
            peerConnection->CreateOffer(creationObserver, answerOptions);

            std::future_status status;

            LOG_DEBUG("Awaiting SDP offer");
            auto constexpr futureWait = 1s;
            auto creationFuture = creationObserver->getFuture();
            status = creationFuture.wait_for(futureWait);
            if (status == std::future_status::timeout) {
                LOG_ERROR("Timeout Error: creationObserver");
                failedCallback(caff_ResultFailure);
                return;
            }

            auto offer = creationFuture.get();
            if (!offer) {
                // Logged by the observer
                failedCallback(caff_ResultFailure);
                return;
            }

            if (offer->type() != webrtc::SessionDescriptionInterface::kOffer) {
                LOG_ERROR("Expected %s but got %s", webrtc::SessionDescriptionInterface::kOffer, offer->type().c_str());
                failedCallback(caff_ResultFailure);
                return;
            }

            std::string offerSdp;
            if (!offer->ToString(&offerSdp)) {
                LOG_ERROR("Error serializing SDP offer");
                failedCallback(caff_ResultFailure);
                return;
            }

            LOG_DEBUG("Creating local session description");
            webrtc::SdpParseError offerError;
            auto localDesc = webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, offerSdp, &offerError);
            if (!localDesc) {
                LOG_ERROR("Error parsing SDP offer: %s", offerError.description.c_str());
                failedCallback(caff_ResultFailure);
                return;
            }

            rtc::scoped_refptr<SetSessionDescriptionObserver> setLocalObserver =
                    new rtc::RefCountedObject<SetSessionDescriptionObserver>;

            peerConnection->SetLocalDescription(setLocalObserver, localDesc.release());

            auto setLocalFuture = setLocalObserver->getFuture();
            status = setLocalFuture.wait_for(futureWait);
            if (status == std::future_status::timeout) {
                LOG_ERROR("Timeout Error: setLocalObserver");
                failedCallback(caff_ResultFailure);
                return;
            }

            auto setLocalSuccess = setLocalFuture.get();
            if (!setLocalSuccess) {
                failedCallback(caff_ResultFailure);
                return;
            }

            LOG_DEBUG("Setting RTP sender min/max bitrates");
            auto senders = peerConnection->GetSenders();
            if (senders.size() == 0) {
                LOG_WARNING("No RTP senders are setup");
            } else {
                for (auto sender : senders) {
                    if (cricket::MEDIA_TYPE_VIDEO == sender->media_type()) {
                        webrtc::RtpParameters params = sender->GetParameters();

                        if (params.encodings.size() > 0) {
                            params.encodings[0].max_bitrate_bps = params.encodings[0].min_bitrate_bps = targetBitrate;
                            LOG_DEBUG("Setting video RTP sender min/max bitrate to target bitrate: %d", targetBitrate);
                        }

                        webrtc::RTCError ret = sender->SetParameters(params);
                        if (!ret.ok()) {
                            LOG_ERROR("Failed to set rtp parameters: %s", ret.message());
                        }
                    }
                }
            }


            LOG_DEBUG("Creating feed");
            auto result = createFeed(offerSdp);
            auto error = get_if<caff_Result>(&result);
            if (error) {
                LOG_ERROR("Failed to create feed");
                failedCallback(*error);
                return;
            }

            LOG_DEBUG("Creating remote session description");
            webrtc::SdpParseError answerError;
            auto remoteDesc =
                    webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, get<std::string>(result), &answerError);
            if (!remoteDesc) {
                LOG_ERROR("Error parsing SDP answer: %s", answerError.description.c_str());
                failedCallback(caff_ResultFailure);
                return;
            }

            auto observerFuture = observer->getFuture();
            status = observerFuture.wait_for(futureWait);
            if (status == std::future_status::timeout) {
                LOG_ERROR("Timeout Error: observer");
                failedCallback(caff_ResultFailure);
                return;
            }

            LOG_DEBUG("Trickling ICE candidates");
            auto & candidates = observerFuture.get();
            if (!trickleCandidates(candidates, streamUrl, sharedCredentials)) {
                LOG_ERROR("Failed to negotiate ICE");
                failedCallback(caff_ResultFailure);
                return;
            }

            LOG_DEBUG("Setting remote session description");
            rtc::scoped_refptr<SetSessionDescriptionObserver> setRemoteObserver =
                    new rtc::RefCountedObject<SetSessionDescriptionObserver>;

            peerConnection->SetRemoteDescription(setRemoteObserver, remoteDesc.release());

            auto setRemoteFuture = setRemoteObserver->getFuture();
            status = setRemoteFuture.wait_for(futureWait);
            if (status == std::future_status::timeout) {
                LOG_ERROR("Timeout Error: setRemoteObserver");
                failedCallback(caff_ResultFailure);
                return;
            }

            auto setRemoteSuccess = setRemoteFuture.get();
            if (!setRemoteSuccess) {
                // Logged by the observer
                failedCallback(caff_ResultFailure);
                return;
            }

            LOG_DEBUG("Adding stats observer");
            statsObserver = new rtc::RefCountedObject<StatsObserver>(sharedCredentials);

            if (!transitionState(State::Starting, State::Streaming)) {
                return;
            }
            startedCallback();

            startHeartbeat();

            statsObserver.release()->Release();
        });
    }

    void Broadcast::setTitle(std::string title) {
        bool titleChanged = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (title != this->title) {
                this->title = title;
                titleChanged = true;
            }
        }

        if (titleChanged && state == State::Live) {
            updateTitle();
        }
    }

    void Broadcast::setRating(caff_Rating rating) {
        bool ratingChanged = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (rating != this->rating) {
                this->rating = rating;
                ratingChanged = true;
            }
        }

        if (ratingChanged && state == State::Live) {
            updateTitle();
        }
    }

    void Broadcast::setGameId(std::string id) {
        bool gameIdChanged = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (this->gameId != id) {
                this->gameId = id;
                gameIdChanged = true;
            }
        }

        if (gameIdChanged && isOnline()) {
            updateFeed();
        }
    }

    void Broadcast::startHeartbeat() {
        if (!requireState(State::Streaming)) {
            return;
        }

        LOG_DEBUG("Awaiting screenshot");
        std::future_status status;
        auto screenshotFuture = screenshotPromise.get_future();
        try {
            // Since we have to wait for the application to provide the first frame, the timeout is generous
            auto constexpr screenshotWait = 5s;
            status = screenshotFuture.wait_for(screenshotWait);
            if (status == std::future_status::timeout) {
                LOG_ERROR("No video for screenshot after %lld seconds", screenshotWait.count());
                failedCallback(caff_ResultBroadcastFailed);
                return;
            }

            LOG_DEBUG("Sending screenshot");
            auto screenshotData = screenshotFuture.get();
            if (!updateScreenshot(broadcastId.value(), screenshotData, sharedCredentials)) {
                LOG_ERROR("Failed to send screenshot");
                failedCallback(caff_ResultBroadcastFailed);
                return;
            }
        } catch (...) {
            // Already logged
            failedCallback(caff_ResultBroadcastFailed);
            return;
        }

        LOG_DEBUG("Awaiting stage subscription");
        // Make sure the subscription has opened before going live
        {
            auto openedFuture = subscriptionOpened.get_future();
            auto status = openedFuture.wait_for(5s);
            if (status != std::future_status::ready) {
                LOG_ERROR("Timed out waiting for stage subscription open");
                failedCallback(caff_ResultBroadcastFailed);
                return;
            }
        }

        // Set stage live
        LOG_DEBUG("Setting stage live");
        auto startPayload = graphqlRequest<caffql::Mutation::StartBroadcastField>(
                sharedCredentials, clientId, caffql::ClientType::Capture, fullTitle());
        if (!startPayload || startPayload->error || !startPayload->stage.live) {
            if (startPayload) {
                LOG_ERROR("Error starting broadcast: %s", startPayload->error->message().c_str());
            }
            failedCallback(caff_ResultBroadcastFailed);
        }

        if (!transitionState(State::Streaming, State::Live)) {
            graphqlRequest<caffql::Mutation::StopBroadcastField>(sharedCredentials, clientId, nullopt);
            return;
        }

        auto constexpr heartbeatInterval = 5000ms;
        auto constexpr checkInterval = 100ms;

        auto interval = heartbeatInterval;

        int const max_failures = 5;
        int failures = 0;

        LOG_DEBUG("Starting heartbeats");
        for (; state == State::Live; std::this_thread::sleep_for(checkInterval)) {
            // TODO: use wall time?
            interval += checkInterval;
            if (interval < heartbeatInterval)
                continue;

            interval = 0ms;

            LOG_DEBUG("Updating webrtc stats");
            peerConnection->GetStats(
                    statsObserver,
                    nullptr,
                    webrtc::PeerConnectionInterface::StatsOutputLevel::kStatsOutputLevelStandard);

            // Heartbeat broadcast
            auto heartbeatResponse = heartbeatStream(streamUrl, sharedCredentials);

            if (heartbeatResponse) {
                LOG_DEBUG("Heartbeat succeeded");
                failures = 0;
                // Update the feed's connection quality if it has changed
                bool shouldMutateFeed = false;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    if (heartbeatResponse->connectionQuality != connectionQuality) {
                        connectionQuality = heartbeatResponse->connectionQuality;
                        shouldMutateFeed = true;
                    }
                }

                if (shouldMutateFeed) {
                    if (updateFeed()) {
                        LOG_DEBUG("Updated feed connection quality");
                    } else {
                        LOG_DEBUG("Failed to update feed");
                        failedCallback(caff_ResultBroadcastFailed);
                        return;
                    }
                }
            } else {
                LOG_ERROR("Heartbeat failed");
                ++failures;
                if (failures > max_failures) {
                    LOG_ERROR("Heartbeat failed %d times; ending broadcast.", failures);
                    failedCallback(caff_ResultDisconnected);
                    break;
                }
            }
        }

        if (state == State::Stopping) {
            graphqlRequest<caffql::Mutation::StopBroadcastField>(sharedCredentials, clientId, nullopt);
        }
    }

    void Broadcast::stop() {
        state = State::Stopping;
        subscription = nullptr;
        if (broadcastThread.joinable()) {
            broadcastThread.join();
        }
        state = State::Offline;
    }

    void Broadcast::sendAudio(uint8_t const * samples, size_t samplesPerChannel) {
        if (isOnline()) {
            audioDevice->sendAudio(samples, samplesPerChannel);
        }
    }

    void Broadcast::sendVideo(
            caff_VideoFormat format,
            uint8_t const * frameData,
            size_t frameBytes,
            int32_t width,
            int32_t height,
            std::chrono::microseconds timestamp) {
        if (!isOnline()) {
            return;
        }
        if (auto result = checkAspectRatio(width, height)) {
            failedCallback(result);
            return;
        }
        auto rtcFormat = static_cast<webrtc::VideoType>(format);
        auto i420frame = videoCapturer->sendVideo(rtcFormat, frameData, frameBytes, width, height, timestamp);

        bool expected = true;
        if (i420frame && isScreenshotNeeded.compare_exchange_strong(expected, false)) {
            try {
                screenshotPromise.set_value(createScreenshot(i420frame));
                LOG_DEBUG("Screenshot promise set");
            } catch (std::exception ex) {
                LOG_ERROR("Failed to create screenshot: %s", ex.what());
                screenshotPromise.set_exception(std::current_exception());
            } catch (...) {
                LOG_ERROR("Failed to create screenshot");
                screenshotPromise.set_exception(std::current_exception());
            }
        }
    }

    static void writeScreenshot(void * context, void * data, int size) {
        auto screenshot = reinterpret_cast<ScreenshotData *>(context);
        auto pixels = reinterpret_cast<uint8_t *>(data);
        screenshot->insert(screenshot->end(), pixels, pixels + size);
    }

    ScreenshotData Broadcast::createScreenshot(rtc::scoped_refptr<webrtc::I420Buffer> buffer) {
        if (!buffer) {
            throw std::runtime_error("No buffer for screenshot");
        }
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
            throw std::runtime_error("Failed to convert I420 to RAW");
        }
#ifdef SAVE_SCREENSHOT
        ret = stbi_write_png("screenshot.png", width, height, channels, &raw[0], destStride);
        ret = stbi_write_jpg("screenshot.jpg", width, height, channels, &raw[0], 95);
#endif
        ScreenshotData screenshot;
        ret = stbi_write_jpg_to_func(writeScreenshot, &screenshot, width, height, channels, &raw[0], 95);
        if (ret == 0) {
            throw std::runtime_error("Failed to convert RAW to JPEG");
        }
        return screenshot;
    }

    caff_ConnectionQuality Broadcast::getConnectionQuality() {
        std::lock_guard<std::mutex> lock(mutex);
        return connectionQuality;
    }

    caffql::FeedInput Broadcast::currentFeedInput() {
        std::lock_guard<std::mutex> lock(mutex);
        caffql::FeedInput feed{};
        feed.id = feedId;
        feed.game = caffql::GameInput{};
        if (!gameId.empty()) {
            feed.game->id = gameId;
        }
        switch (connectionQuality) {
        case caff_ConnectionQualityGood:
            feed.sourceConnectionQuality = caffql::SourceConnectionQuality::Good;
            break;
        case caff_ConnectionQualityPoor:
            feed.sourceConnectionQuality = caffql::SourceConnectionQuality::Poor;
            break;
        case caff_ConnectionQualityUnknown:
            break;
        }
        return feed;
    }

    std::string Broadcast::fullTitle() {
        std::lock_guard<std::mutex> lock(mutex);
        return annotateTitle(title, rating);
    }

    bool Broadcast::updateFeed() {
        auto feed = currentFeedInput();
        auto payload = graphqlRequest<caffql::Mutation::UpdateFeedField>(
                sharedCredentials, clientId, caffql::ClientType::Capture, feed);
        if (payload && payload->error) {
            LOG_ERROR("Error updating feed: %s", payload->error->message().c_str());
        }
        return payload && !payload->error;
    }

    bool Broadcast::updateTitle() {
        auto payload = graphqlRequest<caffql::Mutation::ChangeStageTitleField>(
                sharedCredentials, clientId, caffql::ClientType::Capture, fullTitle());
        if (payload && payload->error) {
            LOG_ERROR("Error updating title: %s", payload->error->message().c_str());
        }
        return payload && !payload->error;
    }

    void Broadcast::setupSubscription(size_t tryNum) {
        std::weak_ptr<Broadcast> weakThis = shared_from_this();

        LOG_DEBUG("Setting up GraphQL subscription. Attempt %zu", tryNum);

        auto messageHandler = [weakThis](caffql::GraphqlResponse<caffql::StageSubscriptionPayload> update) mutable {
            LOG_DEBUG("Subscription message received");
            auto strongThis = weakThis.lock();
            if (!strongThis) {
                LOG_DEBUG("Broadcast no longer exists. Ignoring.");
                return;
            }

            if (auto payload = get_if<caffql::StageSubscriptionPayload>(&update)) {
                LOG_DEBUG("Stage update received");
                if (strongThis->subscriptionState == SubscriptionState::None) {
                    strongThis->subscriptionState = SubscriptionState::Open;
                    strongThis->subscriptionOpened.set_value(true);
                }

                auto const & feeds = payload->stage.feeds;
                auto feedIt = std::find_if(
                        feeds.begin(), feeds.end(), [&](auto const & feed) { return feed.id == strongThis->feedId; });
                bool feedIsOnStage = feedIt != feeds.end();

                if (feedIsOnStage) {
                    if (strongThis->subscriptionState == SubscriptionState::Open) {
                        LOG_DEBUG("Feed has appeared on stage");
                        strongThis->subscriptionState = SubscriptionState::FeedHasAppeared;
                    }

                    if (payload->stage.live) {
                        LOG_DEBUG("Stage has gone live");
                        strongThis->subscriptionState = SubscriptionState::StageHasGoneLive;
                    }
                } else if (
                        (strongThis->subscriptionState == SubscriptionState::FeedHasAppeared ||
                         strongThis->subscriptionState == SubscriptionState::StageHasGoneLive) &&
                        strongThis->isOnline()) {
                    // If our feed has appeared in the subscription and is no longer there
                    // while we think we're online, then we've failed.
                    LOG_ERROR("Feed no longer exists on stage");
                    strongThis->failedCallback(caff_ResultTakeover);
                    return;
                } else {
                    LOG_ERROR("Feed not present in stage");
                }

                if (!payload->stage.live && strongThis->subscriptionState == SubscriptionState::StageHasGoneLive &&
                    strongThis->state == State::Live) {
                    // If the stage is no longer live after we've gone live with our feed, then we've failed.
                    LOG_ERROR("Stage failed to go live");
                    strongThis->failedCallback(caff_ResultTakeover);
                }
            } else if (auto errors = get_if<std::vector<caffql::GraphqlError>>(&update)) {
                // TODO: See if we can report more meaningful errors to user than "broadcast failed"
                LOG_ERROR("Error(s) creating GraphQL subscription:");
                for (auto & error : *errors) {
                    LOG_ERROR("    %s", error.message.c_str());
                }
                strongThis->failedCallback(caff_ResultBroadcastFailed);
            }
        };

        auto endedHandler = [weakThis, tryNum](WebsocketClient::ConnectionEndType endType) {
            if (auto strongThis = weakThis.lock()) {
                switch (strongThis->state) {
                case State::Starting:
                case State::Streaming:
                case State::Live:
                    if (endType == WebsocketClient::ConnectionEndType::Closed) {
                        LOG_WARNING("Stage websocket was closed.");
                        // retry immediately
                        strongThis->setupSubscription();
                        return;
                    }
                    LOG_WARNING("Stage websocket failed.");
                    // retry after delay below
                    break;
                case State::Offline:
                case State::Stopping:
                    LOG_DEBUG("Broadcast is offline. Subscription ended.");
                    return;
                }
            } else {
                LOG_DEBUG("Broadcast no longer exists. Subscription ended.");
                return;
            }

            auto sleepFor = backoffDuration(tryNum);
            LOG_WARNING("Retrying broadcast subscription in %lld seconds.", sleepFor.count());
            std::this_thread::sleep_for(sleepFor);

            if (auto strongThis = weakThis.lock()) {
                switch (strongThis->state) {
                case State::Starting:
                case State::Streaming:
                case State::Live:
                    LOG_WARNING("Retrying broadcast subscription.");
                    strongThis->setupSubscription(tryNum + 1);
                    return;
                case State::Offline:
                case State::Stopping:
                    LOG_WARNING("Broadcast offline. Retry canceled.");
                    return;
                }
            } else {
                LOG_DEBUG("Broadcast no longer exists. Retry canceled.");
            }
        };

        subscription = std::make_shared<GraphqlSubscription<caffql::Subscription::StageField>>(
                websocketClient,
                sharedCredentials,
                "stage " + username,
                messageHandler,
                endedHandler,
                clientId,
                caffql::ClientType::Capture,
                clientType,
                username,
                nullopt,
                nullopt,
                false);
        subscription->connect();
    }

} // namespace caff
