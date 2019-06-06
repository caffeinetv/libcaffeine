// Copyright 2019 Caffeine Inc. All rights reserved.

#pragma once

#include "RestApi.hpp"
#include "websocketpp/client.hpp"
#include "websocketpp/config/asio_client.hpp"

namespace caff {

    class WebsocketClient {

    public:
        WebsocketClient();
        ~WebsocketClient();

        struct Connection {
            std::string name;
            websocketpp::connection_hdl handle;
        };

        enum ConnectionEndType { Failed, Closed };

        optional<Connection> connect(
                std::string url,
                std::string name,
                std::function<void()> openedCallback,
                std::function<void(ConnectionEndType)> endedCallback,
                std::function<void(std::string const &)> messageReceivedCallback);

        void sendMessage(Connection const & connection, std::string const & message);

        void close(Connection && connection);

    private:
        using Client = websocketpp::client<websocketpp::config::asio_tls_client>;
        Client client;
        std::thread clientThread;
    };

} // namespace caff
