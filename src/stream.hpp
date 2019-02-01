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

#include <atomic>
#include <functional>
#include <vector>

#include "iceinfo.hpp"

#include "common_types.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace {
// Protect against changes in underlying enum

constexpr bool isEnumEqual(caff_format caff, webrtc::VideoType rtc) {
  return caff == static_cast<caff_format>(rtc);
}

#define MISMATCH_MSG "mismatch between caff_format and webrtc::VideoType"

static_assert(isEnumEqual(CAFF_FORMAT_UNKNOWN, webrtc::VideoType::kUnknown),
              MISMATCH_MSG);
static_assert(isEnumEqual(CAFF_FORMAT_I420, webrtc::VideoType::kI420),
              MISMATCH_MSG);
static_assert(isEnumEqual(CAFF_FORMAT_IYUV, webrtc::VideoType::kIYUV),
              MISMATCH_MSG);
static_assert(isEnumEqual(CAFF_FORMAT_RGB24, webrtc::VideoType::kRGB24),
              MISMATCH_MSG);
static_assert(isEnumEqual(CAFF_FORMAT_ABGR, webrtc::VideoType::kABGR),
              MISMATCH_MSG);
static_assert(isEnumEqual(CAFF_FORMAT_ARGB, webrtc::VideoType::kARGB),
              MISMATCH_MSG);
static_assert(isEnumEqual(CAFF_FORMAT_ARGB4444, webrtc::VideoType::kARGB4444),
              MISMATCH_MSG);
static_assert(isEnumEqual(CAFF_FORMAT_RGB565, webrtc::VideoType::kRGB565),
              MISMATCH_MSG);
static_assert(isEnumEqual(CAFF_FORMAT_ARGB1555, webrtc::VideoType::kARGB1555),
              MISMATCH_MSG);
static_assert(isEnumEqual(CAFF_FORMAT_YUY2, webrtc::VideoType::kYUY2),
              MISMATCH_MSG);
static_assert(isEnumEqual(CAFF_FORMAT_YV12, webrtc::VideoType::kYV12),
              MISMATCH_MSG);
static_assert(isEnumEqual(CAFF_FORMAT_UYVY, webrtc::VideoType::kUYVY),
              MISMATCH_MSG);
static_assert(isEnumEqual(CAFF_FORMAT_MJPEG, webrtc::VideoType::kMJPEG),
              MISMATCH_MSG);
static_assert(isEnumEqual(CAFF_FORMAT_NV21, webrtc::VideoType::kNV21),
              MISMATCH_MSG);
static_assert(isEnumEqual(CAFF_FORMAT_NV12, webrtc::VideoType::kNV12),
              MISMATCH_MSG);
static_assert(isEnumEqual(CAFF_FORMAT_BGRA, webrtc::VideoType::kBGRA),
              MISMATCH_MSG);

#undef MISMATCH_MSG
}  // anonymous namespace

namespace webrtc {
class PeerConnectionFactoryInterface;
class MediaStreamInterface;
class PeerConnectionInterface;
}  // namespace webrtc

namespace caff {
class AudioDevice;
class VideoCapturer;

class Stream {
 public:
  static int const kMaxBitrateBps = 2000000;

  Stream(AudioDevice* audioDevice,
         webrtc::PeerConnectionFactoryInterface* factory);
  virtual ~Stream();

  void Start(
      std::function<std::string(std::string const&)> offerGeneratedCallback,
      std::function<bool(std::vector<IceInfo> const&)> iceGatheredCallback,
      std::function<void()> startedCallback,
      std::function<void(caff_error)> failedCallback);

  void SendAudio(uint8_t const* samples, size_t samples_per_channel);
  void SendVideo(uint8_t const* frameData,
                 size_t frameBytes,
                 int32_t width,
                 int32_t height,
                 caff_format format);

 private:
  std::atomic_bool started{false};
  AudioDevice* audioDevice;
  VideoCapturer* videoCapturer;
  webrtc::PeerConnectionFactoryInterface* factory;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection;
};

}  // namespace caff
