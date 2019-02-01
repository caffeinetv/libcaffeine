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

#include "caffeine.h"
#include "iceinfo.hpp"

#include <functional>
#include <vector>

#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {
class PeerConnectionFactoryInterface;
}

namespace rtc {
class Thread;
}

namespace caff {
class Stream;
class AudioDevice;

class Interface {
 public:
  Interface();

  virtual ~Interface();

  Stream* StartStream(
      std::function<std::string(std::string const&)> offerGeneratedCallback,
      std::function<bool(std::vector<IceInfo> const&)> iceGatheredCallback,
      std::function<void()> startedCallback,
      std::function<void(caff_error)> failedCallback);

 private:
  rtc::scoped_refptr<AudioDevice> audioDevice;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;
  std::unique_ptr<rtc::Thread> networkThread;
  std::unique_ptr<rtc::Thread> workerThread;
  std::unique_ptr<rtc::Thread> signalingThread;
};

}  // namespace caff
