// Copyright 2019 Caffeine Inc. All rights reserved.

#include "interface.hpp"

#include "audiodevice.hpp"
#include "stream.hpp"
#include "x264encoder.hpp"

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

        virtual std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
            auto profileId = webrtc::H264::ProfileLevelId(
                webrtc::H264::kProfileConstrainedBaseline, webrtc::H264::kLevel3_1);
            auto name = cricket::kH264CodecName;
            auto profile_string = webrtc::H264::ProfileLevelIdToString(profileId);
            std::map<std::string, std::string> parameters = {
                {cricket::kH264FmtpProfileLevelId, profile_string.value()} };
            return { webrtc::SdpVideoFormat(name, parameters) };
        }

        virtual CodecInfo QueryVideoEncoder(const webrtc::SdpVideoFormat& format) const override {
            return { false, false };
        }

        virtual std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
            const webrtc::SdpVideoFormat& format) override {
            return std::make_unique<X264Encoder>(cricket::VideoCodec(format));
        }
    };

    Interface::Interface() {
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

    Interface::~Interface() {
        factory = nullptr;
    }

    Stream* Interface::StartStream(
        std::function<std::string(std::string const&)> offerGeneratedCallback,
        std::function<bool(std::vector<IceInfo> const&)> iceGatheredCallback,
        std::function<void()> startedCallback,
        std::function<void(caff_error)> failedCallback) {
        auto stream = new Stream(audioDevice, factory);
        stream->Start(offerGeneratedCallback, iceGatheredCallback, startedCallback, failedCallback);
        return stream;
    }

}  // namespace caff
