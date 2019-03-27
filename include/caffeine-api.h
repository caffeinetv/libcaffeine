// Copyright 2019 Caffeine Inc. All rights reserved.

#ifndef LIBCAFFEINE_CAFFEINE_API_H
#define LIBCAFFEINE_CAFFEINE_API_H

#include "caffeine.h"

#include <string>
#include <vector>
#include "absl/types/optional.h"

typedef struct caff_auth_response {
    caff_credentials_handle credentials;
    char * next;
    char * mfa_otp_method;
} caff_auth_response;

typedef struct caff_user_info {
    char * caid;
    char * username;
    char * stage_id;
    bool can_broadcast;
} caff_user_info;

typedef struct caff_game_info {
    char * id;
    char * name;
    char ** process_names;
    size_t num_process_names;
} caff_game_info;

typedef struct caff_games {
    caff_game_info ** game_infos;
    size_t num_games;
} caff_games;

typedef struct caff_feed_stream {
    std::string id;
    std::string source_id;
    std::string url;
    std::string sdp_offer;
    std::string sdp_answer;
} caff_feed_stream;

typedef struct caff_feed_capabilities {
    bool video;
    bool audio;
} caff_feed_capabilities;

typedef struct caff_content {
    std::string id;
    std::string type;
} caff_content;

typedef struct caff_feed {
    std::string id;
    std::string client_id;
    std::string role;
    absl::optional<std::string> description;
    absl::optional<std::string> source_connection_quality;
    double volume;
    caff_feed_capabilities capabilities;
    absl::optional<caff_content> content;
    absl::optional<caff_feed_stream> stream;
} caff_feed;

typedef struct caff_stage {
    std::string id;
    std::string username;
    std::string title;
    absl::optional<std::string> broadcast_id;
    bool upsert_broadcast;
    bool live;
    std::vector<caff_feed> feeds;
} caff_stage;

typedef struct caff_stage_request {
    std::string username;
    std::string client_id;
    absl::optional<std::string> cursor;
    absl::optional<caff_stage> stage;
} caff_stage_request;

typedef struct caff_heartbeat_response {
    char * connection_quality;
} caff_heartbeat_response;

CAFFEINE_API void caff_set_string(char ** dest, char const * new_value);

CAFFEINE_API void caff_free_string(char ** str);

CAFFEINE_API bool caff_is_supported_version();

CAFFEINE_API caff_auth_response * caff_signin(char const * username, char const * password, char const * otp);

CAFFEINE_API caff_credentials_handle caff_refresh_auth(char const * refresh_token);

CAFFEINE_API void caff_free_credentials(caff_credentials_handle * creds);

CAFFEINE_API char const * caff_refresh_token(caff_credentials_handle creds);

CAFFEINE_API void caff_free_auth_response(caff_auth_response ** auth_response);

CAFFEINE_API caff_user_info * caff_getuser(caff_credentials_handle creds);

CAFFEINE_API void caff_free_user_info(caff_user_info ** user_info);

CAFFEINE_API caff_games * caff_get_supported_games();

CAFFEINE_API void caff_free_game_info(caff_game_info ** info);
CAFFEINE_API void caff_free_game_list(caff_games ** games);

CAFFEINE_API caff_heartbeat_response * caff_heartbeat_stream(char const * stream_url, caff_credentials_handle creds);

CAFFEINE_API void caff_free_heartbeat_response(caff_heartbeat_response ** response);

CAFFEINE_API bool caff_update_broadcast_screenshot(
    char const * broadcast_id,
    uint8_t const * screenshot_data,
    size_t screenshot_size,
    caff_credentials_handle creds);

#endif // LIBCAFFEINE_CAFFEINE_API_H
