// This file was automatically generated and should not be edited.
#pragma once

#include <memory>
#include <vector>
#include "nlohmann/json.hpp"
#include "absl/types/optional.h"
#include "absl/types/variant.h"

// optional serialization
namespace nlohmann {
    template <typename T>
    struct adl_serializer<absl::optional<T>> {
        static void to_json(json & json, absl::optional<T> const & opt) {
            if (opt.has_value()) {
                json = *opt;
            } else {
                json = nullptr;
            }
        }

        static void from_json(const json & json, absl::optional<T> & opt) {
            if (json.is_null()) {
                opt.reset();
            } else {
                opt = json.get<T>();
            }
        }
    };
}

namespace caffql {

    using Json = nlohmann::json;
    using Id = std::string;
    using absl::optional;
    using absl::variant;
    using absl::monostate;
    using absl::visit;

    struct GraphqlError {
        std::string message;
    };

    template <typename Data>
    using GraphqlResponse = variant<Data, std::vector<GraphqlError>>;

    inline void from_json(Json const & json, GraphqlError & value) {
        json.at("message").get_to(value.message);
    }

    struct BroadcasterStream {
        Id id;
        std::string url;
        std::string sdpAnswer;
    };

    inline void from_json(Json const & json, BroadcasterStream & value) {
        json.at("id").get_to(value.id);
        json.at("url").get_to(value.url);
        json.at("sdpAnswer").get_to(value.sdpAnswer);
    }

    enum class Capability {
        Audio,
        Video,
        Unknown = -1
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(Capability, {
        {Capability::Unknown, nullptr},
        {Capability::Audio, "AUDIO"},
        {Capability::Video, "VIDEO"},
    });

    struct CdnError {
        optional<std::string> title;
        std::string message;
    };

    inline void from_json(Json const & json, CdnError & value) {
        {
            auto it = json.find("title");
            if (it != json.end()) {
                it->get_to(value.title);
            } else {
                value.title.reset();
            }
        }
        json.at("message").get_to(value.message);
    }

    struct ClientContentionError {
        optional<std::string> title;
        std::string message;
    };

    inline void from_json(Json const & json, ClientContentionError & value) {
        {
            auto it = json.find("title");
            if (it != json.end()) {
                it->get_to(value.title);
            } else {
                value.title.reset();
            }
        }
        json.at("message").get_to(value.message);
    }

    enum class ClientType {
        Web,

        // iOS and Android
        Mobile,

        // COBS and Lucio (via libcaffeine)
        Capture,
        Unknown = -1
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(ClientType, {
        {ClientType::Unknown, nullptr},
        {ClientType::Web, "WEB"},
        {ClientType::Mobile, "MOBILE"},
        {ClientType::Capture, "CAPTURE"},
    });

    struct ControllingClient {
        Id clientId;
        ClientType clientType;
    };

    inline void from_json(Json const & json, ControllingClient & value) {
        json.at("clientId").get_to(value.clientId);
        json.at("clientType").get_to(value.clientType);
    }

    struct GeoRestrictionError {
        optional<std::string> title;
        std::string message;
    };

    inline void from_json(Json const & json, GeoRestrictionError & value) {
        {
            auto it = json.find("title");
            if (it != json.end()) {
                it->get_to(value.title);
            } else {
                value.title.reset();
            }
        }
        json.at("message").get_to(value.message);
    }

    struct LiveHostable {
        Id address;
    };

    inline void from_json(Json const & json, LiveHostable & value) {
        json.at("address").get_to(value.address);
    }

    struct LiveHosting {
        Id address;
        double volume;

        /*
        ownerId is the account ID of the owner of the stage that is providing the
        underlying live-hostable content.
        */
        std::string ownerId;

        /*
        ownerUsername is the username of the owner of the stage that is providing the
        underlying live-hostable content.
        */
        std::string ownerUsername;
    };

    inline void from_json(Json const & json, LiveHosting & value) {
        json.at("address").get_to(value.address);
        json.at("volume").get_to(value.volume);
        json.at("ownerId").get_to(value.ownerId);
        json.at("ownerUsername").get_to(value.ownerUsername);
    }

    struct OutOfCapacityError {
        optional<std::string> title;
        std::string message;
    };

    inline void from_json(Json const & json, OutOfCapacityError & value) {
        {
            auto it = json.find("title");
            if (it != json.end()) {
                it->get_to(value.title);
            } else {
                value.title.reset();
            }
        }
        json.at("message").get_to(value.message);
    }

    struct RequestViewerStreamPayload {
        optional<Id> feedId;
    };

    inline void from_json(Json const & json, RequestViewerStreamPayload & value) {
        {
            auto it = json.find("feedId");
            if (it != json.end()) {
                it->get_to(value.feedId);
            } else {
                value.feedId.reset();
            }
        }
    }

    enum class Restriction {
        UsOnly,
        Unknown = -1
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(Restriction, {
        {Restriction::Unknown, nullptr},
        {Restriction::UsOnly, "US_ONLY"},
    });

    enum class Role {
        Primary,
        Secondary,
        Unknown = -1
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(Role, {
        {Role::Unknown, nullptr},
        {Role::Primary, "PRIMARY"},
        {Role::Secondary, "SECONDARY"},
    });

    enum class SourceConnectionQuality {
        Good,
        Poor,
        Unknown = -1
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(SourceConnectionQuality, {
        {SourceConnectionQuality::Unknown, nullptr},
        {SourceConnectionQuality::Good, "GOOD"},
        {SourceConnectionQuality::Poor, "POOR"},
    });

    struct TakedownStagePayload {
        optional<bool> success;
    };

    inline void from_json(Json const & json, TakedownStagePayload & value) {
        {
            auto it = json.find("success");
            if (it != json.end()) {
                it->get_to(value.success);
            } else {
                value.success.reset();
            }
        }
    }

    struct ViewerStream {
        Id id;
        std::string url;
        std::string sdpOffer;
    };

