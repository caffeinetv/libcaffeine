/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma once

#include "modules/audio_device/audio_device_generic.h"
#include "rtc_base/refcountedobject.h"

namespace caff {

// Contains default implementations of AudioDeviceModule methods that
// are not supported by caffeine. This is mostly to make the actual impl
// more readable and maintainable.
class AudioDeviceDefaultImpl
    : public rtc::RefCountedObject<webrtc::AudioDeviceModule> {
 public:
  // Retrieve the currently utilized audio layer
  virtual int32_t ActiveAudioLayer(AudioLayer* audioLayer) const override {
    return -1;
  }

  // Full-duplex transportation of PCM audio
  virtual int32_t RegisterAudioCallback(
      webrtc::AudioTransport* audioCallback) override = 0;

  // Main initialization and termination
  virtual int32_t Init() override = 0;
  virtual int32_t Terminate() override = 0;
  virtual bool Initialized() const override = 0;

  // Device enumeration
  virtual int16_t PlayoutDevices() override { return 0; }
  virtual int16_t RecordingDevices() override = 0;
  virtual int32_t PlayoutDeviceName(
      uint16_t index,
      char name[webrtc::kAdmMaxDeviceNameSize],
      char guid[webrtc::kAdmMaxGuidSize]) override {
    return -1;
  }
  virtual int32_t RecordingDeviceName(
      uint16_t index,
      char name[webrtc::kAdmMaxDeviceNameSize],
      char guid[webrtc::kAdmMaxGuidSize]) override {
    return -1;
  }

  // Device selection
  virtual int32_t SetPlayoutDevice(uint16_t index) override { return -1; }
  virtual int32_t SetPlayoutDevice(WindowsDeviceType device) override {
    return -1;
  }
  virtual int32_t SetRecordingDevice(uint16_t index) override { return -1; }
  virtual int32_t SetRecordingDevice(WindowsDeviceType device) override {
    return -1;
  }

  // Audio transport initialization
  virtual int32_t PlayoutIsAvailable(bool* available) override {
    *available = false;
    return 0;
  }
  virtual int32_t InitPlayout() override { return -1; }
  virtual bool PlayoutIsInitialized() const override { return false; }
  virtual int32_t RecordingIsAvailable(bool* available) override {
    *available = true;
    return 0;
  }
  virtual int32_t InitRecording() override = 0;
  virtual bool RecordingIsInitialized() const override = 0;

  // Audio transport control
  virtual int32_t StartPlayout() override { return -1; }
  virtual int32_t StopPlayout() override { return -1; }
  virtual bool Playing() const override { return -1; }
  virtual int32_t StartRecording() override = 0;
  virtual int32_t StopRecording() override = 0;
  virtual bool Recording() const override = 0;

  // Audio mixer initialization
  virtual int32_t InitSpeaker() override { return -1; }
  virtual bool SpeakerIsInitialized() const override { return false; }
  virtual int32_t InitMicrophone() override { return -1; }
  virtual bool MicrophoneIsInitialized() const override { return false; }

  // Speaker volume controls
  virtual int32_t SpeakerVolumeIsAvailable(bool* available) override {
    *available = false;
    return 0;
  }
  virtual int32_t SetSpeakerVolume(uint32_t volume) override { return -1; }
  virtual int32_t SpeakerVolume(uint32_t* volume) const override { return -1; }
  virtual int32_t MaxSpeakerVolume(uint32_t* maxVolume) const override {
    return -1;
  }
  virtual int32_t MinSpeakerVolume(uint32_t* minVolume) const override {
    return -1;
  }

  // Microphone volume controls
  virtual int32_t MicrophoneVolumeIsAvailable(bool* available) override {
    *available = false;
    return 0;
  }
  virtual int32_t SetMicrophoneVolume(uint32_t volume) override { return -1; }
  virtual int32_t MicrophoneVolume(uint32_t* volume) const override {
    return -1;
  }
  virtual int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const override {
    return -1;
  }
  virtual int32_t MinMicrophoneVolume(uint32_t* minVolume) const override {
    return -1;
  }

  // Speaker mute control
  virtual int32_t SpeakerMuteIsAvailable(bool* available) override {
    *available = false;
    return 0;
  }
  virtual int32_t SetSpeakerMute(bool enable) override { return -1; }
  virtual int32_t SpeakerMute(bool* enabled) const override { return -1; }

  // Microphone mute control
  virtual int32_t MicrophoneMuteIsAvailable(bool* available) override {
    *available = false;
    return 0;
  }
  virtual int32_t SetMicrophoneMute(bool enable) override { return -1; }
  virtual int32_t MicrophoneMute(bool* enabled) const override { return -1; }

  // Stereo support
  virtual int32_t StereoPlayoutIsAvailable(bool* available) const override {
    *available = false;
    return 0;
  }
  virtual int32_t SetStereoPlayout(bool enable) override { return -1; }
  virtual int32_t StereoPlayout(bool* enabled) const override { return -1; }
  virtual int32_t StereoRecordingIsAvailable(bool* available) const override {
    *available = true;
    return 0;
  }
  virtual int32_t SetStereoRecording(bool enable) override = 0;
  virtual int32_t StereoRecording(bool* enabled) const override = 0;

  // Playout delay
  virtual int32_t PlayoutDelay(uint16_t* delayMS) const override { return -1; }

  // Only supported on Android.
  virtual bool BuiltInAECIsAvailable() const override { return false; }
  virtual bool BuiltInAGCIsAvailable() const override { return false; }
  virtual bool BuiltInNSIsAvailable() const override { return false; }

  // Enables the built-in audio effects. Only supported on Android.
  virtual int32_t EnableBuiltInAEC(bool enable) override { return -1; }
  virtual int32_t EnableBuiltInAGC(bool enable) override { return -1; }
  virtual int32_t EnableBuiltInNS(bool enable) override { return -1; }

 protected:
  virtual ~AudioDeviceDefaultImpl() override {}
};

}  // namespace caff
