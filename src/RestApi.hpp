// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "CaffQL.hpp"
#include "ErrorLogging.hpp"
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
    using absl::holds_alternative;
    using absl::nullopt;
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

    using ScreenshotData = std::vector<uint8_t>;

    struct HeartbeatResponse {
        caff_ConnectionQuality connectionQuality;
    };

    struct EncoderSettings {
        int width;
        int height;
        int targetBitrate;
        int framerate;
    };

    struct EncoderInfoResponse {
        std::string encoderType;
        EncoderSettings setting;
    };

    std::chrono::duration<long long> backoffDuration(size_t tryNum);

    optional<HeartbeatResponse> heartbeatStream(std::string const & streamUrl, SharedCredentials & creds);

    bool updateScreenshot(
            std::string broadcastId, ScreenshotData const & screenshotData, SharedCredentials & sharedCreds);

    optional<GameList> getSupportedGames();

    caff_Result checkVersion();

    AuthResponse signIn(char const * username, char const * password, char const * otp);

    AuthResponse refreshAuth(char const * refreshToken);

    bool refreshCredentials(SharedCredentials & creds);

    optional<UserInfo> getUserInfo(SharedCredentials & creds);

    bool trickleCandidates(
            std::vector<caff::IceInfo> const & candidates,
            std::string const & streamUrl,
            SharedCredentials & credentials);

    void sendWebrtcStats(SharedCredentials & creds, Json const & report);

    optional<Json> graphqlRawRequest(SharedCredentials & creds, Json const & requestJson);

    optional<EncoderInfoResponse> getEncoderInfo(SharedCredentials & creds);

    template <typename OperationField, typename... Args>
    optional<typename OperationField::ResponseData> graphqlRequest(SharedCredentials & creds, Args const &... args) {
        static_assert(
                OperationField::operation != caffql::Operation::Subscription,
                "graphqlRequest only supports query and mutation operations");

        auto requestJson = OperationField::request(args...);
        auto rawResponse = graphqlRawRequest(creds, requestJson);

        if (!rawResponse) {
            return {};
        }

        caffql::GraphqlResponse<typename OperationField::ResponseData> response;

        try {
            response = OperationField::response(rawResponse);
        } catch (...) {
            LOG_ERROR("Failed to unpack graphql response");
            return {};
        }

        if (auto data = get_if<typename OperationField::ResponseData>(&response)) {
            return std::move(*data);
        } else {
            auto const & errors = get<1>(response);

            if (errors.empty()) {
                LOG_ERROR("Empty error list for graphql request");
            }

            for (auto const & error : errors) {
                LOG_ERROR("Encountered graphql error with message: %s", error.message.c_str());
            }

            return {};
        }
    }

} // namespace caff
