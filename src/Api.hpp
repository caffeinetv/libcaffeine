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
#include "websocketpp/client.hpp"
#include "websocketpp/config/asio_client.hpp"

namespace caff {
    // TODO: Get C++17 working and use standard versions
    using absl::get;
    using absl::get_if;
    using absl::optional;
    using absl::variant;
    using Json = nlohmann::json;

    extern std::string clientType;
    extern std::string clientVersion;

    extern std::string const realtimeGraphqlURL;

    class WebsocketClient {

    public:
        WebsocketClient();
        ~WebsocketClient();

        static std::unique_ptr<WebsocketClient> shared;

        using Connection = websocketpp::connection_hdl;

        enum ConnectionEndType { Failed, Closed };

        optional<Connection> connect(
                std::string url,
                std::function<void(Connection)> openedCallback,
                std::function<void(Connection, ConnectionEndType)> endedCallback,
                std::function<void(Connection, std::string const &)> messageReceivedCallback);

        void sendMessage(Connection const & connection, std::string const & message);

        void close(Connection const & connection);

    private:
        using Client = websocketpp::client<websocketpp::config::asio_tls_client>;
        Client client;
        optional<std::thread> clientThread;
    };

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

    caff_Result checkVersion();

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

    optional<Json> graphqlRawRequest(SharedCredentials & creds, Json const & requestJson);

    template <typename OperationField, typename... Args>
    optional<typename OperationField::ResponseData> graphqlRequest(SharedCredentials & creds, Args const &... args) {

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
            return *data;
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

    template <typename OperationField, typename... Args>
    optional<WebsocketClient::Connection> graphqlSubscription(
            WebsocketClient & client,
            std::function<void(caffql::GraphqlResponse<typename OperationField::ResponseData>)> messageHandler,
            std::function<void(WebsocketClient::ConnectionEndType)> endedHandler,
            SharedCredentials & creds,
            Args const &... args) {
        std::string credential;
        {
            auto locked = creds.lock();
            credential = locked.credentials.credential;
        }
        Json connectionInit{ { "type", "connection_init" },
                             { "payload", Json::object({ { "X-Credential", std::move(credential) } }) } };
        auto requestJson = OperationField::request(args...);
        Json connectionParams = Json::object({ { "type", "start" }, { "payload", requestJson } });

        auto opened = [init = std::move(connectionInit), params = std::move(connectionParams), &client](
                              WebsocketClient::Connection connection) {
            client.sendMessage(connection, init.dump());
            client.sendMessage(connection, params.dump());
        };

        auto ended = [=](WebsocketClient::Connection connection, WebsocketClient::ConnectionEndType endType) {
            if (endedHandler) {
                endedHandler(endType);
            }
        };

        auto messageReceived = [=](WebsocketClient::Connection connection, std::string const & message) {
            Json json;
            try {
                json = Json::parse(message);
            } catch (...) {
                LOG_ERROR("Failed to parse graphql subscription message");
                return;
            }

            auto typeIt = json.find("type");
            if (typeIt == json.end() || *typeIt != "data") {
                return;
            }

            caffql::GraphqlResponse<typename OperationField::ResponseData> response;

            try {
                response = OperationField::response(json.at("payload"));
            } catch (std::exception const & error) {
                LOG_ERROR("Failed to unpack graphql subscription message: %s", error.what());
                return;
            }

            if (messageHandler) {
                messageHandler(response);
            }
        };

        return client.connect(realtimeGraphqlURL, opened, ended, messageReceived);
    }

} // namespace caff
