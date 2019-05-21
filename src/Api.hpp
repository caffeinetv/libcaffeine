// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "caffeine.h"

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "nlohmann/json.hpp"

namespace caff {
    // TODO: Get C++17 working and use standard versions
    using absl::get;
    using absl::get_if;
    using absl::optional;
    using absl::variant;
    using Json = nlohmann::json;

    extern std::string clientType;
    extern std::string clientVersion;

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
        caff_Result result = caff_ResultFailure;
        optional<Credentials> credentials;

        // Helper for RETRY_REQUEST
        operator bool() const { return result != caff_ResultFailure; }
    };

    struct UserInfo {
        std::string username;
        bool canBroadcast;
    };

    // Supported game detection info
    struct GameInfo {
        std::string id;
        std::string name;
        std::vector<std::string> processNames;
    };

    using GameList = std::vector<GameInfo>;

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

    enum class ContentType { Game, User };

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

        StageRequest(std::string username, std::string clientId) {
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

    using ScreenshotData = std::vector<uint8_t>;

    struct HeartbeatResponse {
        caff_ConnectionQuality connectionQuality;
    };

    optional<HeartbeatResponse> heartbeatStream(std::string const & streamUrl, SharedCredentials & creds);

    bool updateScreenshot(
            std::string broadcastId, ScreenshotData const & screenshotData, SharedCredentials & sharedCreds);

    optional<GameList> getSupportedGames();

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

    void sendWebrtcStats(SharedCredentials & creds, Json const & report);

}  // namespace caff
