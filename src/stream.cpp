// Copyright 2019 Caffeine Inc. All rights reserved.

#include "stream.hpp"

#include <thread>

#include "audiodevice.hpp"
#include "caffeine.h"
#include "peerconnectionobserver.hpp"
#include "sessiondescriptionobserver.hpp"
#include "videocapturer.hpp"

#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"

namespace caff {

    Stream::Stream(AudioDevice* audioDevice, webrtc::PeerConnectionFactoryInterface* factory)
        : audioDevice(audioDevice), factory(factory) {}

    Stream::~Stream() {}

    void Stream::Start(
        std::function<std::string(std::string const&)> offerGeneratedCallback,
        std::function<bool(std::vector<IceInfo> const&)> iceGatheredCallback,
        std::function<void()> startedCallback,
        std::function<void(caff_error)> failedCallback) {

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
                return;
            }

            if (offer->type() != webrtc::SessionDescriptionInterface::kOffer) {
                RTC_LOG(LS_ERROR)
                    << "Expected " << webrtc::SessionDescriptionInterface::kOffer
                    << " but got " << offer->type();
                failedCallback(CAFF_ERROR_SDP_OFFER);
                return;
            }

            std::string offerSdp;
            if (!offer->ToString(&offerSdp)) {
                RTC_LOG(LS_ERROR) << "Error serializing SDP offer";
                failedCallback(CAFF_ERROR_SDP_OFFER);
                return;
            }

            webrtc::SdpParseError offerError;
            auto localDesc = webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, offerSdp, &offerError);
            if (!localDesc) {
                RTC_LOG(LS_ERROR) << "Error parsing SDP offer: " << offerError.description;
                failedCallback(CAFF_ERROR_SDP_OFFER);
                return;
            }

            rtc::scoped_refptr<SetSessionDescriptionObserver> setLocalObserver =
                new rtc::RefCountedObject<SetSessionDescriptionObserver>;

            peerConnection->SetLocalDescription(setLocalObserver, localDesc.release());
            auto setLocalSuccess = setLocalObserver->GetFuture().get();
            if (!setLocalSuccess) {
                failedCallback(CAFF_ERROR_SDP_OFFER);
                return;
            }

            // offerGenerated must be called before iceGathered so that a signed_payload
            // is available
            std::string answerSdp = offerGeneratedCallback(offerSdp);

            webrtc::SdpParseError answerError;
            auto remoteDesc = webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, answerSdp, &answerError);
            if (!remoteDesc) {
                RTC_LOG(LS_ERROR) << "Error parsing SDP answer: " << answerError.description;
                failedCallback(CAFF_ERROR_SDP_ANSWER);
                return;
            }

            if (!iceGatheredCallback(observer->GetFuture().get())) {
                RTC_LOG(LS_ERROR) << "Failed to negotiate ICE";
                failedCallback(CAFF_ERROR_ICE_TRICKLE);
                return;
            }

            rtc::scoped_refptr<SetSessionDescriptionObserver> setRemoteObserver =
                new rtc::RefCountedObject<SetSessionDescriptionObserver>;

            peerConnection->SetRemoteDescription(setRemoteObserver, remoteDesc.release());
            auto setRemoteSuccess = setRemoteObserver->GetFuture().get();
            if (!setRemoteSuccess) {
                // Logged by the observer
                failedCallback(CAFF_ERROR_SDP_ANSWER);
                return;
            }

            started.store(true);
            startedCallback();
        });

        startThread.detach();
    }

    void Stream::SendAudio(uint8_t const* samples, size_t samples_per_channel) {
        RTC_DCHECK(started);
        audioDevice->SendAudio(samples, samples_per_channel);
    }

    void Stream::SendVideo(uint8_t const* frameData,
        size_t frameBytes,
        int32_t width,
        int32_t height,
        caff_format format) {
        RTC_DCHECK(started);
        videoCapturer->SendVideo(frameData, frameBytes, width, height, static_cast<webrtc::VideoType>(format));
    }

}  // namespace caff