    inline void from_json(Json const & json, ViewerStream & value) {
        json.at("id").get_to(value.id);
        json.at("url").get_to(value.url);
        json.at("sdpOffer").get_to(value.sdpOffer);
    }

    struct ViewerStreamInput {
        Id id;
        std::string url;
        std::string sdpOffer;
    };

    inline void to_json(Json & json, ViewerStreamInput const & value) {
        json["id"] = value.id;
        json["url"] = value.url;
        json["sdpOffer"] = value.sdpOffer;
    }

    struct UnknownError {
        optional<std::string> title;
        std::string message;
    };

    struct Error {
        variant<OutOfCapacityError, CdnError, ClientContentionError, GeoRestrictionError, UnknownError> implementation;

        optional<std::string> const & title() const {
            return visit([](auto const & implementation) -> optional<std::string> const & {
                return implementation.title;
            }, implementation);
        }

        std::string const & message() const {
            return visit([](auto const & implementation) -> std::string const & {
                return implementation.message;
            }, implementation);
        }

    };

    inline void from_json(Json const & json, UnknownError & value) {
        {
            auto it = json.find("title");
            if (it != json.end()) {
                it->get_to(value.title);
            } else {
                value.title.reset();
            }
        }
        json.at("message").get_to(value.message);
    }

    inline void from_json(Json const & json, Error & value) {
        std::string occupiedType = json.at("__typename");
        if (occupiedType == "OutOfCapacityError") {
            value = {OutOfCapacityError(json)};
        } else if (occupiedType == "CdnError") {
            value = {CdnError(json)};
        } else if (occupiedType == "ClientContentionError") {
            value = {ClientContentionError(json)};
        } else if (occupiedType == "GeoRestrictionError") {
            value = {GeoRestrictionError(json)};
        } else {
            value = {UnknownError(json)};
        }
    }

    struct FeedInput {

        // Must be a valid, lower-cased v4 UUID.
        Id id;
        optional<Id> gameId;

        // Defaults to PRIMARY.
        optional<Role> role;

        /*
        Currently, the volume only matters for live hosting content, so that clients
        can determine the right balance between the volume of live hosting content
        versus the client's content.
        
        Defaults to 1.0; currently only returned on a LiveHostingFeed.
        */
        optional<double> volume;

        // Defaults to GOOD.
        optional<SourceConnectionQuality> sourceConnectionQuality;

        // Defaults to [AUDIO, VIDEO].
        optional<std::vector<Capability>> capabilities;

        /*
        Currently, only Garrus may add live-hostable content, but this is included
        for testing. Users without the permission will not be able to make the feed
        hostable.
        */
        optional<bool> liveHostable;

        /*
        Use this for adding a live hosting feed to the stage.
        
        Must correspond to a valid "address" on a LiveHostableFeed.
        */
        optional<Id> liveHostingAddress;

        /*
        Include the SDP offer when adding a new feed whose content you own and will
        be sending into the CDN. The SDP offer is used to allocate a
        broadcaster stream for the feed.
        
        NOTE: You must either provide an sdpOffer or a viewerStream.
        */
        optional<std::string> sdpOffer;

        /*
        If you want to add a live-hosting feed, and you already have a viewer stream
        that you want to reuse, include it here.
        */
        optional<ViewerStreamInput> viewerStream;
    };

    inline void to_json(Json & json, FeedInput const & value) {
        json["id"] = value.id;
        json["gameId"] = value.gameId;
        json["role"] = value.role;
        json["volume"] = value.volume;
        json["sourceConnectionQuality"] = value.sourceConnectionQuality;
        json["capabilities"] = value.capabilities;
        json["liveHostable"] = value.liveHostable;
        json["liveHostingAddress"] = value.liveHostingAddress;
        json["sdpOffer"] = value.sdpOffer;
        json["viewerStream"] = value.viewerStream;
    }

    struct UnknownLiveHost {
        Id address;
    };

    struct LiveHost {
        variant<LiveHostable, LiveHosting, UnknownLiveHost> implementation;

        Id const & address() const {
            return visit([](auto const & implementation) -> Id const & {
                return implementation.address;
            }, implementation);
        }

    };

    inline void from_json(Json const & json, UnknownLiveHost & value) {
        json.at("address").get_to(value.address);
    }

    inline void from_json(Json const & json, LiveHost & value) {
        std::string occupiedType = json.at("__typename");
        if (occupiedType == "LiveHostable") {
            value = {LiveHostable(json)};
        } else if (occupiedType == "LiveHosting") {
            value = {LiveHosting(json)};
        } else {
            value = {UnknownLiveHost(json)};
        }
    }

    struct StageSubscriptionViewerStreamInput {
        Id feedId;
        ViewerStreamInput viewerStream;
    };

    inline void to_json(Json & json, StageSubscriptionViewerStreamInput const & value) {
        json["feedId"] = value.feedId;
        json["viewerStream"] = value.viewerStream;
    }

    struct UnknownStream {
        Id id;
        std::string url;
    };

    struct Stream {
        variant<BroadcasterStream, ViewerStream, UnknownStream> implementation;

        Id const & id() const {
            return visit([](auto const & implementation) -> Id const & {
                return implementation.id;
            }, implementation);
        }

        std::string const & url() const {
            return visit([](auto const & implementation) -> std::string const & {
                return implementation.url;
            }, implementation);
        }

    };

    inline void from_json(Json const & json, UnknownStream & value) {
        json.at("id").get_to(value.id);
        json.at("url").get_to(value.url);
    }

    inline void from_json(Json const & json, Stream & value) {
        std::string occupiedType = json.at("__typename");
        if (occupiedType == "BroadcasterStream") {
            value = {BroadcasterStream(json)};
        } else if (occupiedType == "ViewerStream") {
            value = {ViewerStream(json)};
        } else {
            value = {UnknownStream(json)};
        }
    }

    struct Feed {
        Id id;
        Id clientId;
        ClientType clientType;

        // Represents content identifiable in Roadhog's game table.
        optional<Id> gameId;
        SourceConnectionQuality sourceConnectionQuality;
        std::vector<Capability> capabilities;

