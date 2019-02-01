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

#include "common_types.h"
#include "media/base/videocapturer.h"
#include "rtc_base/refcountedobject.h"

namespace caff {

class VideoCapturer : public cricket::VideoCapturer {
  RTC_DISALLOW_COPY_AND_ASSIGN(VideoCapturer);

 public:
  VideoCapturer() {}
  virtual ~VideoCapturer() {}

  void SendVideo(uint8_t const* frame,
                 size_t frameBytes,
                 int32_t width,
                 int32_t height,
                 webrtc::VideoType format);

  virtual cricket::CaptureState Start(
      cricket::VideoFormat const& format) override;
  virtual void Stop() override;
  virtual bool IsRunning() override;
  virtual bool IsScreencast() const override;
  bool GetPreferredFourccs(std::vector<uint32_t>* fourccs) override;

 private:
  int64_t lastFrameMicros = 0;
};

}  // namespace caff
