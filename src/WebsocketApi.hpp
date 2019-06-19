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

    template <typename OperationField>
    class GraphqlSubscription : public std::enable_shared_from_this<GraphqlSubscription<OperationField>> {
    public:
        template <typename... Args>
        GraphqlSubscription(
                WebsocketClient & client,
                SharedCredentials & creds,
                std::string label,
                std::function<void(caffql::GraphqlResponse<typename OperationField::ResponseData>)> messageHandler,
                std::function<void(WebsocketClient::ConnectionEndType)> endedHandler,
                Args const &... args)
            : client(client)
            , creds(creds)
            , label(std::move(label))
            , messageHandler(std::move(messageHandler))
            , endedHandler(std::move(endedHandler)) {
            auto requestJson = OperationField::request(args...);
            connectionParams = Json::object({ { "type", "start" }, { "payload", requestJson } });
        }

        ~GraphqlSubscription() { disconnect(); }

        void connect() {
            disconnect();

            std::weak_ptr<GraphqlSubscription> weakThis = this->shared_from_this();

            auto opened = [weakThis](WebsocketClient::Connection connection) {
                if (auto strongThis = weakThis.lock()) {
                    Json connectionInit{
                        { "type", "connection_init" },
                        { "payload",
                          Json::object({ { "X-Credential", strongThis->creds.lock().credentials.credential } }) }
                    };
                    strongThis->client.sendMessage(connection, connectionInit.dump());
                    strongThis->client.sendMessage(connection, strongThis->connectionParams.dump());
                }
            };

            auto ended = [weakThis](
                                 WebsocketClient::Connection connection, WebsocketClient::ConnectionEndType endType) {
                if (auto strongThis = weakThis.lock()) {
                    if (strongThis->endedHandler) {
                        strongThis->endedHandler(endType);
                    }
                }
            };

            auto messageReceived = [weakThis](WebsocketClient::Connection connection, std::string const & message) {
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

                auto strongThis = weakThis.lock();
                if (!strongThis) {
                    return;
                }

                if (auto errors = get_if<std::vector<caffql::GraphqlError>>(&response)) {
                    // Refresh credentials if needed
                    bool shouldRefresh = false;
                    for (auto const & error : *errors) {
                        // TODO: Use a more robust way to tell that we need to refresh credentials
                        if (error.message.find("credential") != std::string::npos) {
                            shouldRefresh = true;
                            break;
                        }
                    }

                    if (shouldRefresh) {
                        if (refreshCredentials(strongThis->creds)) {
                            strongThis->connect();
                            return;
                        }
                    }
                }

                if (strongThis->messageHandler) {
                    strongThis->messageHandler(response);
                }
            };

            connection = client.connect(REALTIME_GRAPHQL_URL, label, opened, ended, messageReceived);
        }

    private:
        WebsocketClient & client;
        SharedCredentials & creds;
        std::string label;
        Json connectionParams;
        optional<WebsocketClient::Connection> connection;
        std::function<void(caffql::GraphqlResponse<typename OperationField::ResponseData>)> messageHandler;
        std::function<void(WebsocketClient::ConnectionEndType)> endedHandler;

        void disconnect() {
            if (connection) {
                client.close(std::move(*connection));
                connection.reset();
            }
        }
    };

} // namespace caff
