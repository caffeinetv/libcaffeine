// Copyright 2019 Caffeine Inc. All rights reserved.

#include "SessionDescriptionObserver.hpp"

#include "ErrorLogging.hpp"

namespace caff {

    std::future<std::unique_ptr<webrtc::SessionDescriptionInterface>> CreateSessionDescriptionObserver::getFuture() {
        return promise.get_future();
    }

    void CreateSessionDescriptionObserver::OnSuccess(webrtc::SessionDescriptionInterface * desc) {
        promise.set_value(std::unique_ptr<webrtc::SessionDescriptionInterface>(desc));
    }

    void CreateSessionDescriptionObserver::OnFailure(std::string const & error) {
        LOG_ERROR("Failed to create session description: %s", error.c_str());
        promise.set_value(nullptr);
    }

    std::future<bool> SetSessionDescriptionObserver::getFuture() { return promise.get_future(); }

    void SetSessionDescriptionObserver::OnSuccess() { promise.set_value(true); }

    void SetSessionDescriptionObserver::OnFailure(std::string const & error) {
        LOG_ERROR("Failed to set session description: %s", error.c_str());
        promise.set_value(false);
    }

}  // namespace caff
