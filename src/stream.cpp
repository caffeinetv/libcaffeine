// Copyright 2019 Caffeine Inc. All rights reserved.

#include "stream.hpp"

#include <thread>
#include <ctime>
#include <random>

#include "caffeine.h"
#include "api.hpp"
#include "audiodevice.hpp"
#include "peerconnectionobserver.hpp"
#include "sessiondescriptionobserver.hpp"
#include "videocapturer.hpp"

#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"
#include "rtc_base/logging.h"

namespace caff {

    Stream::Stream(
        Credentials * credentials,
        std::string username,
        std::string title,
        caff_rating rating,
        AudioDevice* audioDevice,
        webrtc::PeerConnectionFactoryInterface* factory)
        : credentials(credentials)
        , username(std::move(username))
        , title(std::move(title))
        , rating(rating)
        , audioDevice(audioDevice)
        , factory(factory) {}

    Stream::~Stream() {}

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

    char const * Stream::StateString(Stream::State state)
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

    bool Stream::RequireState(State expectedState) const
    {
        State currentState = state;
        if (currentState != expectedState) {
            RTC_LOG(LS_ERROR) << "In state " << StateString(currentState)
                << " when expecting " << StateString(expectedState);
            return false;
        }
        return true;
    }

    bool Stream::TransitionState(State oldState, State newState)
    {
        bool result = state.compare_exchange_strong(oldState, newState);
        if (!result)
            RTC_LOG(LS_ERROR) << "Transitioning to state " << StateString(newState)
                << " expects state " << StateString(oldState);
        return result;
    }

    bool Stream::IsOnline() const
    {
        return state == State::Online;
    }

    static std::string caff_annotate_title(std::string title, caff_rating rating)
    {
        static const size_t MAX_TITLE_LENGTH = 60; // TODO: policy- should be somewhere else
        static std::string const ratingStrings[] = { "", "[17+] " }; // TODO maybe same here

        if (rating < CAFF_RATING_NONE || rating >= CAFF_RATING_MAX)
            rating = CAFF_RATING_NONE;

        auto fullTitle = ratingStrings[rating] + title;
        if (fullTitle.length() > MAX_TITLE_LENGTH)
            fullTitle.resize(MAX_TITLE_LENGTH);

        return fullTitle;
    }

    optional<std::string> Stream::OfferGenerated(std::string const & offer)
    {
        feedId = createUniqueId();

        if (!RequireState(State::Starting)) {
            return {};
        }

        auto fullTitle = caff_annotate_title(title, rating);
        auto clientId = createUniqueId();

        // Make initial stage request to get a cursor

        StageRequest * request = createStageRequest(username, clientId);

        if (!requestStageUpdate(request, credentials, NULL, NULL)) {
            return {};
        }

        // Make request to add our feed and get a new broadcast id
        request->stage->title = std::move(fullTitle);
        request->stage->upsertBroadcast = true;
        request->stage->broadcastId.reset();
        request->stage->live = false;

        FeedStream stream{};
        stream.sdpOffer = offer;

        Feed feed{};
        feed.id = feedId;
        feed.clientId = clientId;
        feed.role = "primary";
        feed.volume = 1.0;
        feed.capabilities = { true, true };
        feed.stream = std::move(stream);

        request->stage->feeds = { {feedId, std::move(feed)} };

        bool isOutOfCapacity = false;
        if (!requestStageUpdate(request, credentials, NULL, &isOutOfCapacity)) {
            if (isOutOfCapacity) {
                // TODO: figure out a way to reproduce this behavior
                //set_error(context->output, "%s", obs_module_text("ErrorOutOfCapacity"));
            }
            return {};
        }

        // Get stream details
        auto feedIt = request->stage->feeds->find(feedId);
        if (feedIt == request->stage->feeds->end()) {
            return {};
        }

        auto & responseStream = feedIt->second.stream;

        if (!responseStream
            || !responseStream->sdpAnswer
            || responseStream->sdpAnswer->empty()
            || !responseStream->url
            || responseStream->url->empty()) {
            return {};
        }

        streamUrl = *responseStream->url;
        nextRequest = request;

        return responseStream->sdpAnswer;
    }

    bool Stream::IceGathered(std::vector<IceInfo> candidates)
    {
        return trickleCandidates(candidates, streamUrl, credentials);
    }

