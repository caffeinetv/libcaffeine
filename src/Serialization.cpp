#include "Serialization.hpp"

#include <sstream>

void from_json(nlohmann::json const & json, caff_GameInfo & gameInfo)
{
    auto idNum = json.at("id").get<size_t>();
    std::ostringstream idStream;
    idStream << idNum;

    gameInfo.id = caff::cstrdup(idStream.str());
    caff::get_value_to(json, "name", gameInfo.name);

    auto const & processNames = json.at("process_names");
    gameInfo.numProcessNames = processNames.size();
    if (gameInfo.numProcessNames == 0) {
        gameInfo.processNames = nullptr;
        return;
    }

    gameInfo.processNames = new char *[gameInfo.numProcessNames] {0};
    for (size_t i = 0; i < gameInfo.numProcessNames; ++i) {
        try {
            caff::get_value_to(processNames, i, gameInfo.processNames[i]);
        }
        catch (...) {
            //LOG_WARN("Unable to read process name; ignoring");
        }
    }
}

void from_json(nlohmann::json const & json, caff_GameList & games)
{
    games.numGames = json.size();
    if (games.numGames == 0) {
        games.gameInfos = nullptr;
        return;
    }

    games.gameInfos = new caff_GameInfo *[games.numGames] {0};
    for (size_t i = 0; i < games.numGames; ++i) {
        try {
            games.gameInfos[i] = new caff_GameInfo(json.at(i));
        }
        catch (...) {
            //LOG_WARN("Unable to read game info; ignoring");
        }
    }
}

namespace caff {
    char * cstrdup(char const * str) {
        if (!str) {
            return nullptr;
        }

        auto copylen = strlen(str) + 1;
        auto result = new char[copylen];
        strncpy(result, str, copylen);
        return result;
    }

    char * cstrdup(std::string const & str) {
        if (str.empty()) {
            return nullptr;
        }

        auto result = new char[str.length() + 1];
        str.copy(result, str.length());
        result[str.length()] = 0;
        return result;
    }

    void from_json(Json const & json, Credentials & credentials)
    {
        get_value_to(json, "access_token", credentials.accessToken);
        get_value_to(json, "refresh_token", credentials.refreshToken);
        get_value_to(json, "caid", credentials.caid);
        get_value_to(json, "credential", credentials.credential);
    }

    void from_json(Json const & json, UserInfo & userInfo)
    {
        caff::get_value_to(json, "username", userInfo.username);
        caff::get_value_to(json, "stage_id", userInfo.stageId);
        caff::get_value_to(json, "can_broadcast", userInfo.canBroadcast);
    }

    void to_json(Json & json, IceInfo const & iceInfo)
    {
        set_value_from(json, "candidate", iceInfo.sdp);
        set_value_from(json, "sdpMid", iceInfo.sdpMid);
        set_value_from(json, "sdpMLineIndex", iceInfo.sdpMLineIndex);
    }

    void to_json(Json & json, Client const & client)
    {
        set_value_from(json, "id", client.id);
        set_value_from(json, "headless", client.headless);
        set_value_from(json, "constrained_baseline", client.constrainedBaseline);
    }

    void to_json(Json & json, FeedCapabilities const & capabilities)
    {
        set_value_from(json, "video", capabilities.video);
        set_value_from(json, "audio", capabilities.audio);
    }

    void from_json(Json const & json, FeedCapabilities & capabilities)
    {
        get_value_to(json, "audio", capabilities.audio);
        get_value_to(json, "video", capabilities.video);
    }

    void to_json(Json & json, FeedContent const & content)
    {
        set_value_from(json, "id", content.id);
        set_value_from(json, "type", content.type);
    }

    void from_json(Json const & json, FeedContent & content)
    {
        get_value_to(json, "id", content.id);
        get_value_to(json, "type", content.type);
    }

    void to_json(Json & json, FeedStream const & stream)
    {
        set_value_from(json, "id", stream.id);
        set_value_from(json, "source_id", stream.sourceId);
        set_value_from(json, "url", stream.url);
        set_value_from(json, "sdp_offer", stream.sdpOffer);
        set_value_from(json, "sdp_answer", stream.sdpAnswer);
    }

    void from_json(Json const & json, FeedStream & stream)
    {
        get_value_to(json, "id", stream.id);
        get_value_to(json, "source_id", stream.sourceId);
        get_value_to(json, "url", stream.url);
        get_value_to(json, "sdp_offer", stream.sdpOffer);
        get_value_to(json, "sdp_answer", stream.sdpAnswer);
    }

    void to_json(Json & json, Feed const & feed)
    {
        set_value_from(json, "id", feed.id);
        set_value_from(json, "client_id", feed.clientId);
        set_value_from(json, "role", feed.role);
        set_value_from(json, "description", feed.description);
        set_value_from(json, "source_connection_quality", feed.sourceConnectionQuality);
        set_value_from(json, "volume", feed.volume);
        set_value_from(json, "capabilities", feed.capabilities);
        set_value_from(json, "content", feed.content);
        set_value_from(json, "stream", feed.stream);
    }

    void from_json(Json const & json, Feed & feed)
    {
        get_value_to(json, "id", feed.id);
        get_value_to(json, "client_id", feed.clientId);
        get_value_to(json, "role", feed.role);
        get_value_to(json, "description", feed.description);
        get_value_to(json, "source_connection_quality", feed.sourceConnectionQuality);
        get_value_to(json, "volume", feed.volume);
        get_value_to(json, "capabilities", feed.capabilities);
        get_value_to(json, "content", feed.content);
        get_value_to(json, "stream", feed.stream);
    }

    void to_json(Json & json, Stage const & stage)
    {
        set_value_from(json, "id", stage.id);
        set_value_from(json, "username", stage.username);
        set_value_from(json, "title", stage.title);
        set_value_from(json, "broadcast_id", stage.broadcastId);
        set_value_from(json, "upsert_broadcast", stage.upsertBroadcast);
        set_value_from(json, "live", stage.live);
        set_value_from(json, "feeds", stage.feeds);
    }

    void from_json(Json const & json, Stage & stage)
    {
        get_value_to(json, "id", stage.id);
        get_value_to(json, "username", stage.username);
        get_value_to(json, "title", stage.title);
        get_value_to(json, "broadcast_id", stage.broadcastId);
        get_value_to(json, "upsert_broadcast", stage.upsertBroadcast);
        get_value_to(json, "live", stage.live);
        get_value_to(json, "feeds", stage.feeds);
    }

    void to_json(Json & json, StageRequest const & request)
    {
        set_value_from(json, "client", request.client);
        set_value_from(json, "cursor", request.cursor);
        set_value_from(json, "payload", request.stage);
    }

    void from_json(Json const & json, StageResponse & response)
    {
        get_value_to(json, "cursor", response.cursor);
        get_value_to(json, "retry_in", response.retryIn);
        get_value_to(json, "payload", response.stage);
    }

    void from_json(Json const & json, DisplayMessage & message)
    {
        get_value_to(json, "title", message.title);
        get_value_to(json, "body", message.body);
    };

    void from_json(Json const & json, FailureResponse & response)
    {
        get_value_to(json, "type", response.type);
        get_value_to(json, "reason", response.reason);
        get_value_to(json, "display_message", response.displayMessage);
    };
}
