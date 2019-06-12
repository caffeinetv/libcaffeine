// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "RestApi.hpp"
#include "caffeine.h"

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

        caff_Result signIn(char const * username, char const * password, char const * otp);
        caff_Result refreshAuth(char const * refreshToken);
        bool isSignedIn() const;
        void signOut();

        char const * getRefreshToken() const;
        char const * getUsername() const;
        bool canBroadcast() const;

        caff_Result enumerateGames(std::function<void(char const *, char const *, char const *)> enumerator);

        caff_Result startBroadcast(
                std::string title,
                caff_Rating rating,
                std::string gameId,
                std::function<void()> startedCallback,
                std::function<void(caff_Result)> failedCallback);

        std::shared_ptr<Broadcast> getBroadcast();

        void endBroadcast();

    private:
        caff_Result authenticate(std::function<AuthResponse()> signinFunc);

        rtc::scoped_refptr<AudioDevice> audioDevice;
        rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;
        std::unique_ptr<rtc::Thread> networkThread;
        std::unique_ptr<rtc::Thread> workerThread;
        std::unique_ptr<rtc::Thread> signalingThread;
        rtc::TaskQueue taskQueue;  // TODO: only used for disptaching failures; maybe find a better way

        optional<SharedCredentials> sharedCredentials;
        std::shared_ptr<Broadcast> broadcast;
        std::mutex broadcastMutex;

        // copies for sharing with C
        optional<std::string> refreshToken;
        optional<UserInfo> userInfo;
        optional<GameList> gameList;
    };

}  // namespace caff