        /*
        Reyes may override the role a client specifies in order to maintain a valid
        feed / role configuration.
        */
        Role role;

        /*
        Currently, only Garrus may set restrictions, which is why they are not
        included on the FeedInput type.
        
        Restrictions indicate when content is geofenced. (TODO Rename to
        "GeoRestriction"?) If a user tries to view geofenced content, it will
        allocate a special stream that shows a message that the content is blocked.
        
        Geofencing is performed based on the client's IP address.
        */
        std::vector<Restriction> restrictions;
        optional<LiveHost> liveHost;
        Stream stream;
    };

    inline void from_json(Json const & json, Feed & value) {
        json.at("id").get_to(value.id);
        json.at("clientId").get_to(value.clientId);
        json.at("clientType").get_to(value.clientType);
        {
            auto it = json.find("gameId");
            if (it != json.end()) {
                it->get_to(value.gameId);
            } else {
                value.gameId.reset();
            }
        }
        json.at("sourceConnectionQuality").get_to(value.sourceConnectionQuality);
        json.at("capabilities").get_to(value.capabilities);
        json.at("role").get_to(value.role);
        json.at("restrictions").get_to(value.restrictions);
        {
            auto it = json.find("liveHost");
            if (it != json.end()) {
                it->get_to(value.liveHost);
            } else {
                value.liveHost.reset();
            }
        }
        json.at("stream").get_to(value.stream);
    }

    struct Stage {

        // The Roadhog account ID of the owner of the stage (no CAID prefix).
        Id id;

        // The username of the owner of the stage.
        std::string username;
        std::string title;
        bool live;

        /*
        For now, clients still need to upload their own images.
        
        A broadcast ID will be created when the stage goes from having no feeds to
        having at least one feed.
        
        When a stage goes offline, the broadcastId will be cleared.
        
        This is optional in order to make it easier to deprecate and to ensure that
        it is only set when the stage has a valid broadcast ID.
        
        If you don't have a broadcast ID.
        */
        optional<Id> broadcastId;

        /*
        If the stage is live (or in test mode), then the ControllingClient will be
        populated. This indicates that there exists a client that is in "control" of
        the stage. While there is a controller, other clients will not be able to
        edit the stage unless strictly authorized to do so (according to the client
        takeover rules, documented here (TODO)).
        */
        optional<ControllingClient> controllingClient;

        /*
        Contains only the feeds that the requesting client should be concerned with.
        (i.e. The feeds necessary to allow the client to view the stage.)
        */
        std::vector<Feed> feeds;
    };

    inline void from_json(Json const & json, Stage & value) {
        json.at("id").get_to(value.id);
        json.at("username").get_to(value.username);
        json.at("title").get_to(value.title);
        json.at("live").get_to(value.live);
        {
            auto it = json.find("broadcastId");
            if (it != json.end()) {
                it->get_to(value.broadcastId);
            } else {
                value.broadcastId.reset();
            }
        }
        {
            auto it = json.find("controllingClient");
            if (it != json.end()) {
                it->get_to(value.controllingClient);
            } else {
                value.controllingClient.reset();
            }
        }
        json.at("feeds").get_to(value.feeds);
    }

    struct StageSubscriptionPayload {
        optional<Error> error;
        Stage stage;
    };

    inline void from_json(Json const & json, StageSubscriptionPayload & value) {
        {
            auto it = json.find("error");
            if (it != json.end()) {
                it->get_to(value.error);
            } else {
                value.error.reset();
            }
        }
        json.at("stage").get_to(value.stage);
    }

    struct StartBroadcastPayload {
        optional<Error> error;
        Stage stage;
    };

    inline void from_json(Json const & json, StartBroadcastPayload & value) {
        {
            auto it = json.find("error");
            if (it != json.end()) {
                it->get_to(value.error);
            } else {
                value.error.reset();
            }
        }
        json.at("stage").get_to(value.stage);
    }

    struct StopBroadcastPayload {
        optional<Error> error;
        Stage stage;
    };

    inline void from_json(Json const & json, StopBroadcastPayload & value) {
        {
            auto it = json.find("error");
            if (it != json.end()) {
                it->get_to(value.error);
            } else {
                value.error.reset();
            }
        }
        json.at("stage").get_to(value.stage);
    }

    namespace Subscription {


        /*
        Use this subscription to get reactive updates to the stage pushed from Reyes.
        Upon connecting, will return immediately with the client's current view of
        the stage. As a viewer, this is where stream allocation happens.
        
        If Reyes fails to retrieve streams for any feed, it will return the feeds
        (possibly 0) that it was able to retrieve streams for. Reyes will then retry
        a few times. If it is still not able to retrieve streams for every feed, then
        it will return a CdnError.
        
        NOTE Stream allocation is not retried in mutations (addFeed, updateFeed, etc.).
        
        Viewer streams allocated here will be made available to views of the stage
        returned in the mutations above; however, those streams will only be valid
        when returned from the mutation if the client is subscribed to stage updates.
        If the client is not subscribed, then those streams may have expired.
        */
        struct StageField {

