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

#include "api/jsep.h"
#include "api/refcountedbase.h"

#include <future>

namespace caff {

class CreateSessionDescriptionObserver
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  std::future<std::unique_ptr<webrtc::SessionDescriptionInterface>> GetFuture();

  virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
  virtual void OnFailure(std::string const& error) override;

 private:
  std::promise<std::unique_ptr<webrtc::SessionDescriptionInterface>> promise;
};

class SetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
 public:
  std::future<bool> GetFuture();

  virtual void OnSuccess() override;
  virtual void OnFailure(std::string const& error) override;

 private:
  std::promise<bool> promise;
};

}  // namespace caff
