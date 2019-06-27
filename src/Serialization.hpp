// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "RestApi.hpp"

#include "ErrorLogging.hpp"

#include "api/statstypes.h"
#include "nlohmann/json.hpp"

// C datatype serialization has to be outside of caff namespace

NLOHMANN_JSON_SERIALIZE_ENUM(
        caff_ConnectionQuality,
        { { caff_ConnectionQualityGood, "GOOD" },
          { caff_ConnectionQualityPoor, "POOR" },
          { caff_ConnectionQualityUnknown, nullptr } })

namespace caff {
    using Json = nlohmann::json;

    template <typename T> inline void get_value_to(Json const & json, char const * key, T & target) {
        json.at(key).get_to(target);
    }

    template <typename T> inline void get_value_to(Json const & json, char const * key, optional<T> & target) {
        auto it = json.find(key);
        if (it != json.end()) {
            it->get_to(target);
        } else {
            target.reset();
        }
    }

    template <typename T> inline void set_value_from(Json & json, char const * key, T const & source) {
        json[key] = source;
    }

    template <typename T> inline void set_value_from(Json & json, char const * key, optional<T> const & source) {
        if (source) {
            json[key] = *source;
            return;
        }
        auto it = json.find(key);
        if (it != json.end()) {
            json.erase(it);
        }
    }

    void from_json(Json const & json, Credentials & credentials);
    void from_json(Json const & json, UserInfo & userInfo);
    void from_json(Json const & json, GameInfo & gameInfo);
    void to_json(Json & json, IceInfo const & iceInfo);
    void from_json(Json const & json, HeartbeatResponse & response);

    // The WebRTC types fail nlohmann's "compatibility" checks when using the to_json overload, either as a free
    // function or as an adl_serializer specialization
    Json serializeWebrtcStats(webrtc::StatsReports const & reports);
} // namespace caff
