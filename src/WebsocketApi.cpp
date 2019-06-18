// Copyright 2019 Caffeine Inc. All rights reserved.

#include "WebsocketApi.hpp"
#include "ErrorLogging.hpp"

namespace caff {

    WebsocketClient::WebsocketClient() {
        // TODO: Configure logging to work with libcaffeine?
        client.set_access_channels(websocketpp::log::alevel::all);
        client.clear_access_channels(websocketpp::log::alevel::frame_payload);
        client.set_error_channels(websocketpp::log::elevel::all);

        client.set_tls_init_handler(
                [](websocketpp::connection_hdl connection) -> std::shared_ptr<websocketpp::lib::asio::ssl::context> {
                    // TODO: Look more into what should be done here
                    auto context = std::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);
                    context->set_default_verify_paths();
                    context->set_verify_mode(asio::ssl::verify_peer);
                    return context;
                });

        client.init_asio();
        client.start_perpetual();

        clientThread = std::thread(&Client::run, &client);
    }

    WebsocketClient::~WebsocketClient() {
        client.stop_perpetual();
        clientThread.join();
    }

    static std::string websocketLogPrefix(std::string const & label) { return "[Websocket " + label + "]"; }

    optional<WebsocketClient::Connection> WebsocketClient::connect(
            std::string url,
            std::string label,
            std::function<void(Connection)> openedCallback,
            std::function<void(Connection, ConnectionEndType)> endedCallback,
            std::function<void(Connection, std::string const &)> messageReceivedCallback) {
        std::error_code error;
        auto clientConnection = client.get_connection(url, error);

        auto logPrefix = websocketLogPrefix(label);

        if (error) {
            LOG_ERROR("%s connection initialization error: %s", logPrefix.c_str(), error.message().c_str());
            return {};
        }

        clientConnection->set_open_handler([=](websocketpp::connection_hdl handle) {
            LOG_DEBUG("%s opened", logPrefix.c_str());
            if (openedCallback) {
                openedCallback({ label, handle });
            }
        });

        clientConnection->set_close_handler([=](websocketpp::connection_hdl handle) {
            LOG_DEBUG("%s closed", logPrefix.c_str());
            std::error_code error;
            if (auto connection = client.get_con_from_hdl(handle, error)) {
                if (auto error = connection->get_ec()) {
                    LOG_DEBUG("%s close reason: %s", logPrefix.c_str(), error.message().c_str());
                }
            }

            if (endedCallback) {
                endedCallback({ label, handle }, ConnectionEndType::Closed);
            }
        });

        clientConnection->set_fail_handler([=](websocketpp::connection_hdl handle) {
            LOG_ERROR("%s failed", logPrefix.c_str());
            std::error_code error;
            if (auto connection = client.get_con_from_hdl(handle, error)) {
                if (auto error = connection->get_ec()) {
                    LOG_ERROR("%s failure reason: %s", logPrefix.c_str(), error.message().c_str());
                }
            }

            if (endedCallback) {
                endedCallback({ label, handle }, ConnectionEndType::Failed);
            }
        });

        clientConnection->set_message_handler([=](websocketpp::connection_hdl handle, Client::message_ptr message) {
            LOG_DEBUG("%s message received: %s", logPrefix.c_str(), message->get_payload().c_str());
            if (messageReceivedCallback) {
                messageReceivedCallback({ label, handle }, message->get_payload());
            }
        });

        client.connect(clientConnection);

        return Connection{ label, clientConnection->get_handle() };
    }

    void WebsocketClient::sendMessage(Connection const & connection, std::string const & message) {
        std::error_code error;
        client.send(connection.handle, message, websocketpp::frame::opcode::text, error);
        if (error) {
            LOG_ERROR(
                    "%s send message error: %s", websocketLogPrefix(connection.label).c_str(), error.message().c_str());
        }
    }

    void WebsocketClient::close(Connection && connection) {
        std::error_code error;
        client.close(connection.handle, websocketpp::close::status::normal, "", error);
        if (error) {
            LOG_ERROR(
                    "%s connection close error: %s",
                    websocketLogPrefix(connection.label).c_str(),
                    error.message().c_str());
        }
    }

} // namespace caff
