#include "Serialization.hpp"

#include "ErrorLogging.hpp"

#include <sstream>

namespace caff {
    void from_json(Json const & json, Credentials & credentials) {
        get_value_to(json, "access_token", credentials.accessToken);
        get_value_to(json, "refresh_token", credentials.refreshToken);
        get_value_to(json, "caid", credentials.caid);
        get_value_to(json, "credential", credentials.credential);
    }

    void from_json(Json const & json, UserInfo & userInfo) {
        caff::get_value_to(json, "username", userInfo.username);
        caff::get_value_to(json, "can_broadcast", userInfo.canBroadcast);
    }

    void from_json(Json const & json, GameInfo & gameInfo) {
        auto idNum = json.at("id").get<size_t>();
        std::ostringstream idStream;
        idStream << idNum;

        gameInfo.id = idStream.str();
        caff::get_value_to(json, "name", gameInfo.name);

        for (auto & processName : json.at("process_names")) {
            try {
                auto processNameStr = processName.get<std::string>();
                if (processNameStr.empty()) {
                    LOG_DEBUG("Skipping empty process name");
                    continue;
                }
                gameInfo.processNames.push_back(std::move(processNameStr));
            } catch (...) {
                LOG_DEBUG("Skipping unreadable process name");
            }
        }
    }

    void to_json(Json & json, IceInfo const & iceInfo) {
        set_value_from(json, "candidate", iceInfo.sdp);
        set_value_from(json, "sdpMid", iceInfo.sdpMid);
        set_value_from(json, "sdpMLineIndex", iceInfo.sdpMLineIndex);
    }

    void from_json(Json const & json, HeartbeatResponse & response) {
        get_value_to(json, "connection_quality", response.connectionQuality);
    }

    void from_json(Json const & json, EncoderInfoResponse & response) {
        get_value_to(json, "encoder_type", response.encoderType);

        get_value_to(json["encoder_setting"], "bitrate", response.setting.bitrate);
        get_value_to(json["encoder_setting"], "framerate", response.setting.framerate);
        get_value_to(json["encoder_setting"], "width", response.setting.width);
        get_value_to(json["encoder_setting"], "height", response.setting.height);
    }

    static bool isWhitelistedReportType(webrtc::StatsReport::StatsType type) {
        switch (type) {
        case webrtc::StatsReport::kStatsReportTypeSsrc:
        case webrtc::StatsReport::kStatsReportTypeBwe:
            return true;
        default:
            return false;
        }
    }

    static bool isWhitelistedStat(webrtc::StatsReport::StatsValueName stat) {
        using Stat = webrtc::StatsReport;

        switch (stat) {
        // standard common
        case Stat::kStatsValueNameBytesSent:
        case Stat::kStatsValueNameMediaType:
        case Stat::kStatsValueNamePacketsLost:
        case Stat::kStatsValueNamePacketsSent:

        // goog prefixed common
        case Stat::kStatsValueNameEncodeUsagePercent:
        case Stat::kStatsValueNameAvgEncodeMs:
        case Stat::kStatsValueNameRtt:
        case Stat::kStatsValueNameNacksReceived:

        // video
        case Stat::kStatsValueNameFrameRateInput:
        case Stat::kStatsValueNameFrameRateSent:
        case Stat::kStatsValueNameFrameHeightInput:
        case Stat::kStatsValueNameFrameHeightSent:
        case Stat::kStatsValueNameFrameWidthInput:
        case Stat::kStatsValueNameFrameWidthSent:
        case Stat::kStatsValueNameCpuLimitedResolution:
        case Stat::kStatsValueNameFirsReceived:

        // audio
        case Stat::kStatsValueNameAudioInputLevel:

        // videobwe stats
        case Stat::kStatsValueNameAvailableSendBandwidth:
        case Stat::kStatsValueNameTargetEncBitrate:
        case Stat::kStatsValueNameActualEncBitrate:
        case Stat::kStatsValueNameTransmitBitrate:
            return true;
        default:
            return false;
        }
    }

    static void addStat(Json & json, webrtc::StatsReport::Value const & stat) {
        using Type = webrtc::StatsReport::Value;
        switch (stat.type()) {
        case Type::kBool:
            json[stat.display_name()] = stat.bool_val();
            break;
        case Type::kFloat:
            json[stat.display_name()] = stat.float_val();
            break;
        case Type::kId:
            // id_val() is declared in statstypes.h but there is no implementation
            LOG_DEBUG("Unexpected ID stat: %s", stat.display_name());
            break;
        case Type::kInt:
            json[stat.display_name()] = stat.int_val();
            break;
        case Type::kInt64:
            json[stat.display_name()] = stat.int64_val();
            break;
        case Type::kStaticString:
            json[stat.display_name()] = stat.static_string_val();
            break;
        case Type::kString:
            json[stat.display_name()] = stat.string_val();
            break;
        }
    }

    Json serializeWebrtcStats(webrtc::StatsReports const & reports) {
        auto serialized = Json::array();
        for (auto const * report : reports) {
            if (isWhitelistedReportType(report->type())) {
                serialized.push_back({
                        { "caffeineUnixTimestamp", report->timestamp() },
                        { "caffeineReportType", report->TypeToString() },
                });

                for (auto const & entry : report->values()) {
                    if (isWhitelistedStat(entry.first)) {
                        addStat(serialized.back(), *entry.second);
                    }
                }
            }
        }
        return serialized;
    }

} // namespace caff
