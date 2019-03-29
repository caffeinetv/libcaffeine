#pragma once

// Copyright 2019 Caffeine Inc. All rights reserved.

#include "caffeine.h"

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include "absl/types/optional.h"

struct Credentials {
    std::string access_token;
    std::string refresh_token;
    std::string caid;
    std::string credential;

    std::mutex mutex; // TODO: there's surely a better place for this

    Credentials(
        std::string access_token,
        std::string refresh_token,
        std::string caid,
        std::string credential)
        : access_token(std::move(access_token))
        , refresh_token(std::move(refresh_token))
        , caid(std::move(caid))
        , credential(std::move(credential))
    { }
};

struct FeedStream {
    absl::optional<std::string> id;
    absl::optional<std::string> source_id;
    absl::optional<std::string> url;
    absl::optional<std::string> sdp_offer;
    absl::optional<std::string> sdp_answer;
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
    absl::optional<std::string> client_id;
    absl::optional<std::string> role;
    absl::optional<std::string> description;
    absl::optional<caff_connection_quality> source_connection_quality;
    absl::optional<double> volume;
    absl::optional<FeedCapabilities> capabilities;
    absl::optional<FeedContent> content;
    absl::optional<FeedStream> stream;
};

struct Stage {
    absl::optional<std::string> id;
    absl::optional<std::string> username;
    absl::optional<std::string> title;
    absl::optional<std::string> broadcast_id;
    absl::optional<bool> upsert_broadcast;
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
    double retry_in;
    absl::optional<Stage> stage;
};

struct DisplayMessage {
    std::string title;
    absl::optional<std::string> body;
};

struct FailureResponse {
    std::string type;
    absl::optional<std::string> reason;
    absl::optional<DisplayMessage> display_message;
};

struct StageResponseResult {
    absl::optional<StageResponse> response;
    absl::optional<FailureResponse> failure;
};

struct HeartbeatResponse {
    char * connection_quality;
};

void caff_set_string(char ** dest, char const * new_value);

void caff_free_string(char ** str);

HeartbeatResponse * caff_heartbeat_stream(char const * stream_url, Credentials * creds);

void caff_free_heartbeat_response(HeartbeatResponse ** response);

bool caff_update_broadcast_screenshot(
    char const * broadcast_id,
    uint8_t const * screenshot_data,
    size_t screenshot_size,
    Credentials * creds);

Credentials * refresh_auth(char const * refresh_token);
void free_credentials(Credentials ** creds);
char const * refresh_token(Credentials * creds);
caff_user_info * getuser(Credentials * creds);
