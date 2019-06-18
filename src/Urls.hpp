// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

// TODO: load these from config? environment?
#if CAFFEINE_STAGING
#    define CAFFEINE_DOMAIN "staging.caffeine.tv/"
#else
#    define CAFFEINE_DOMAIN "caffeine.tv/"
#endif

#define API_ENDPOINT "https://api." CAFFEINE_DOMAIN
#define REALTIME_ENDPOINT "https://realtime." CAFFEINE_DOMAIN
#define EVENTS_ENDPOINT "https://events." CAFFEINE_DOMAIN

// TODO: some of these are deprecated
#define VERSION_CHECK_URL API_ENDPOINT "v1/version-check"
#define SIGNIN_URL API_ENDPOINT "v1/account/signin"
#define REFRESH_TOKEN_URL API_ENDPOINT "v1/account/token"
#define GETGAMES_URL API_ENDPOINT "v1/games"
#define GETUSER_URL(id) (std::string(API_ENDPOINT "v1/users/") + (id))
#define BROADCAST_URL(id) (std::string(API_ENDPOINT "v1/broadcasts/") + (id))

#define REALTIME_GRAPHQL_URL REALTIME_ENDPOINT "public/graphql/query"
#define STREAM_HEARTBEAT_URL(streamUrl) (std::string((streamUrl)) + "/heartbeat")

#define BROADCAST_METRICS_URL EVENTS_ENDPOINT "v1/broadcast_metrics"
