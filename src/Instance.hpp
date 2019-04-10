// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "caffeine.h"
#include "Api.hpp"

#include <functional>
#include <vector>

#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/task_queue.h"

namespace webrtc {
    class PeerConnectionFactoryInterface;
}

namespace rtc {
    class Thread;
}

namespace caff {
    class Broadcast;
    class AudioDevice;

    class Instance {
    public:
        Instance();

        virtual ~Instance();

        caff_AuthResult signIn(char const * username, char const * password, char const * otp);
        caff_AuthResult refreshAuth(char const * refreshToken);
        bool isSignedIn() const;
        void signOut();

        char const * getRefreshToken() const;
        char const * getUsername() const;
        char const * getStageId() const;
        bool canBroadcast() const;

        caff_Error startBroadcast(
            std::string title,
            caff_Rating rating,
            std::function<void()> startedCallback,
            std::function<void(caff_Error)> failedCallback);

        Broadcast * getBroadcast();

        void endBroadcast();

    private:
        caff_AuthResult authenticate(std::function<AuthResponse()> signinFunc);

        rtc::scoped_refptr<AudioDevice> audioDevice;
        rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;
        std::unique_ptr<rtc::Thread> networkThread;
        std::unique_ptr<rtc::Thread> workerThread;
        std::unique_ptr<rtc::Thread> signalingThread;
        rtc::TaskQueue taskQueue; // TODO: only used for disptaching failures; find a better way

        optional<SharedCredentials> sharedCredentials;
        std::unique_ptr<Broadcast> broadcast;

        // copies for sharing with C
        optional<std::string> refreshToken;
        optional<UserInfo> userInfo;
    };

}  // namespace caff
