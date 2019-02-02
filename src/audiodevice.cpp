// Copyright 2019 Caffeine Inc. All rights reserved.

#include "audiodevice.hpp"

namespace caff {

    // TODO: make these dynamic and transcode
    static size_t const channels = 2;
    static size_t const sampleRate = 48000;
    static size_t const sampleSize = 2;
    static size_t const chunkSamples = sampleRate / 100;
    static size_t const chunkSize = chunkSamples * sampleSize * channels;

    AudioDevice::AudioDevice() : buffer(chunkSize) {}

    void AudioDevice::SendAudio(uint8_t const* data, size_t samplesPerChannel) {
        RTC_DCHECK(data);
        RTC_DCHECK(samplesPerChannel);

        size_t remainingData = samplesPerChannel * channels * sampleSize;
        size_t remainingBuffer = buffer.size() - bufferIndex;
        size_t toCopy = std::min(remainingData, remainingBuffer);

        while (toCopy) {
            memcpy(&buffer[bufferIndex], data, toCopy);
            data += toCopy;
            bufferIndex += toCopy;
            remainingData -= toCopy;
            remainingBuffer -= toCopy;
            if (remainingBuffer == 0) {
                uint32_t unused;
                audioTransport->RecordedDataIsAvailable(
                    &buffer[0], chunkSamples, sampleSize * channels, channels, sampleRate, 0, 0, 0, 0, unused);
                bufferIndex = 0;
                remainingBuffer = buffer.size();
            }
            toCopy = std::min(remainingData, remainingBuffer);
        }
    }

    int32_t AudioDevice::RegisterAudioCallback(webrtc::AudioTransport* audioTransport) {
        this->audioTransport = audioTransport;
        return 0;
    }

    int32_t AudioDevice::Init() {
        return 0;
    }

    int32_t AudioDevice::Terminate() {
        return 0;
    }

    bool AudioDevice::Initialized() const {
        return true;
    }

    int16_t AudioDevice::RecordingDevices() {
        return 1;
    }

    int32_t AudioDevice::InitRecording() {
        return 0;
    }

    bool AudioDevice::RecordingIsInitialized() const {
        return true;
    }

    int32_t AudioDevice::StartRecording() {
        return 0;
    }

    int32_t AudioDevice::StopRecording() {
        return 0;
    }

    bool AudioDevice::Recording() const {
        return true;
    }

    int32_t AudioDevice::SetStereoRecording(bool enable) {
        return 0;
    }

    int32_t AudioDevice::StereoRecording(bool* enabled) const {
        *enabled = true;
        return 0;
    }

}  // namespace caff
