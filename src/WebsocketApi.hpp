// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "RestApi.hpp"
#include "Urls.hpp"
#include "websocketpp/client.hpp"
#include "websocketpp/config/asio_client.hpp"

namespace caff {

    class WebsocketClient {

    public:
        WebsocketClient();
        ~WebsocketClient();

        struct Connection {
            std::string label;
            websocketpp::connection_hdl handle;
        };

        enum ConnectionEndType { Failed, Closed };

        optional<Connection> connect(
                std::string url,
                std::string label,
                std::function<void(Connection)> openedCallback,
                std::function<void(Connection, ConnectionEndType)> endedCallback,
                std::function<void(Connection, std::string const &)> messageReceivedCallback);

        void sendMessage(Connection const & connection, std::string const & message);

        void close(Connection && connection);

    private:
        using Client = websocketpp::client<websocketpp::config::asio_tls_client>;
        Client client;
        std::thread clientThread;
    };

    template <typename OperationField, typename... Args>
    optional<WebsocketClient::Connection> graphqlSubscription(
            WebsocketClient & client,
            std::string const & label,
            std::function<void(caffql::GraphqlResponse<typename OperationField::ResponseData>)> messageHandler,
            std::function<void(WebsocketClient::ConnectionEndType)> endedHandler,
            SharedCredentials & creds,
            Args const &... args) {
        auto credential = creds.lock().credentials.credential;
        Json connectionInit{ { "type", "connection_init" },
                             { "payload", Json::object({ { "X-Credential", std::move(credential) } }) } };
        auto requestJson = OperationField::request(args...);
        Json connectionParams = Json::object({ { "type", "start" }, { "payload", requestJson } });

        auto opened = [init = std::move(connectionInit), params = std::move(connectionParams), &client](
                              WebsocketClient::Connection connection) {
            client.sendMessage(connection, init.dump());
            client.sendMessage(connection, params.dump());
        };

        auto ended = [=](WebsocketClient::Connection connection, WebsocketClient::ConnectionEndType endType) {
            if (endedHandler) {
                endedHandler(endType);
            }
        };

        auto messageReceived = [=](WebsocketClient::Connection connection, std::string const & message) {
            Json json;
            try {
                json = Json::parse(message);
            } catch (...) {
                LOG_ERROR("Failed to parse graphql subscription message");
                return;
            }

            auto typeIt = json.find("type");
            if (typeIt == json.end() || *typeIt != "data") {
                return;
            }

            caffql::GraphqlResponse<typename OperationField::ResponseData> response;

            try {
                response = OperationField::response(json.at("payload"));
            } catch (std::exception const & error) {
                LOG_ERROR("Failed to unpack graphql subscription message: %s", error.what());
                return;
            }

            if (messageHandler) {
                messageHandler(response);
            }
        };

        return client.connect(REALTIME_GRAPHQL_URL, label, opened, ended, messageReceived);
    }

} // namespace caff
