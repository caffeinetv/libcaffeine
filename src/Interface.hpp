// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "caffeine.h"
#include "Api.hpp"

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
            Credentials * credentials,
            std::string username,
            std::string title,
            caff_rating rating,
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
