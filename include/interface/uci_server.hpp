#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <cerrno>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "uci.hpp"


class UCIServer
{
    using UCI_Factory = std::function<std::shared_ptr<UCI>()>;

    UCI_Factory uci_factory;

    uint16_t port;

    int server_socket = -1;

    std::atomic<bool> running{false};

    std::mutex clients_mutex;
    std::vector<std::thread> client_threads;

    bool CreateSocket();

    void ClientThread(int client_socket);

    void CloseServer();

    static bool Send(
        int client_socket,
        const std::string& data);

    static bool SendLine(
        int client_socket,
        const std::string& line);

    static std::string ReceiveLine(
        int client_socket);

public:
    UCIServer(
        UCI_Factory uci_factory,
        uint16_t port);

    ~UCIServer();

    UCIServer(const UCIServer&) = delete;
    UCIServer& operator=(const UCIServer&) = delete;

    // Start the server.
    //
    // Blocks while accepting clients.
    void Run();

    // Stop accepting clients and disconnect all clients.
    void Stop();
};
