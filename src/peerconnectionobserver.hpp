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

#include "iceinfo.hpp"

#include "api/peerconnectioninterface.h"

#include <future>

namespace caff {

class PeerConnectionObserver : public webrtc::PeerConnectionObserver {
  RTC_DISALLOW_COPY_AND_ASSIGN(PeerConnectionObserver);

 public:
  PeerConnectionObserver();

  using Candidates = std::vector<IceInfo>;
  std::future<Candidates const&> GetFuture();

  virtual void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override;
  virtual void OnRenegotiationNeeded() override;
  virtual void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  virtual void OnAddStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  virtual void OnRemoveStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  virtual void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
  virtual void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  virtual void OnIceCandidate(
      webrtc::IceCandidateInterface const* candidate) override;

 private:
  std::promise<Candidates const&> promise;
  Candidates gatheredCandidates;
};

}  // namespace caff
