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

        caff_AuthResult signin(char const * username, char const * password, char const * otp);
        caff_AuthResult refreshAuth(char const * refreshToken);
        bool isSignedIn() const;
        void signout();

        char const * getRefreshToken() const;
        char const * getUsername() const;
        char const * getStageId() const;
        bool canBroadcast() const;

        Stream* startStream(
            std::string title,
            caff_Rating rating,
            std::function<void()> startedCallback,
            std::function<void(caff_Error)> failedCallback);

    private:
        caff_AuthResult updateUserInfo(caff_AuthResult origResult);

        rtc::scoped_refptr<AudioDevice> audioDevice;
        rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;
        std::unique_ptr<rtc::Thread> networkThread;
        std::unique_ptr<rtc::Thread> workerThread;
        std::unique_ptr<rtc::Thread> signalingThread;

        optional<SharedCredentials> sharedCredentials;

        // copies for sharing with C
        optional<std::string> refreshToken;
        optional<UserInfo> userInfo;
    };

}  // namespace caff
