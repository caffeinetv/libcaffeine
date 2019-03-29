#pragma once

// Copyright 2019 Caffeine Inc. All rights reserved.

#include "caffeine.h"
#include "iceinfo.hpp"

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include "absl/types/optional.h"

namespace caff {

    struct Credentials {
        std::string accessToken;
        std::string refreshToken;
        std::string caid;
        std::string credential;

        std::mutex mutex; // TODO: there's surely a better place for this

        Credentials(
            std::string accessToken,
            std::string refreshToken,
            std::string caid,
            std::string credential)
            : accessToken(std::move(accessToken))
            , refreshToken(std::move(refreshToken))
            , caid(std::move(caid))
            , credential(std::move(credential))
        { }
    };

    struct FeedStream {
        absl::optional<std::string> id;
        absl::optional<std::string> sourceId;
        absl::optional<std::string> url;
        absl::optional<std::string> sdpOffer;
        absl::optional<std::string> sdpAnswer;
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
        absl::optional<std::string> clientId;
        absl::optional<std::string> role;
        absl::optional<std::string> description;
        absl::optional<caff_connection_quality> sourceConnectionQuality;
        absl::optional<double> volume;
        absl::optional<FeedCapabilities> capabilities;
        absl::optional<FeedContent> content;
        absl::optional<FeedStream> stream;
    };

    struct Stage {
        absl::optional<std::string> id;
        absl::optional<std::string> username;
        absl::optional<std::string> title;
        absl::optional<std::string> broadcastId;
        absl::optional<bool> upsertBroadcast;
        absl::optional<bool> live;
        absl::optional<std::map<std::string, Feed>> feeds;
    };

    struct Client {
        std::string id;
        bool headless = true;
        bool constrainedBaseline = false;
    };

    struct StageRequest {
        Client client;
        absl::optional<std::string> cursor;
        absl::optional<Stage> stage;
    };

    struct StageResponse {
        std::string cursor;
        double retryIn;
        absl::optional<Stage> stage;
    };

    struct DisplayMessage {
        std::string title;
        absl::optional<std::string> body;
    };

    struct FailureResponse {
        std::string type;
        absl::optional<std::string> reason;
        absl::optional<DisplayMessage> displayMessage;
    };

    struct StageResponseResult {
        absl::optional<StageResponse> response;
        absl::optional<FailureResponse> failure;
    };

    /*
    struct HeartbeatResponse {
        char * connection_quality;
    };
    void caff_set_string(char ** dest, char const * new_value);

    void caff_free_string(char ** str);

    HeartbeatResponse * caff_heartbeat_stream(char const * stream_url, Credentials * creds);

    void caff_free_heartbeat_response(HeartbeatResponse ** response);

    bool caff_update_broadcast_screenshot(
        char const * broadcastId,
        uint8_t const * screenshot_data,
        size_t screenshot_size,
        Credentials * creds);
    */

    // TODO: not pointers
    caff_games * getSupportedGames();
    bool isSupportedVersion();
    caff_auth_response * caffSignin(char const * username, char const * password, char const * otp);
    Credentials * refreshAuth(char const * refreshToken);
    caff_user_info * getUserInfo(Credentials * creds);

    bool trickleCandidates(
        std::vector<caff::IceInfo> const & candidates,
        std::string const & streamUrl,
        Credentials * credentials);

    StageRequest * createStageRequest(std::string username, std::string clientId);

    bool requestStageUpdate(StageRequest * request, Credentials * creds, double * retryIn, bool * isOutOfCapacity);

}