            static Json request(Id const & clientId, ClientType clientType, std::string const & username, optional<bool> constrainedBaseline, optional<std::vector<StageSubscriptionViewerStreamInput>> const & viewerStreams, optional<bool> skipStreamAllocation) {
                Json query = R"(
                    subscription Stage(
                        $clientId: ID!
                        $clientType: ClientType!
                        $username: String!
                        $constrainedBaseline: Boolean
                        $viewerStreams: [StageSubscriptionViewerStreamInput!]
                        $skipStreamAllocation: Boolean
                    ) {
                        stage(
                            clientId: $clientId
                            clientType: $clientType
                            username: $username
                            constrainedBaseline: $constrainedBaseline
                            viewerStreams: $viewerStreams
                            skipStreamAllocation: $skipStreamAllocation
                        ) {
                            error {
                                __typename
                                ...on OutOfCapacityError {
                                    title
                                    message
                                }
                                ...on CdnError {
                                    title
                                    message
                                }
                                ...on ClientContentionError {
                                    title
                                    message
                                }
                                ...on GeoRestrictionError {
                                    title
                                    message
                                }
                            }
                            stage {
                                id
                                username
                                title
                                live
                                broadcastId
                                controllingClient {
                                    clientId
                                    clientType
                                }
                                feeds {
                                    id
                                    clientId
                                    clientType
                                    gameId
                                    sourceConnectionQuality
                                    capabilities
                                    role
                                    restrictions
                                    liveHost {
                                        __typename
                                        ...on LiveHostable {
                                            address
                                        }
                                        ...on LiveHosting {
                                            address
                                            volume
                                            ownerId
                                            ownerUsername
                                        }
                                    }
                                    stream {
                                        __typename
                                        ...on BroadcasterStream {
                                            id
                                            url
                                            sdpAnswer
                                        }
                                        ...on ViewerStream {
                                            id
                                            url
                                            sdpOffer
                                        }
                                    }
                                }
                            }
                        }
                    }
                )";
                Json variables;
                variables["clientId"] = clientId;
                variables["clientType"] = clientType;
                variables["username"] = username;
                variables["constrainedBaseline"] = constrainedBaseline;
                variables["viewerStreams"] = viewerStreams;
                variables["skipStreamAllocation"] = skipStreamAllocation;
                return {{"query", std::move(query)}, {"variables", std::move(variables)}};
            }

            using ResponseData = StageSubscriptionPayload;

            static GraphqlResponse<ResponseData> response(Json const & json) {
                auto errors = json.find("errors");
                if (errors != json.end()) {
                    std::vector<GraphqlError> errorsList = *errors;
                    return errorsList;
                } else {
                    auto const & data = json.at("data");
                    return ResponseData(data.at("stage"));
                }
            }

        };

    }; // namespace Subscription 

    struct UpdateFeedPayload {
        optional<Error> error;
        Stage stage;
        Feed feed;
    };

    inline void from_json(Json const & json, UpdateFeedPayload & value) {
        {
            auto it = json.find("error");
            if (it != json.end()) {
                it->get_to(value.error);
            } else {
                value.error.reset();
            }
        }
        json.at("stage").get_to(value.stage);
        json.at("feed").get_to(value.feed);
    }

    struct AddFeedPayload {
        optional<Error> error;
        Stage stage;
        Feed feed;
    };

    inline void from_json(Json const & json, AddFeedPayload & value) {
        {
            auto it = json.find("error");
            if (it != json.end()) {
                it->get_to(value.error);
            } else {
                value.error.reset();
            }
        }
        json.at("stage").get_to(value.stage);
        json.at("feed").get_to(value.feed);
    }

    struct ChangeStageTitlePayload {
        optional<Error> error;
        Stage stage;
    };

    inline void from_json(Json const & json, ChangeStageTitlePayload & value) {
        {
            auto it = json.find("error");
            if (it != json.end()) {
                it->get_to(value.error);
            } else {
                value.error.reset();
            }
        }
        json.at("stage").get_to(value.stage);
    }

    namespace Query {


        /*
        NOTE All this query does is to read stage and sanitize it depending on who
        the requester is.
        
        No streams are allocated, and no feeds will be removed in order to show the
        client-specific view.
        
        There may be some stream information if Reyes has stored it, but clients
        should never use this information for anything.
        */
        struct StageField {

            static Json request(std::string const & username) {
                Json query = R"(
                    query Stage(
                        $username: String!
                    ) {
                        stage(
                            username: $username
                        ) {
                            id
                            username
                            title
                            live
                            broadcastId
                            controllingClient {
                                clientId
                                clientType
                            }
                            feeds {
                                id
                                clientId
                                clientType
                                gameId
                                sourceConnectionQuality
                                capabilities
                                role
                                restrictions
                                liveHost {
                                    __typename
                                    ...on LiveHostable {
                                        address
                                    }
                                    ...on LiveHosting {
                                        address
                                        volume
                                        ownerId
                                        ownerUsername
                                    }
                                }
                                stream {
                                    __typename
                                    ...on BroadcasterStream {
                                        id
                                        url
                                        sdpAnswer
                                    }
                                    ...on ViewerStream {
                                        id
                                        url
                                        sdpOffer
                                    }
                                }
                            }
                        }
                    }
                )";
                Json variables;
                variables["username"] = username;
                return {{"query", std::move(query)}, {"variables", std::move(variables)}};
            }

            using ResponseData = Stage;

            static GraphqlResponse<ResponseData> response(Json const & json) {
                auto errors = json.find("errors");
                if (errors != json.end()) {
                    std::vector<GraphqlError> errorsList = *errors;
                    return errorsList;
                } else {
                    auto const & data = json.at("data");
                    return ResponseData(data.at("stage"));
                }
            }

        };

    }; // namespace Query 

    struct RemoveFeedPayload {
        optional<Error> error;
        Stage stage;
    };

    inline void from_json(Json const & json, RemoveFeedPayload & value) {
        {
            auto it = json.find("error");
            if (it != json.end()) {
                it->get_to(value.error);
            } else {
                value.error.reset();
            }
        }
        json.at("stage").get_to(value.stage);
    }

    struct SetLiveHostingFeedPayload {
        optional<Error> error;
        Stage stage;
        Feed feed;
    };

    inline void from_json(Json const & json, SetLiveHostingFeedPayload & value) {
        {
            auto it = json.find("error");
            if (it != json.end()) {
                it->get_to(value.error);
            } else {
                value.error.reset();
            }
        }
        json.at("stage").get_to(value.stage);
        json.at("feed").get_to(value.feed);
    }

    struct UnknownStagePayload {
        Stage stage;
    };

    struct StagePayload {
        variant<AddFeedPayload, SetLiveHostingFeedPayload, UpdateFeedPayload, RemoveFeedPayload, ChangeStageTitlePayload, StartBroadcastPayload, StopBroadcastPayload, StageSubscriptionPayload, UnknownStagePayload> implementation;

