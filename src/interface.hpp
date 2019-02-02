// Copyright 2019 Caffeine Inc. All rights reserved.

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
