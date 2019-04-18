// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "AudioDeviceDefaultImpl.hpp"

#include <vector>

namespace caff {

    class AudioDevice : public AudioDeviceDefaultImpl {
    public:
        AudioDevice();

        void sendAudio(uint8_t const * data, size_t samples_per_channel);

        virtual int32_t RegisterAudioCallback(webrtc::AudioTransport * audioTransport) override;
        virtual int32_t Init() override;
        virtual int32_t Terminate() override;
        virtual bool Initialized() const override;
        virtual int16_t RecordingDevices() override;
        virtual int32_t InitRecording() override;
        virtual bool RecordingIsInitialized() const override;
        virtual int32_t StartRecording() override;
        virtual int32_t StopRecording() override;
        virtual bool Recording() const override;
        virtual int32_t SetStereoRecording(bool enable) override;
        virtual int32_t StereoRecording(bool * enabled) const override;

    private:
        webrtc::AudioTransport * audioTransport{ nullptr };
        std::vector<uint8_t> buffer;
        size_t bufferIndex{ 0 };
    };

}  // namespace caff
