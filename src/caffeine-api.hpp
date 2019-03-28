#pragma once

// Copyright 2019 Caffeine Inc. All rights reserved.

#include "caffeine.h"

#include <string>
#include <vector>
#include "absl/types/optional.h"

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

void caff_set_string(char ** dest, char const * new_value);

void caff_free_string(char ** str);

caff_heartbeat_response * caff_heartbeat_stream(char const * stream_url, caff_credentials_handle creds);

void caff_free_heartbeat_response(caff_heartbeat_response ** response);

bool caff_update_broadcast_screenshot(
    char const * broadcast_id,
    uint8_t const * screenshot_data,
    size_t screenshot_size,
    caff_credentials_handle creds);
