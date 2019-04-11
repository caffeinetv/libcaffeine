#pragma once

// Copyright 2019 Caffeine Inc. All rights reserved.

#include "caffeine.h"

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include "absl/types/optional.h"
#include "absl/types/variant.h"

namespace caff {
    // TODO: Get C++17 working and use standard versions
    using absl::optional;
    using absl::variant;
    using absl::get;
    using absl::get_if;

    struct Credentials {
        std::string accessToken;
        std::string refreshToken;
        std::string caid;
        std::string credential;
    };

    class SharedCredentials;

    class LockedCredentials {
    public:
        Credentials & credentials;

    private:
        friend class SharedCredentials;
        explicit LockedCredentials(SharedCredentials & credentials);
        std::unique_lock<std::mutex> lock;
    };

    class SharedCredentials {
    public:
        explicit SharedCredentials(Credentials credentials);
        LockedCredentials lock();

    private:
        friend class LockedCredentials;
        Credentials credentials;
        std::mutex mutex;
    };

    struct AuthResponse {
        caff_Result result = caff_ResultRequestFailed;
        optional<Credentials> credentials;

        // Helper for RETRY_REQUEST
        operator bool() const {
            return result != caff_ResultRequestFailed;
        }
    };

    struct UserInfo {
        std::string username;
        std::string stageId;
        bool canBroadcast;
    };

    struct IceInfo {
        std::string sdp;
        std::string sdpMid;
        int sdpMLineIndex;
    };

    struct FeedStream {
        optional<std::string> id;
        optional<std::string> sourceId;
        optional<std::string> url;
        optional<std::string> sdpOffer;
        optional<std::string> sdpAnswer;
    };

    struct FeedCapabilities {
        bool video;
        bool audio;
    };

    enum class ContentType {
        Game,
        User
    };

    struct FeedContent {
        std::string id;
        ContentType type;
    };

    struct Feed {
        std::string id;
        optional<std::string> clientId;
        optional<std::string> role;
        optional<std::string> description;
        optional<caff_ConnectionQuality> sourceConnectionQuality;
        optional<double> volume;
        optional<FeedCapabilities> capabilities;
        optional<FeedContent> content;
        optional<FeedStream> stream;
    };

    struct Stage {
        optional<std::string> id;
        optional<std::string> username;
        optional<std::string> title;
        optional<std::string> broadcastId;
        optional<bool> upsertBroadcast;
        optional<bool> live;
        optional<std::map<std::string, Feed>> feeds;
    };

    struct Client {
        std::string id;
        bool headless = true;
        bool constrainedBaseline = false;
    };

    struct StageRequest {
        Client client;
        optional<std::string> cursor;
        optional<Stage> stage;

        StageRequest(std::string username, std::string clientId)
        {
            client.id = std::move(clientId);
            stage = Stage{};
            stage->username = std::move(username);
        }
    };

    struct StageResponse {
        std::string cursor;
        std::chrono::milliseconds retryIn;
        optional<Stage> stage;
    };

    struct DisplayMessage {
        std::string title;
        optional<std::string> body;
    };

    struct FailureResponse {
        std::string type;
        optional<std::string> reason;
        optional<DisplayMessage> displayMessage;
    };

    using StageResponseResult = variant<StageResponse, FailureResponse>;

    struct HeartbeatResponse {
        caff_ConnectionQuality connectionQuality;
    };

    optional<HeartbeatResponse> heartbeatStream(std::string const & streamUrl, SharedCredentials & creds);

    /*
    bool caff_update_broadcast_screenshot(
        char const * broadcastId,
        uint8_t const * screenshot_data,
        size_t screenshot_size,
        Credentials * creds);
    */

    // TODO: not pointers
    caff_GameList * getSupportedGames();
    bool isSupportedVersion();
    AuthResponse signIn(char const * username, char const * password, char const * otp);
    AuthResponse refreshAuth(char const * refreshToken);
    optional<UserInfo> getUserInfo(SharedCredentials & creds);

    bool trickleCandidates(
        std::vector<caff::IceInfo> const & candidates,
        std::string const & streamUrl,
        SharedCredentials & credentials);

    bool requestStageUpdate(
        StageRequest & request,
        SharedCredentials & creds,
        std::chrono::milliseconds * retryIn,
        bool * isOutOfCapacity);

}