        Stage const & stage() const {
            return visit([](auto const & implementation) -> Stage const & {
                return implementation.stage;
            }, implementation);
        }

    };

    inline void from_json(Json const & json, UnknownStagePayload & value) {
        json.at("stage").get_to(value.stage);
    }

    inline void from_json(Json const & json, StagePayload & value) {
        std::string occupiedType = json.at("__typename");
        if (occupiedType == "AddFeedPayload") {
            value = {AddFeedPayload(json)};
        } else if (occupiedType == "SetLiveHostingFeedPayload") {
            value = {SetLiveHostingFeedPayload(json)};
        } else if (occupiedType == "UpdateFeedPayload") {
            value = {UpdateFeedPayload(json)};
        } else if (occupiedType == "RemoveFeedPayload") {
            value = {RemoveFeedPayload(json)};
        } else if (occupiedType == "ChangeStageTitlePayload") {
            value = {ChangeStageTitlePayload(json)};
        } else if (occupiedType == "StartBroadcastPayload") {
            value = {StartBroadcastPayload(json)};
        } else if (occupiedType == "StopBroadcastPayload") {
            value = {StopBroadcastPayload(json)};
        } else if (occupiedType == "StageSubscriptionPayload") {
            value = {StageSubscriptionPayload(json)};
        } else {
            value = {UnknownStagePayload(json)};
        }
    }

    struct UnknownErrorPayload {
        optional<Error> error;
    };

    struct ErrorPayload {
        variant<AddFeedPayload, SetLiveHostingFeedPayload, UpdateFeedPayload, RemoveFeedPayload, ChangeStageTitlePayload, StartBroadcastPayload, StopBroadcastPayload, StageSubscriptionPayload, UnknownErrorPayload> implementation;

        optional<Error> const & error() const {
            return visit([](auto const & implementation) -> optional<Error> const & {
                return implementation.error;
            }, implementation);
        }

    };

    inline void from_json(Json const & json, UnknownErrorPayload & value) {
        {
            auto it = json.find("error");
            if (it != json.end()) {
                it->get_to(value.error);
            } else {
                value.error.reset();
            }
        }
    }

    inline void from_json(Json const & json, ErrorPayload & value) {
        std::string occupiedType = json.at("__typename");
        if (occupiedType == "AddFeedPayload") {
            value = {AddFeedPayload(json)};
        } else if (occupiedType == "SetLiveHostingFeedPayload") {
            value = {SetLiveHostingFeedPayload(json)};
        } else if (occupiedType == "UpdateFeedPayload") {
            value = {UpdateFeedPayload(json)};
        } else if (occupiedType == "RemoveFeedPayload") {
            value = {RemoveFeedPayload(json)};
        } else if (occupiedType == "ChangeStageTitlePayload") {
            value = {ChangeStageTitlePayload(json)};
        } else if (occupiedType == "StartBroadcastPayload") {
            value = {StartBroadcastPayload(json)};
        } else if (occupiedType == "StopBroadcastPayload") {
            value = {StopBroadcastPayload(json)};
        } else if (occupiedType == "StageSubscriptionPayload") {
            value = {StageSubscriptionPayload(json)};
        } else {
            value = {UnknownErrorPayload(json)};
        }
    }

    namespace Mutation {


        /*
        Add a feed to the stage. Any non-required fields in the input will be filled
        with default values if not specified.
        
        Reyes will not allow you to add the feed if another client is controlling the
        stage.
        
        TODO Link to the client contention logic documentation in the repo.
        
        Reyes may limit the number of feeds and live hostable feeds each client is
        allowed to add.
        
        Adding a feed may mutate other feeds, since Reyes always maintains a legal
        assignment of roles to the various feeds.
        */
        struct AddFeedField {

            static Json request(Id const & clientId, ClientType clientType, FeedInput const & input) {
                Json query = R"(
                    mutation AddFeed(
                        $clientId: ID!
                        $clientType: ClientType!
                        $input: FeedInput!
                    ) {
                        addFeed(
                            clientId: $clientId
                            clientType: $clientType
                            input: $input
                        ) {
                            error {
                                __typename
                                ...on OutOfCapacityError {
                                    title
                                    message
                                }
                                ...on CdnError {
                                    title
                                    message
                                }
                                ...on ClientContentionError {
                                    title
                                    message
                                }
                                ...on GeoRestrictionError {
                                    title
                                    message
                                }
                            }
                            stage {
                                id
                                username
                                title
                                live
                                broadcastId
                                controllingClient {
                                    clientId
                                    clientType
                                }
                                feeds {
                                    id
                                    clientId
                                    clientType
                                    gameId
                                    sourceConnectionQuality
                                    capabilities
                                    role
                                    restrictions
                                    liveHost {
                                        __typename
                                        ...on LiveHostable {
                                            address
                                        }
                                        ...on LiveHosting {
                                            address
                                            volume
                                            ownerId
                                            ownerUsername
                                        }
                                    }
                                    stream {
                                        __typename
                                        ...on BroadcasterStream {
                                            id
                                            url
                                            sdpAnswer
                                        }
                                        ...on ViewerStream {
                                            id
                                            url
                                            sdpOffer
                                        }
                                    }
                                }
                            }
                            feed {
                                id
                                clientId
                                clientType
                                gameId
                                sourceConnectionQuality
                                capabilities
                                role
                                restrictions
                                liveHost {
                                    __typename
                                    ...on LiveHostable {
                                        address
                                    }
                                    ...on LiveHosting {
                                        address
                                        volume
                                        ownerId
                                        ownerUsername
                                    }
                                }
                                stream {
                                    __typename
                                    ...on BroadcasterStream {
                                        id
                                        url
                                        sdpAnswer
                                    }
                                    ...on ViewerStream {
                                        id
                                        url
                                        sdpOffer
                                    }
                                }
                            }
                        }
                    }
                )";
                Json variables;
                variables["clientId"] = clientId;
                variables["clientType"] = clientType;
                variables["input"] = input;
                return {{"query", std::move(query)}, {"variables", std::move(variables)}};
            }

