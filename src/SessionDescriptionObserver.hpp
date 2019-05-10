// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include <future>

#include "api/jsep.h"
#include "api/ref_counted_base.h"

namespace caff {

    class CreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver {
    public:
        std::future<std::unique_ptr<webrtc::SessionDescriptionInterface>> getFuture();

        virtual void OnSuccess(webrtc::SessionDescriptionInterface * desc) override;
        virtual void OnFailure(std::string const & error) override;

    private:
        std::promise<std::unique_ptr<webrtc::SessionDescriptionInterface>> promise;
    };

    class SetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
    public:
        std::future<bool> getFuture();

        virtual void OnSuccess() override;
        virtual void OnFailure(std::string const & error) override;

    private:
        std::promise<bool> promise;
    };

} // namespace caff