    /*

    // Falls back to obs_id if no foreground game detected
    static char const * get_running_game_id(
        caff_games * games, const char * fallback_id)
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


    static void * longpoll_thread(void * data);

    static void * broadcast_thread(void * data)
    {
        TRACE();
        os_set_thread_name("Caffeine broadcast");

        struct caffeine_output * context = data;

        pthread_mutex_lock(&context->stream_mutex);
        if (!RequireState(ONLINE)) {
            pthread_mutex_unlock(&context->stream_mutex);
            return NULL;
        }

        char * feed_id = bstrdup(context->broadcast_info->feed_id);
        char * stream_url = bstrdup(context->broadcast_info->stream_url);
        StageRequest * request =
            context->broadcast_info->next_request;
        context->broadcast_info->next_request = NULL;
        pthread_mutex_unlock(&context->stream_mutex);

        obs_service_t * service = obs_output_get_service(context->output);
        Credentials * credentials =
            obs_service_query(service, CAFFEINE_QUERY_CREDENTIALS);

        caff_games * games = caff_get_supported_games();
        char const * obs_game_id = get_game_id(games, "obs");

        // Obtain broadcast id

        char * broadcastId = request->stage->broadcastId;

        for (int broadcast_id_retry_count = 0; !broadcastId && broadcast_id_retry_count < 3; ++broadcast_id_retry_count) {
            request->stage->upsertBroadcast = true;
            if (!requestStageUpdate(request, credentials, NULL, NULL)
                || !caff_get_stage_feed(request->stage, feed_id))
            {
                caffeine_stream_failed(data, CAFF_ERROR_UNKNOWN);
                goto broadcast_error;
            }

            broadcastId = request->stage->broadcastId;
        }

        pthread_mutex_lock(&context->screenshot_mutex);
        while (context->screenshot_needed)
            pthread_cond_wait(&context->screenshot_cond,
                &context->screenshot_mutex);
        pthread_mutex_unlock(&context->screenshot_mutex);

        bool screenshot_success = caff_update_broadcast_screenshot(
            broadcastId,
            context->screenshot.data,
            context->screenshot.size,
            credentials);

        if (!screenshot_success) {
            caffeine_stream_failed(data, CAFF_ERROR_BROADCAST_FAILED);
            goto broadcast_error;
        }

        // Set stage live with current game content

        caffeine_update_game_id(
            get_running_game_id(games, obs_game_id),
            caff_get_stage_feed(request->stage, feed_id));
        request->stage->live = true;

        if (!requestStageUpdate(request, credentials, NULL, NULL)
            || !request->stage->live
            || !caff_get_stage_feed(request->stage, feed_id))
        {
            caffeine_stream_failed(data, CAFF_ERROR_BROADCAST_FAILED);
            goto broadcast_error;
        }

        pthread_mutex_lock(&context->stream_mutex);
        context->broadcast_info->next_request = request;
        request = NULL;
        pthread_mutex_unlock(&context->stream_mutex);

        pthread_create(
            &context->longpoll_thread, NULL, longpoll_thread, context);

        // TODO: use wall time instead of accumulation of sleep time
        long const heartbeat_interval = 5000; // TODO: use chrono
        long const check_interval = 100;

        long interval = heartbeat_interval;

        static int const max_failures = 5;
        int failures = 0;

        for (enum state state = get_state(context);
            state == ONLINE;
            os_sleep_ms(check_interval), state = get_state(context))
        {
            interval += check_interval;
            if (interval < heartbeat_interval)
                continue;

            interval = 0;

            caff_free_stage_request(&request);

            pthread_mutex_lock(&context->stream_mutex);
            request = caff_copy_stage_request(context->broadcast_info->next_request);
            pthread_mutex_unlock(&context->stream_mutex);

            if (!request) {
                caffeine_stream_failed(data, CAFF_ERROR_UNKNOWN);
                goto broadcast_error;
            }

            Feed * feed = caff_get_stage_feed(request->stage, feed_id);
            if (!feed || !request->stage->live) {
                caffeine_stream_failed(data, CAFF_ERROR_TAKEOVER);
                goto broadcast_error;
            }

            bool should_mutate_feed = false;

            // Heartbeat stream

            HeartbeatResponse * heartbeat_response =
                caff_heartbeat_stream(stream_url, credentials);

            if (heartbeat_response) {
                should_mutate_feed =
                    caffeine_update_connection_quality(
                        heartbeat_response->connection_quality,
                        feed);
                failures = 0;
                caff_free_heartbeat_response(&heartbeat_response);
            }
            else {
                LOG_DEBUG("Heartbeat failed");
                ++failures;
                if (failures > max_failures) {
                    LOG_ERROR("Heartbeat failed %d times; ending stream.",
                        failures);
                    caffeine_stream_failed(data, CAFF_ERROR_DISCONNECTED);
                    break;
                }
            }

            should_mutate_feed =
                caffeine_update_game_id(
                    get_running_game_id(games, obs_game_id), feed)
                || should_mutate_feed;

            if (!should_mutate_feed) {
                continue;
            }

            // Mutate the feed

            set_is_mutating_feed(context, true);

            if (!requestStageUpdate(request, credentials, NULL, NULL)) {
                set_is_mutating_feed(context, false);
                // If we have a stream going but can't talk to
                // the stage endpoint, retry the mutation next loop
                continue;
            }

            if (!request->stage->live
                || !caff_get_stage_feed(request->stage, feed_id))
            {
                caffeine_stream_failed(data, CAFF_ERROR_TAKEOVER);
                goto broadcast_error;
            }

            pthread_mutex_lock(&context->stream_mutex);
            caff_free_stage_request(&context->broadcast_info->next_request);
            context->broadcast_info->next_request = request;
            request = NULL;
            pthread_mutex_unlock(&context->stream_mutex);
            set_is_mutating_feed(context, false);
        }

        set_is_mutating_feed(context, true);

        pthread_mutex_lock(&context->stream_mutex);
        if (context->broadcast_info) {
            caff_free_stage_request(&request);
            request = context->broadcast_info->next_request;
            context->broadcast_info->next_request = NULL;
        }
        pthread_mutex_unlock(&context->stream_mutex);

        // Only set the stage offline if it contains our feed
        if (request && caff_get_stage_feed(request->stage, feed_id)) {
            request->stage->live = false;
            caff_clear_stage_feeds(request->stage);

            if (!requestStageUpdate(request, credentials, NULL, NULL)) {
                caffeine_stream_failed(data, CAFF_ERROR_UNKNOWN);
            }
        }

    broadcast_error:
        bfree(stream_url);
        bfree(feed_id);
        caff_free_stage_request(&request);
        caff_free_game_list(&games);
        return NULL;
    }

    static void * longpoll_thread(void * data)
    {
        // This thread is purely for hearbeating our feed.
        // If the broadcast thread is making a mutation, this thread waits.

        TRACE();
        os_set_thread_name("Caffeine longpoll");

        struct caffeine_output * context = data;

        obs_service_t * service = obs_output_get_service(context->output);
        Credentials * credentials =
            obs_service_query(service, CAFFEINE_QUERY_CREDENTIALS);

        long retry_interval_ms = 0; // TODO use chrono
        long const check_interval = 100;

        long interval = retry_interval_ms;

        char * feed_id = NULL;

        pthread_mutex_lock(&context->stream_mutex);
        if (context->broadcast_info) {
            feed_id = bstrdup(context->broadcast_info->feed_id);
        }
        pthread_mutex_unlock(&context->stream_mutex);

        if (!feed_id) {
            return NULL;
        }

        for (enum state state = get_state(context);
            state == ONLINE;
            os_sleep_ms(check_interval), state = get_state(context))
        {
            interval += check_interval;
            if (interval < retry_interval_ms || is_mutating_feed(context))
                continue;

            StageRequest * request = NULL;

            pthread_mutex_lock(&context->stream_mutex);
            if (context->broadcast_info) {
                request = caff_copy_stage_request(
                    context->broadcast_info->next_request);
            }
            pthread_mutex_unlock(&context->stream_mutex);

            if (!request) {
                break;
            }

            double retryIn = 0;

            if (!requestStageUpdate(request, credentials, &retryIn, NULL)) {
                //If we have a stream going but can't talk to the stage endpoint,
                // just continually retry with some waiting
                retry_interval_ms = 5000;
                continue;
            }

            bool live_feed_is_still_present =
                request->stage->live
                && caff_get_stage_feed(request->stage, feed_id);

            pthread_mutex_lock(&context->stream_mutex);
            if (context->broadcast_info) {
                caff_free_stage_request(
                    &context->broadcast_info->next_request);
                context->broadcast_info->next_request = request;
                request = NULL;
            }
            pthread_mutex_unlock(&context->stream_mutex);

            caff_free_stage_request(&request);

            if (!live_feed_is_still_present) {
                break;
            }

            interval = 0;
            retry_interval_ms = (long)(retryIn * 1000);
        }

        bfree(feed_id);

        return NULL;
    }
    */