            using ResponseData = AddFeedPayload;

            static GraphqlResponse<ResponseData> response(Json const & json) {
                auto errors = json.find("errors");
                if (errors != json.end()) {
                    std::vector<GraphqlError> errorsList = *errors;
                    return errorsList;
                } else {
                    auto const & data = json.at("data");
                    return ResponseData(data.at("addFeed"));
                }
            }

        };


        /*
        Sets the live hosting feed to the stage. If a live hosting feed was already on
        the stage, this mutation will remove the pre-existing feed.
        
        If you try to set a non-hostable feed, then you will receive an error.
        
        You may also add live hosting feeds using the regular addFeed mutation, but
        doing so will give an error if you attempt to add more than a legal number of
        live hosting feeds to the stage.
        
        NOTE Stream allocation will not be retried if it fails. Instead, adding the
        feed will fail with a CdnError. This is also true for the other mutations.
        */
        struct SetLiveHostingFeedField {

            static Json request(Id const & clientId, ClientType clientType, FeedInput const & input) {
                Json query = R"(
                    mutation SetLiveHostingFeed(
                        $clientId: ID!
                        $clientType: ClientType!
                        $input: FeedInput!
                    ) {
                        setLiveHostingFeed(
                            clientId: $clientId
                            clientType: $clientType
                            input: $input
                        ) {
                            error {
                                __typename
                                ...on OutOfCapacityError {
                                    title
                                    message
                                }
                                ...on CdnError {
                                    title
                                    message
                                }
                                ...on ClientContentionError {
                                    title
                                    message
                                }
                                ...on GeoRestrictionError {
                                    title
                                    message
                                }
                            }
                            stage {
                                id
                                username
                                title
                                live
                                broadcastId
                                controllingClient {
                                    clientId
                                    clientType
                                }
                                feeds {
                                    id
                                    clientId
                                    clientType
                                    gameId
                                    sourceConnectionQuality
                                    capabilities
                                    role
                                    restrictions
                                    liveHost {
                                        __typename
                                        ...on LiveHostable {
                                            address
                                        }
                                        ...on LiveHosting {
                                            address
                                            volume
                                            ownerId
                                            ownerUsername
                                        }
                                    }
                                    stream {
                                        __typename
                                        ...on BroadcasterStream {
                                            id
                                            url
                                            sdpAnswer
                                        }
                                        ...on ViewerStream {
                                            id
                                            url
                                            sdpOffer
                                        }
                                    }
                                }
                            }
                            feed {
                                id
                                clientId
                                clientType
                                gameId
                                sourceConnectionQuality
                                capabilities
                                role
                                restrictions
                                liveHost {
                                    __typename
                                    ...on LiveHostable {
                                        address
                                    }
                                    ...on LiveHosting {
                                        address
                                        volume
                                        ownerId
                                        ownerUsername
                                    }
                                }
                                stream {
                                    __typename
                                    ...on BroadcasterStream {
                                        id
                                        url
                                        sdpAnswer
                                    }
                                    ...on ViewerStream {
                                        id
                                        url
                                        sdpOffer
                                    }
                                }
                            }
                        }
                    }
                )";
                Json variables;
                variables["clientId"] = clientId;
                variables["clientType"] = clientType;
                variables["input"] = input;
                return {{"query", std::move(query)}, {"variables", std::move(variables)}};
            }

            using ResponseData = SetLiveHostingFeedPayload;

            static GraphqlResponse<ResponseData> response(Json const & json) {
                auto errors = json.find("errors");
                if (errors != json.end()) {
                    std::vector<GraphqlError> errorsList = *errors;
                    return errorsList;
                } else {
                    auto const & data = json.at("data");
                    return ResponseData(data.at("setLiveHostingFeed"));
                }
            }

        };


        /*
        Reyes will only modify fields that are explicitly set in the input.
        
        It will give an error of the specified feed does not exist.
        */
        struct UpdateFeedField {

            static Json request(Id const & clientId, ClientType clientType, FeedInput const & input) {
                Json query = R"(
                    mutation UpdateFeed(
                        $clientId: ID!
                        $clientType: ClientType!
                        $input: FeedInput!
                    ) {
                        updateFeed(
                            clientId: $clientId
                            clientType: $clientType
                            input: $input
                        ) {
                            error {
                                __typename
                                ...on OutOfCapacityError {
                                    title
                                    message
                                }
                                ...on CdnError {
                                    title
                                    message
                                }
                                ...on ClientContentionError {
                                    title
                                    message
                                }
                                ...on GeoRestrictionError {
                                    title
                                    message
                                }
                            }
                            stage {
                                id
                                username
                                title
                                live
                                broadcastId
                                controllingClient {
                                    clientId
                                    clientType
                                }
                                feeds {
                                    id
                                    clientId
                                    clientType
                                    gameId
                                    sourceConnectionQuality
                                    capabilities
                                    role
                                    restrictions
                                    liveHost {
                                        __typename
                                        ...on LiveHostable {
                                            address
                                        }
                                        ...on LiveHosting {
                                            address
                                            volume
                                            ownerId
                                            ownerUsername
                                        }
                                    }
                                    stream {
                                        __typename
                                        ...on BroadcasterStream {
                                            id
                                            url
                                            sdpAnswer
                                        }
                                        ...on ViewerStream {
                                            id
                                            url
                                            sdpOffer
                                        }
                                    }
                                }
                            }
                            feed {
                                id
                                clientId
                                clientType
                                gameId
                                sourceConnectionQuality
                                capabilities
                                role
                                restrictions
                                liveHost {
                                    __typename
                                    ...on LiveHostable {
                                        address
                                    }
                                    ...on LiveHosting {
                                        address
                                        volume
                                        ownerId
                                        ownerUsername
                                    }
                                }
                                stream {
                                    __typename
                                    ...on BroadcasterStream {
                                        id
                                        url
                                        sdpAnswer
                                    }
                                    ...on ViewerStream {
                                        id
                                        url
                                        sdpOffer
                                    }
                                }
                            }
                        }
                    }
                )";
                Json variables;
                variables["clientId"] = clientId;
                variables["clientType"] = clientType;
                variables["input"] = input;
                return {{"query", std::move(query)}, {"variables", std::move(variables)}};
            }

