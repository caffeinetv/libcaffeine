// Copyright 2019 Caffeine Inc. All rights reserved.

#include "Instance.hpp"

#include "AudioDevice.hpp"
#include "Broadcast.hpp"
#include "X264Encoder.hpp"

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/peerconnectioninterface.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "media/base/h264_profile_level_id.h"
#include "media/base/mediaconstants.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/thread.h"

namespace caff {

    // TODO: Use hardware encoding on low powered cpu or high quality GPU
    class EncoderFactory : public webrtc::VideoEncoderFactory {
    public:
        virtual ~EncoderFactory() {}

        virtual std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
            auto profileId =
                    webrtc::H264::ProfileLevelId(webrtc::H264::kProfileConstrainedBaseline, webrtc::H264::kLevel3_1);
            auto name = cricket::kH264CodecName;
            auto profile_string = webrtc::H264::ProfileLevelIdToString(profileId);
            std::map<std::string, std::string> parameters = { { cricket::kH264FmtpProfileLevelId,
                                                                profile_string.value() } };
            return { webrtc::SdpVideoFormat(name, parameters) };
        }

        virtual CodecInfo QueryVideoEncoder(const webrtc::SdpVideoFormat & format) const override {
            return { false, false };
        }

        virtual std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
                const webrtc::SdpVideoFormat & format) override {
            return std::make_unique<X264Encoder>(cricket::VideoCodec(format));
        }
    };

    Instance::Instance() : taskQueue("caffeine-dispatcher") {
        networkThread = rtc::Thread::CreateWithSocketServer();
        networkThread->SetName("caffeine-network", nullptr);
        networkThread->Start();

        workerThread = rtc::Thread::Create();
        workerThread->SetName("caffeine-worker", nullptr);
        workerThread->Start();

        signalingThread = rtc::Thread::Create();
        signalingThread->SetName("caffeine-signaling", nullptr);
        signalingThread->Start();

        audioDevice =
                workerThread->Invoke<rtc::scoped_refptr<AudioDevice>>(RTC_FROM_HERE, [] { return new AudioDevice(); });

        factory = webrtc::CreatePeerConnectionFactory(
                networkThread.get(),
                workerThread.get(),
                signalingThread.get(),
                audioDevice,
                webrtc::CreateBuiltinAudioEncoderFactory(),
                webrtc::CreateBuiltinAudioDecoderFactory(),
                std::make_unique<EncoderFactory>(),
                webrtc::CreateBuiltinVideoDecoderFactory(),
                nullptr,
                nullptr);

        gameList = getSupportedGames();
    }

    Instance::~Instance() { factory = nullptr; }

    caff_Result Instance::enumerateGames(std::function<void(char const *, char const *, char const *)> enumerator) {
        if (!gameList) {
            gameList = getSupportedGames();
        }
        if (!gameList || gameList->empty()) {
            LOG_ERROR("Failed to load supported game list");
            return caff_ResultFailure;
        }
        for (auto const & game : *gameList) {
            for (auto const & processName : game.processNames) {
                enumerator(processName.c_str(), game.id.c_str(), game.name.c_str());
            }
        }
        return caff_ResultSuccess;
    }

    caff_Result Instance::signIn(char const * username, char const * password, char const * otp) {
        return authenticate([=] { return caff::signIn(username, password, otp); });
    }

    caff_Result Instance::refreshAuth(char const * refreshToken) {
        return authenticate([=] { return caff::refreshAuth(refreshToken); });
    }

    caff_Result Instance::authenticate(std::function<AuthResponse()> authFunc) {
        if (!isSupportedVersion()) {
            return caff_ResultOldVersion;
        }
        auto response = authFunc();
        if (!response.credentials) {
            return response.result;
        }
        refreshToken = response.credentials->refreshToken;
        sharedCredentials.emplace(std::move(*response.credentials));
        userInfo = caff::getUserInfo(*sharedCredentials);
        if (!userInfo) {
            sharedCredentials.reset();
            this->refreshToken.reset();
            return caff_ResultFailure;
        }
        return response.result;
    }

    void Instance::signOut() {
        sharedCredentials.reset();
        refreshToken.reset();
        userInfo.reset();
    }

    bool Instance::isSignedIn() const { return sharedCredentials.has_value() && userInfo.has_value(); }

    char const * Instance::getRefreshToken() const { return refreshToken ? refreshToken->c_str() : nullptr; }

    char const * Instance::getUsername() const { return userInfo ? userInfo->username.c_str() : nullptr; }

    char const * Instance::getStageId() const { return userInfo ? userInfo->stageId.c_str() : nullptr; }

    bool Instance::canBroadcast() const { return userInfo ? userInfo->canBroadcast : false; }

    caff_Result Instance::startBroadcast(
            std::string title,
            caff_Rating rating,
            std::string gameId,
            std::function<void()> startedCallback,
            std::function<void(caff_Result)> failedCallback) {
        if (!isSignedIn()) {
            return caff_ResultNotSignedIn;
        }

        std::lock_guard<std::mutex> lock(broadcastMutex);

        if (broadcast) {
            return caff_ResultAlreadyBroadcasting;
        }

        auto dispatchFailure = [=](caff_Result error) {
            taskQueue.PostTask([=] {
                failedCallback(error);
                endBroadcast();
            });
        };

        broadcast = std::make_shared<Broadcast>(
                *sharedCredentials,
                userInfo->username,
                std::move(title),
                rating,
                std::move(gameId),
                audioDevice,
                factory);
        broadcast->start(startedCallback, dispatchFailure);
        return caff_ResultSuccess;
    }

    std::shared_ptr<Broadcast> Instance::getBroadcast() {
        std::lock_guard<std::mutex> lock(broadcastMutex);
        return broadcast;
    }

    void Instance::endBroadcast() {
        std::lock_guard<std::mutex> lock(broadcastMutex);
        if (broadcast) {
            broadcast->stop();
            broadcast.reset();
        }
    }

}  // namespace caff
