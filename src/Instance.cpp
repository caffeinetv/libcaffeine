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

    // TODO: Use hardware encoding on low powered cpu
    class EncoderFactory : public webrtc::VideoEncoderFactory {
    public:
        virtual ~EncoderFactory() {}

        virtual std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override
        {
            auto profileId = webrtc::H264::ProfileLevelId(
                webrtc::H264::kProfileConstrainedBaseline, webrtc::H264::kLevel3_1);
            auto name = cricket::kH264CodecName;
            auto profile_string = webrtc::H264::ProfileLevelIdToString(profileId);
            std::map<std::string, std::string> parameters = {
                {cricket::kH264FmtpProfileLevelId, profile_string.value()} };
            return { webrtc::SdpVideoFormat(name, parameters) };
        }

        virtual CodecInfo QueryVideoEncoder(const webrtc::SdpVideoFormat& format) const override
        {
            return { false, false };
        }

        virtual std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
            const webrtc::SdpVideoFormat& format) override
        {
            return std::make_unique<X264Encoder>(cricket::VideoCodec(format));
        }
    };

    Instance::Instance()
        : taskQueue("caffeine-dispatcher")
    {
        networkThread = rtc::Thread::CreateWithSocketServer();
        networkThread->SetName("caffeine-network", nullptr);
        networkThread->Start();

        workerThread = rtc::Thread::Create();
        workerThread->SetName("caffeine-worker", nullptr);
        workerThread->Start();

        signalingThread = rtc::Thread::Create();
        signalingThread->SetName("caffeine-signaling", nullptr);
        signalingThread->Start();

        audioDevice = workerThread->Invoke<rtc::scoped_refptr<AudioDevice>>(
            RTC_FROM_HERE, [] { return new AudioDevice(); });

        factory = webrtc::CreatePeerConnectionFactory(
            networkThread.get(), workerThread.get(), signalingThread.get(),
            audioDevice, webrtc::CreateBuiltinAudioEncoderFactory(),
            webrtc::CreateBuiltinAudioDecoderFactory(),
            std::make_unique<EncoderFactory>(),
            webrtc::CreateBuiltinVideoDecoderFactory(), nullptr, nullptr);
    }

    Instance::~Instance() {
        factory = nullptr;
    }

    caff_AuthResult Instance::signIn(char const * username, char const * password, char const * otp)
    {
        return authenticate([=] { return caff::signIn(username, password, otp); });
    }

    caff_AuthResult Instance::refreshAuth(char const * refreshToken)
    {
        return authenticate([=] { return caff::refreshAuth(refreshToken); });
    }

    caff_AuthResult Instance::authenticate(std::function<AuthResponse()> authFunc)
    {
        if (!isSupportedVersion()) {
            return caff_AuthResultOldVersion;
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
            return caff_AuthResultRequestFailed;
        }
        return response.result;
    }

    void Instance::signOut()
    {
        sharedCredentials.reset();
        refreshToken.reset();
        userInfo.reset();
    }

    bool Instance::isSignedIn() const
    {
        return sharedCredentials.has_value() && userInfo.has_value();
    }

    char const * Instance::getRefreshToken() const
    {
        return refreshToken ? refreshToken->c_str() : nullptr;
    }

    char const * Instance::getUsername() const
    {
        return userInfo ? userInfo->username.c_str() : nullptr;
    }

    char const * Instance::getStageId() const
    {
        return userInfo ? userInfo->stageId.c_str() : nullptr;
    }

    bool Instance::canBroadcast() const
    {
        return userInfo ? userInfo->canBroadcast : false;
    }

    caff_Error Instance::startBroadcast(
        std::string title,
        caff_Rating rating,
        std::function<void()> startedCallback,
        std::function<void(caff_Error)> failedCallback)
    {
        if (!isSignedIn()) {
            return caff_ErrorNotSignedIn;
        }
        if (broadcast) {
            return caff_ErrorInvalid;
        }

        auto dispatchFailure = [=](caff_Error error) {
            taskQueue.PostTask([=] {
                failedCallback(error);
                endBroadcast();
            });
        };

        broadcast = std::make_unique<Broadcast>(
            *sharedCredentials,
            userInfo->username,
            title,
            rating,
            audioDevice,
            factory);
        broadcast->start(startedCallback, dispatchFailure);
        return caff_ErrorNone;
    }

    Broadcast * Instance::getBroadcast()
    {
        return broadcast.get();
    }

    void Instance::endBroadcast()
    {
        if (broadcast) {
            broadcast->stop();
            broadcast.reset(nullptr);
        }
    }

}  // namespace caff
