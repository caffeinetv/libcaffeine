/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sessiondescriptionobserver.hpp"

namespace caff {

std::future<std::unique_ptr<webrtc::SessionDescriptionInterface>>
CreateSessionDescriptionObserver::GetFuture() {
  return promise.get_future();
}

void CreateSessionDescriptionObserver::OnSuccess(
    webrtc::SessionDescriptionInterface* desc) {
  promise.set_value(std::unique_ptr<webrtc::SessionDescriptionInterface>(desc));
}

void CreateSessionDescriptionObserver::OnFailure(std::string const& error) {
  RTC_LOG(LS_ERROR) << "Failed to create session description: " << error;
  promise.set_value(nullptr);
}

std::future<bool> SetSessionDescriptionObserver::GetFuture() {
  return promise.get_future();
}

void SetSessionDescriptionObserver::OnSuccess() {
  promise.set_value(true);
}

void SetSessionDescriptionObserver::OnFailure(std::string const& error) {
  RTC_LOG(LS_ERROR) << "Failed to create session description: " << error;
  promise.set_value(false);
}

}  // namespace caff
