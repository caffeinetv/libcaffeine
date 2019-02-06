#pragma once

#include <caffeine.h>
#include <string>

struct caff_credentials;

struct caff_auth_response {
    struct caff_credentials * credentials;
    char * next;
    char * mfa_otp_method;
};

struct caff_user_info {
    char * caid;
    char * username;
    char * stage_id;
    bool can_broadcast;
};

/* TODO: game detection will be handled behind the scenes */
struct caff_game_info {
    char * id;
    char * name;
    char ** process_names;
    size_t num_process_names;
};

struct caff_games {
    struct caff_game_info ** game_infos;
    size_t num_games;
};

enum caff_rating {
    CAFF_RATING_NONE,
    CAFF_RATING_SEVENTEEN_PLUS,
    CAFF_RATING_MAX,
};

struct caff_feed_stream {
    char * id;
    char * source_id;
    char * url;
    char * sdp_offer;
    char * sdp_answer;
};

struct caff_feed_capabilities {
    bool video;
    bool audio;
};

struct caff_content {
    char * id;
    char * type;
};

struct caff_feed {
    char * id;
    char * client_id;
    char * role;
    char * description;
    char * source_connection_quality;
    double volume;
    struct caff_feed_capabilities capabilities;
    struct caff_content content;
    struct caff_feed_stream stream;
};

struct caff_stage {
    char * id;
    char * username;
    char * title;
    char * broadcast_id;
    bool upsert_broadcast;
    bool live;
    struct caff_feed * feeds;
    size_t num_feeds;
};

struct caff_stage_request {
    char * username;
    char * client_id;
    char * cursor;
    struct caff_stage * stage;
};

std::string caff_generate_unique_id();

bool caff_is_supported_version();

struct caff_heartbeat_response {
    char * connection_quality;
};

struct caff_auth_response * caff_signin(char const * username, char const * password, char const * otp);

struct caff_credentials * caff_refresh_auth(char const * refresh_token);

void caff_free_credentials(struct caff_credentials ** creds);

char const * caff_refresh_token(struct caff_credentials * creds);

void caff_free_auth_response(struct caff_auth_response ** auth_response);

struct caff_user_info * caff_getuser(struct caff_credentials * creds);

void caff_free_user_info(struct caff_user_info ** user_info);

struct caff_games * caff_get_supported_games();

void caff_free_game_info(struct caff_game_info ** info);
void caff_free_game_list(struct caff_games ** games);

char * caff_annotate_title(char const * title, enum caff_rating rating);

bool caff_trickle_candidates(
    caff_ice_candidates candidates,
    size_t num_candidates,
    char const * stream_url,
    struct caff_credentials * creds);

struct caff_heartbeat_response * caff_heartbeat_stream(
    char const * stream_url,
    struct caff_credentials * creds);

void caff_free_heartbeat_response(struct caff_heartbeat_response ** response);

bool caff_update_broadcast_screenshot(
    char const * broadcast_id,
    uint8_t const * screenshot_data,
    size_t screenshot_size,
    struct caff_credentials * creds);

struct caff_feed * caff_get_stage_feed(struct caff_stage * stage, char const * id);
// Replaces any existing feeds, copies passed in data
void caff_set_stage_feed(struct caff_stage * stage, struct caff_feed const * feed);
void caff_clear_stage_feeds(struct caff_stage * stage);
void caff_free_stage(struct caff_stage ** stage);

struct caff_stage_request * caff_copy_stage_request(struct caff_stage_request const * request);
void caff_free_stage_request(struct caff_stage_request ** request);

// If successful, updates request cursor and stage with the response values
bool caff_request_stage_update(
    struct caff_stage_request * request,
    struct caff_credentials * creds,
    double * retry_in,
    bool * is_out_of_capacity);
