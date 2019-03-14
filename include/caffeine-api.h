// Copyright 2019 Caffeine Inc. All rights reserved.

#ifndef LIBCAFFEINE_CAFFEINE_API_H
#define LIBCAFFEINE_CAFFEINE_API_H

#include "caffeine.h"

struct caff_credentials;
typedef struct caff_credentials * caff_credentials_handle;

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

/* TODO: game detection will be handled behind the scenes */
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

typedef enum caff_rating {
    CAFF_RATING_NONE,
    CAFF_RATING_SEVENTEEN_PLUS,
    CAFF_RATING_MAX,
} caff_rating;

typedef struct caff_feed_stream {
    char * id;
    char * source_id;
    char * url;
    char * sdp_offer;
    char * sdp_answer;
} caff_feed_stream;

typedef struct caff_feed_capabilities {
    bool video;
    bool audio;
} caff_feed_capabilities;

typedef struct caff_content {
    char * id;
    char * type;
} caff_content;

typedef struct caff_feed {
    char * id;
    char * client_id;
    char * role;
    char * description;
    char * source_connection_quality;
    double volume;
    caff_feed_capabilities capabilities;
    caff_content content;
    caff_feed_stream stream;
} caff_feed;

typedef struct caff_stage {
    char * id;
    char * username;
    char * title;
    char * broadcast_id;
    bool upsert_broadcast;
    bool live;
    caff_feed * feeds;
    size_t num_feeds;
} caff_stage;

typedef struct caff_stage_request {
    char * username;
    char * client_id;
    char * cursor;
    caff_stage * stage;
} caff_stage_request;

typedef struct caff_heartbeat_response {
    char * connection_quality;
} caff_heartbeat_response;

CAFFEINE_API char * caff_create_unique_id();

CAFFEINE_API void caff_free_unique_id(char ** id);

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

CAFFEINE_API char * caff_annotate_title(char const * title, enum caff_rating rating);

CAFFEINE_API bool caff_trickle_candidates(
    caff_ice_candidates candidates,
    size_t num_candidates,
    char const * stream_url,
    caff_credentials_handle creds);

CAFFEINE_API caff_heartbeat_response * caff_heartbeat_stream(char const * stream_url, caff_credentials_handle creds);

CAFFEINE_API void caff_free_heartbeat_response(caff_heartbeat_response ** response);

CAFFEINE_API bool caff_update_broadcast_screenshot(
    char const * broadcast_id,
    uint8_t const * screenshot_data,
    size_t screenshot_size,
    caff_credentials_handle creds);

CAFFEINE_API caff_feed * caff_get_stage_feed(caff_stage * stage, char const * id);
// Replaces any existing feeds, copies passed in data
CAFFEINE_API void caff_set_stage_feed(caff_stage * stage, caff_feed const * feed);
CAFFEINE_API void caff_clear_stage_feeds(caff_stage * stage);
CAFFEINE_API void caff_free_stage(caff_stage ** stage);

CAFFEINE_API caff_stage_request * caff_copy_stage_request(caff_stage_request const * request);
CAFFEINE_API void caff_free_stage_request(caff_stage_request ** request);

// If successful, updates request cursor and stage with the response values
CAFFEINE_API bool caff_request_stage_update(
    caff_stage_request * request,
    caff_credentials_handle creds,
    double * retry_in,
    bool * is_out_of_capacity);

#endif // LIBCAFFEINE_CAFFEINE_API_H