    void Stream::Start(
        std::function<void()> startedCallback,
        std::function<void(caff_error)> failedCallback) {

        TransitionState(State::Offline, State::Starting);

        auto doFailure = [=](caff_error error) {
            failedCallback(error);
            state.store(State::Offline);
        };

        std::thread startThread([=] {
            //webrtc::PeerConnectionInterface::IceServer server;
            //server.urls.push_back("stun:stun.l.google.com:19302");

            webrtc::PeerConnectionInterface::RTCConfiguration config;
            //config.servers.push_back(server);

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

            auto offer = creationObserver->GetFuture().get();
            if (!offer) {
                // Logged by the observer
                failedCallback(CAFF_ERROR_SDP_OFFER);
                state.store(State::Offline);
                return;
            }

            if (offer->type() != webrtc::SessionDescriptionInterface::kOffer) {
                RTC_LOG(LS_ERROR)
                    << "Expected " << webrtc::SessionDescriptionInterface::kOffer
                    << " but got " << offer->type();
                doFailure(CAFF_ERROR_SDP_OFFER);
                return;
            }

            std::string offerSdp;
            if (!offer->ToString(&offerSdp)) {
                RTC_LOG(LS_ERROR) << "Error serializing SDP offer";
                doFailure(CAFF_ERROR_SDP_OFFER);
                return;
            }

            webrtc::SdpParseError offerError;
            auto localDesc = webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, offerSdp, &offerError);
            if (!localDesc) {
                RTC_LOG(LS_ERROR) << "Error parsing SDP offer: " << offerError.description;
                doFailure(CAFF_ERROR_SDP_OFFER);
                return;
            }

            rtc::scoped_refptr<SetSessionDescriptionObserver> setLocalObserver =
                new rtc::RefCountedObject<SetSessionDescriptionObserver>;

            peerConnection->SetLocalDescription(setLocalObserver, localDesc.release());
            auto setLocalSuccess = setLocalObserver->GetFuture().get();
            if (!setLocalSuccess) {
                doFailure(CAFF_ERROR_SDP_OFFER);
                return;
            }

            // offerGenerated must be called before iceGathered so that a signed_payload
            // is available
            auto answerSdp = OfferGenerated(offerSdp);
            if (!answerSdp) {
                RTC_LOG(LS_ERROR) << "Did not get SDP answer";
                doFailure(CAFF_ERROR_SDP_ANSWER);
                return;
            }

            webrtc::SdpParseError answerError;
            auto remoteDesc = webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, *answerSdp, &answerError);
            if (!remoteDesc) {
                RTC_LOG(LS_ERROR) << "Error parsing SDP answer: " << answerError.description;
                doFailure(CAFF_ERROR_SDP_ANSWER);
                return;
            }

            if (!IceGathered(observer->GetFuture().get())) {
                RTC_LOG(LS_ERROR) << "Failed to negotiate ICE";
                doFailure(CAFF_ERROR_ICE_TRICKLE);
                return;
            }

            rtc::scoped_refptr<SetSessionDescriptionObserver> setRemoteObserver =
                new rtc::RefCountedObject<SetSessionDescriptionObserver>;

            peerConnection->SetRemoteDescription(setRemoteObserver, remoteDesc.release());
            auto setRemoteSuccess = setRemoteObserver->GetFuture().get();
            if (!setRemoteSuccess) {
                // Logged by the observer
                doFailure(CAFF_ERROR_SDP_ANSWER);
                return;
            }

            state.store(State::Online);
            startedCallback();
        });

        startThread.detach();
    }

    void Stream::SendAudio(uint8_t const* samples, size_t samplesPerChannel) {
        RTC_DCHECK(IsOnline());
        audioDevice->SendAudio(samples, samplesPerChannel);
    }

    void Stream::SendVideo(uint8_t const* frameData,
        size_t frameBytes,
        int32_t width,
        int32_t height,
        caff_format format) {
        RTC_DCHECK(IsOnline());
        videoCapturer->SendVideo(frameData, frameBytes, width, height, static_cast<webrtc::VideoType>(format));
    }

    caff_connection_quality Stream::GetConnectionQuality() {
        if (nextRequest && nextRequest->stage->feeds) {
            auto feedIt = nextRequest->stage->feeds->find(feedId);
            if (feedIt != nextRequest->stage->feeds->end() && feedIt->second.sourceConnectionQuality) {
                return *feedIt->second.sourceConnectionQuality;
            }
        }

        return CAFF_CONNECTION_QUALITY_UNKNOWN;
    }

}  // namespace caff
