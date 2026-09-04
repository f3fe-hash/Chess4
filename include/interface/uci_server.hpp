#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include <cerrno>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "uci.hpp"


class UCIServer
{
    std::shared_ptr<UCI> uci;

    uint16_t port;

    int server_socket = -1;
    int client_socket = -1;

    bool running = false;

    bool CreateSocket();
    bool AcceptClient();

    void CloseClient();
    void CloseServer();

    bool Send(const std::string& data);
    bool SendLine(const std::string& line);

    std::string ReceiveLine();

public:
    UCIServer(
        std::shared_ptr<UCI> uci,
        uint16_t port);

    ~UCIServer();

    UCIServer(const UCIServer&) = delete;
    UCIServer& operator=(const UCIServer&) = delete;

    // Start the server.
    //
    // Blocks while accepting and serving clients.
    void Run();

    // Stop the server.
    void Stop();
};
