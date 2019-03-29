#include "serialization.hpp"

#include <sstream>

void from_json(nlohmann::json const & json, caff_user_info & userInfo)
{
    caff::getValue(json, "caid", userInfo.caid);
    caff::getValue(json, "username", userInfo.username);
    caff::getValue(json, "stage_id", userInfo.stage_id);
    caff::getValue(json, "can_broadcast", userInfo.can_broadcast);
}

void from_json(nlohmann::json const & json, caff_game_info & gameInfo)
{
    auto idNum = json.at("id").get<size_t>();
    std::ostringstream idStream;
    idStream << idNum;

    gameInfo.id = caff::cstrdup(idStream.str());
    caff::getValue(json, "name", gameInfo.name);

    auto const & processNames = json.at("process_names");
    gameInfo.num_process_names = processNames.size();
    if (gameInfo.num_process_names == 0) {
        gameInfo.process_names = nullptr;
        return;
    }

    gameInfo.process_names = new char *[gameInfo.num_process_names] {0};
    for (size_t i = 0; i < gameInfo.num_process_names; ++i) {
        try {
            caff::getValue(processNames, i, gameInfo.process_names[i]);
        }
        catch (...) {
            //LOG_WARN("Unable to read process name; ignoring");
        }
    }
}

void from_json(nlohmann::json const & json, caff_games & games)
{
    games.num_games = json.size();
    if (games.num_games == 0) {
        games.game_infos = nullptr;
        return;
    }

    games.game_infos = new caff_game_info *[games.num_games] {0};
    for (size_t i = 0; i < games.num_games; ++i) {
        try {
            games.game_infos[i] = new caff_game_info(json.at(i));
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
        getValue(json, "access_token", credentials.accessToken);
        getValue(json, "refresh_token", credentials.refreshToken);
        getValue(json, "caid", credentials.caid);
        getValue(json, "credential", credentials.credential);
    }

    void to_json(Json & json, Client const & client)
    {
        setValue(json, "id", client.id);
        setValue(json, "headless", client.headless);
        setValue(json, "constrained_baseline", client.constrainedBaseline);
    }

    void to_json(Json & json, FeedCapabilities const & capabilities)
    {
        setValue(json, "video", capabilities.video);
        setValue(json, "audio", capabilities.audio);
    }

    void from_json(Json const & json, FeedCapabilities & capabilities)
    {
        getValue(json, "audio", capabilities.audio);
        getValue(json, "video", capabilities.video);
    }

    void to_json(Json & json, FeedContent const & content)
    {
        setValue(json, "id", content.id);
        setValue(json, "type", content.type);
    }

    void from_json(Json const & json, FeedContent & content)
    {
        getValue(json, "id", content.id);
        getValue(json, "type", content.type);
    }

    void to_json(Json & json, FeedStream const & stream)
    {
        setValue(json, "id", stream.id);
        setValue(json, "source_id", stream.sourceId);
        setValue(json, "url", stream.url);
        setValue(json, "sdp_offer", stream.sdpOffer);
        setValue(json, "sdp_answer", stream.sdpAnswer);
    }

    void from_json(Json const & json, FeedStream & stream)
    {
        getValue(json, "id", stream.id);
        getValue(json, "source_id", stream.sourceId);
        getValue(json, "url", stream.url);
        getValue(json, "sdp_offer", stream.sdpOffer);
        getValue(json, "sdp_answer", stream.sdpAnswer);
    }

    void to_json(Json & json, Feed const & feed)
    {
        setValue(json, "id", feed.id);
        setValue(json, "client_id", feed.clientId);
        setValue(json, "role", feed.role);
        setValue(json, "description", feed.description);
        setValue(json, "source_connection_quality", feed.sourceConnectionQuality);
        setValue(json, "volume", feed.volume);
        setValue(json, "capabilities", feed.capabilities);
        setValue(json, "content", feed.content);
        setValue(json, "stream", feed.stream);
    }

    void from_json(Json const & json, Feed & feed)
    {
        getValue(json, "id", feed.id);
        getValue(json, "client_id", feed.clientId);
        getValue(json, "role", feed.role);
        getValue(json, "description", feed.description);
        getValue(json, "source_connection_quality", feed.sourceConnectionQuality);
        getValue(json, "volume", feed.volume);
        getValue(json, "capabilities", feed.capabilities);
        getValue(json, "content", feed.content);
        getValue(json, "stream", feed.stream);
    }

    void to_json(Json & json, Stage const & stage)
    {
        setValue(json, "id", stage.id);
        setValue(json, "username", stage.username);
        setValue(json, "title", stage.title);
        setValue(json, "broadcast_id", stage.broadcastId);
        setValue(json, "upsert_broadcast", stage.upsertBroadcast);
        setValue(json, "live", stage.live);
        setValue(json, "feeds", stage.feeds);
    }

    void from_json(Json const & json, Stage & stage)
    {
        getValue(json, "id", stage.id);
        getValue(json, "username", stage.username);
        getValue(json, "title", stage.title);
        getValue(json, "broadcast_id", stage.broadcastId);
        getValue(json, "upsert_broadcast", stage.upsertBroadcast);
        getValue(json, "live", stage.live);
        getValue(json, "feeds", stage.feeds);
    }

    void to_json(Json & json, StageRequest const & request)
    {
        setValue(json, "client", request.client);
        setValue(json, "cursor", request.cursor);
        setValue(json, "payload", request.stage);
    }

    void from_json(Json const & json, StageResponse & response)
    {
        getValue(json, "cursor", response.cursor);
        getValue(json, "retry_in", response.retryIn);
        getValue(json, "payload", response.stage);
    }

    void from_json(Json const & json, DisplayMessage & message)
    {
        getValue(json, "title", message.title);
        getValue(json, "body", message.body);
    };

    void from_json(Json const & json, FailureResponse & response)
    {
        getValue(json, "type", response.type);
        getValue(json, "reason", response.reason);
        getValue(json, "display_message", response.displayMessage);
    };
}