            using ResponseData = UpdateFeedPayload;

            static GraphqlResponse<ResponseData> response(Json const & json) {
                auto errors = json.find("errors");
                if (errors != json.end()) {
                    std::vector<GraphqlError> errorsList = *errors;
                    return errorsList;
                } else {
                    auto const & data = json.at("data");
                    return ResponseData(data.at("updateFeed"));
                }
            }

        };


        /*
        This mutation will give you an error if you try to remove the last feed on a
        live stage.
        
        It will also give an error of the specified feed does not exist.
        */
        struct RemoveFeedField {

            static Json request(Id const & clientId, ClientType clientType, Id const & feedId) {
                Json query = R"(
                    mutation RemoveFeed(
                        $clientId: ID!
                        $clientType: ClientType!
                        $feedId: ID!
                    ) {
                        removeFeed(
                            clientId: $clientId
                            clientType: $clientType
                            feedId: $feedId
                        ) {
                            error {
                                __typename
                                ...on OutOfCapacityError {
                                    title
                                    message
                                }
                                ...on CdnError {
                                    title
                                    message
                                }
                                ...on ClientContentionError {
                                    title
                                    message
                                }
                                ...on GeoRestrictionError {
                                    title
                                    message
                                }
                            }
                            stage {
                                id
                                username
                                title
                                live
                                broadcastId
                                controllingClient {
                                    clientId
                                    clientType
                                }
                                feeds {
                                    id
                                    clientId
                                    clientType
                                    gameId
                                    sourceConnectionQuality
                                    capabilities
                                    role
                                    restrictions
                                    liveHost {
                                        __typename
                                        ...on LiveHostable {
                                            address
                                        }
                                        ...on LiveHosting {
                                            address
                                            volume
                                            ownerId
                                            ownerUsername
                                        }
                                    }
                                    stream {
                                        __typename
                                        ...on BroadcasterStream {
                                            id
                                            url
                                            sdpAnswer
                                        }
                                        ...on ViewerStream {
                                            id
                                            url
                                            sdpOffer
                                        }
                                    }
                                }
                            }
                        }
                    }
                )";
                Json variables;
                variables["clientId"] = clientId;
                variables["clientType"] = clientType;
                variables["feedId"] = feedId;
                return {{"query", std::move(query)}, {"variables", std::move(variables)}};
            }

            using ResponseData = RemoveFeedPayload;

            static GraphqlResponse<ResponseData> response(Json const & json) {
                auto errors = json.find("errors");
                if (errors != json.end()) {
                    std::vector<GraphqlError> errorsList = *errors;
                    return errorsList;
                } else {
                    auto const & data = json.at("data");
                    return ResponseData(data.at("removeFeed"));
                }
            }

        };


        // If a client is controlling the stage, then only that client may change the title.
        struct ChangeStageTitleField {

            static Json request(Id const & clientId, ClientType clientType, std::string const & title) {
                Json query = R"(
                    mutation ChangeStageTitle(
                        $clientId: ID!
                        $clientType: ClientType!
                        $title: String!
                    ) {
                        changeStageTitle(
                            clientId: $clientId
                            clientType: $clientType
                            title: $title
                        ) {
                            error {
                                __typename
                                ...on OutOfCapacityError {
                                    title
                                    message
                                }
                                ...on CdnError {
                                    title
                                    message
                                }
                                ...on ClientContentionError {
                                    title
                                    message
                                }
                                ...on GeoRestrictionError {
                                    title
                                    message
                                }
                            }
                            stage {
                                id
                                username
                                title
                                live
                                broadcastId
                                controllingClient {
                                    clientId
                                    clientType
                                }
                                feeds {
                                    id
                                    clientId
                                    clientType
                                    gameId
                                    sourceConnectionQuality
                                    capabilities
                                    role
                                    restrictions
                                    liveHost {
                                        __typename
                                        ...on LiveHostable {
                                            address
                                        }
                                        ...on LiveHosting {
                                            address
                                            volume
                                            ownerId
                                            ownerUsername
                                        }
                                    }
                                    stream {
                                        __typename
                                        ...on BroadcasterStream {
                                            id
                                            url
                                            sdpAnswer
                                        }
                                        ...on ViewerStream {
                                            id
                                            url
                                            sdpOffer
                                        }
                                    }
                                }
                            }
                        }
                    }
                )";
                Json variables;
                variables["clientId"] = clientId;
                variables["clientType"] = clientType;
                variables["title"] = title;
                return {{"query", std::move(query)}, {"variables", std::move(variables)}};
            }

            using ResponseData = ChangeStageTitlePayload;

