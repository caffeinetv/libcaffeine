// Copyright 2019 Caffeine Inc. All rights reserved.

#include "SessionDescriptionObserver.hpp"

namespace caff {

    std::future<std::unique_ptr<webrtc::SessionDescriptionInterface>> CreateSessionDescriptionObserver::getFuture() {
        return promise.get_future();
    }

    void CreateSessionDescriptionObserver::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
        promise.set_value(std::unique_ptr<webrtc::SessionDescriptionInterface>(desc));
    }

    void CreateSessionDescriptionObserver::OnFailure(std::string const& error) {
        RTC_LOG(LS_ERROR) << "Failed to create session description: " << error;
        promise.set_value(nullptr);
    }

    std::future<bool> SetSessionDescriptionObserver::getFuture() {
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