            static GraphqlResponse<ResponseData> response(Json const & json) {
                auto errors = json.find("errors");
                if (errors != json.end()) {
                    std::vector<GraphqlError> errorsList = *errors;
                    return errorsList;
                } else {
                    auto const & data = json.at("data");
                    return ResponseData(data.at("changeStageTitle"));
                }
            }

        };


        /*
        If multiple clients were in preview mode and had feeds on the stage, starting
        a broadcast will remove the feeds no longer in use for the live broadcast.
        */
        struct StartBroadcastField {

            static Json request(Id const & clientId, ClientType clientType, optional<std::string> const & title) {
                Json query = R"(
                    mutation StartBroadcast(
                        $clientId: ID!
                        $clientType: ClientType!
                        $title: String
                    ) {
                        startBroadcast(
                            clientId: $clientId
                            clientType: $clientType
                            title: $title
                        ) {
                            error {
                                __typename
                                ...on OutOfCapacityError {
                                    title
                                    message
                                }
                                ...on CdnError {
                                    title
                                    message
                                }
                                ...on ClientContentionError {
                                    title
                                    message
                                }
                                ...on GeoRestrictionError {
                                    title
                                    message
                                }
                            }
                            stage {
                                id
                                username
                                title
                                live
                                broadcastId
                                controllingClient {
                                    clientId
                                    clientType
                                }
                                feeds {
                                    id
                                    clientId
                                    clientType
                                    gameId
                                    sourceConnectionQuality
                                    capabilities
                                    role
                                    restrictions
                                    liveHost {
                                        __typename
                                        ...on LiveHostable {
                                            address
                                        }
                                        ...on LiveHosting {
                                            address
                                            volume
                                            ownerId
                                            ownerUsername
                                        }
                                    }
                                    stream {
                                        __typename
                                        ...on BroadcasterStream {
                                            id
                                            url
                                            sdpAnswer
                                        }
                                        ...on ViewerStream {
                                            id
                                            url
                                            sdpOffer
                                        }
                                    }
                                }
                            }
                        }
                    }
                )";
                Json variables;
                variables["clientId"] = clientId;
                variables["clientType"] = clientType;
                variables["title"] = title;
                return {{"query", std::move(query)}, {"variables", std::move(variables)}};
            }

            using ResponseData = StartBroadcastPayload;

            static GraphqlResponse<ResponseData> response(Json const & json) {
                auto errors = json.find("errors");
                if (errors != json.end()) {
                    std::vector<GraphqlError> errorsList = *errors;
                    return errorsList;
                } else {
                    auto const & data = json.at("data");
                    return ResponseData(data.at("startBroadcast"));
                }
            }

        };


        /*
        keepLiveHostingFeeds, if true, will leave any live hosting feeds on the stage.
        Otherwise, all other feeds will be removed.
        */
        struct StopBroadcastField {

            static Json request(Id const & clientId, optional<bool> keepLiveHostingFeeds) {
                Json query = R"(
                    mutation StopBroadcast(
                        $clientId: ID!
                        $keepLiveHostingFeeds: Boolean
                    ) {
                        stopBroadcast(
                            clientId: $clientId
                            keepLiveHostingFeeds: $keepLiveHostingFeeds
                        ) {
                            error {
                                __typename
                                ...on OutOfCapacityError {
                                    title
                                    message
                                }
                                ...on CdnError {
                                    title
                                    message
                                }
                                ...on ClientContentionError {
                                    title
                                    message
                                }
                                ...on GeoRestrictionError {
                                    title
                                    message
                                }
                            }
                            stage {
                                id
                                username
                                title
                                live
                                broadcastId
                                controllingClient {
                                    clientId
                                    clientType
                                }
                                feeds {
                                    id
                                    clientId
                                    clientType
                                    gameId
                                    sourceConnectionQuality
                                    capabilities
                                    role
                                    restrictions
                                    liveHost {
                                        __typename
                                        ...on LiveHostable {
                                            address
                                        }
                                        ...on LiveHosting {
                                            address
                                            volume
                                            ownerId
                                            ownerUsername
                                        }
                                    }
                                    stream {
                                        __typename
                                        ...on BroadcasterStream {
                                            id
                                            url
                                            sdpAnswer
                                        }
                                        ...on ViewerStream {
                                            id
                                            url
                                            sdpOffer
                                        }
                                    }
                                }
                            }
                        }
                    }
                )";
                Json variables;
                variables["clientId"] = clientId;
                variables["keepLiveHostingFeeds"] = keepLiveHostingFeeds;
                return {{"query", std::move(query)}, {"variables", std::move(variables)}};
            }

            using ResponseData = StopBroadcastPayload;

            static GraphqlResponse<ResponseData> response(Json const & json) {
                auto errors = json.find("errors");
                if (errors != json.end()) {
                    std::vector<GraphqlError> errorsList = *errors;
                    return errorsList;
                } else {
                    auto const & data = json.at("data");
                    return ResponseData(data.at("stopBroadcast"));
                }
            }

        };


        /*
        Use this when the CDN gives you a 400 type error on the stream to tell Reyes
        you need a new viewer stream.
        
        (500's should be retried)
        
        This request will return immediately and trigger an update in the stage
        subscription with the new stream.
        */
        struct RequestViewerStreamField {

            static Json request(Id const & clientId, Id const & feedId) {
                Json query = R"(
                    mutation RequestViewerStream(
                        $clientId: ID!
                        $feedId: ID!
                    ) {
                        requestViewerStream(
                            clientId: $clientId
                            feedId: $feedId
                        ) {
                            feedId
                        }
                    }
                )";
                Json variables;
                variables["clientId"] = clientId;
                variables["feedId"] = feedId;
                return {{"query", std::move(query)}, {"variables", std::move(variables)}};
            }

            using ResponseData = RequestViewerStreamPayload;

            static GraphqlResponse<ResponseData> response(Json const & json) {
                auto errors = json.find("errors");
                if (errors != json.end()) {
                    std::vector<GraphqlError> errorsList = *errors;
                    return errorsList;
                } else {
                    auto const & data = json.at("data");
                    return ResponseData(data.at("requestViewerStream"));
                }
            }

        };


        // This is an admin-only mutation.
        struct TakedownStageField {

            static Json request(std::string const & username) {
                Json query = R"(
                    mutation TakedownStage(
                        $username: String!
                    ) {
                        takedownStage(
                            username: $username
                        ) {
                            success
                        }
                    }
                )";
                Json variables;
                variables["username"] = username;
                return {{"query", std::move(query)}, {"variables", std::move(variables)}};
            }

            using ResponseData = TakedownStagePayload;

            static GraphqlResponse<ResponseData> response(Json const & json) {
                auto errors = json.find("errors");
                if (errors != json.end()) {
                    std::vector<GraphqlError> errorsList = *errors;
                    return errorsList;
                } else {
                    auto const & data = json.at("data");
                    return ResponseData(data.at("takedownStage"));
                }
            }

        };

    }; // namespace Mutation 

} // namespace caffql
