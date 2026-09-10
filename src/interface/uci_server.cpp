#include "interface/uci_server.hpp"


UCIServer::UCIServer(
    UCI_Factory uci_factory,
    uint16_t port)
    :
    uci_factory(std::move(uci_factory)),
    port(port)
{
}


UCIServer::~UCIServer()
{
    Stop();

    /*
     * Run() may have already returned, but make absolutely
     * sure every client thread has finished before destroying
     * the server.
     */
    std::lock_guard lock(clients_mutex);

    for (std::thread& thread : client_threads)
    {
        if (thread.joinable())
            thread.join();
    }

    client_threads.clear();
}


bool UCIServer::CreateSocket()
{
    server_socket = socket(
        AF_INET,
        SOCK_STREAM,
        0);

    if (server_socket < 0)
        return false;

    /*
     * Allow the port to be reused immediately after
     * restarting the engine.
     */
    int reuse = 1;

    if (setsockopt(
            server_socket,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)) < 0)
    {
        CloseServer();
        return false;
    }

    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(
            server_socket,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) < 0)
    {
        CloseServer();
        return false;
    }

    /*
     * The old value of 1 effectively serialized incoming
     * connections. Give the kernel a reasonable backlog.
     */
    if (listen(server_socket, SOMAXCONN) < 0)
    {
        CloseServer();
        return false;
    }

    return true;
}


void UCIServer::CloseServer()
{
    if (server_socket < 0)
        return;

    shutdown(
        server_socket,
        SHUT_RDWR);

    close(server_socket);

    server_socket = -1;
}


bool UCIServer::Send(
    int client_socket,
    const std::string& data)
{
    const char* current = data.data();
    size_t remaining = data.size();

    while (remaining > 0)
    {
        const ssize_t sent = send(
            client_socket,
            current,
            remaining,
            MSG_NOSIGNAL);

        if (sent <= 0)
            return false;

        current += sent;
        remaining -= static_cast<size_t>(sent);
    }

    return true;
}


bool UCIServer::SendLine(
    int client_socket,
    const std::string& line)
{
    std::string message = line;

    if (message.empty() ||
        message.back() != '\n')
    {
        message += '\n';
    }

    return Send(client_socket, message);
}


std::string UCIServer::ReceiveLine(
    int client_socket)
{
    std::string line;

    char c;

    while (true)
    {
        const ssize_t received = recv(
            client_socket,
            &c,
            1,
            0);

        if (received == 0)
        {
            // Client disconnected.
            return {};
        }

        if (received < 0)
        {
            if (errno == EINTR)
                continue;

            return {};
        }

        if (c == '\n')
            break;

        /*
         * UCI uses LF, but accepting CRLF costs nothing.
         */
        if (c != '\r')
            line += c;
    }

    return line;
}


void UCIServer::ClientThread(
    int client_socket)
{
    /*
     * IMPORTANT:
     *
     * Every client gets its own UCI object.
     *
     * This means every client also gets its own:
     *
     *     ChessBoard
     *     ChessBot
     *     search thread
     *     clocks
     *     output buffer
     *     search state
     */
    std::shared_ptr<UCI> uci = uci_factory();

    if (!uci)
    {
        close(client_socket);
        return;
    }

    auto FlushOutput = [&]() -> bool
    {
        const std::string output = uci->TakeOutput();

        if (output.empty())
            return true;

        return Send(
            client_socket,
            output);
    };

    /*
     * Serve this client until:
     *
     *     - it disconnects
     *     - the server is stopped
     *     - the UCI instance receives "quit"
     */
    while (running && !uci->QuitRequested())
    {
        fd_set read_set;

        FD_ZERO(&read_set);
        FD_SET(client_socket, &read_set);

        timeval timeout{};

        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        const int ready = select(
            client_socket + 1,
            &read_set,
            nullptr,
            nullptr,
            &timeout);

        if (ready < 0)
        {
            if (errno == EINTR)
                continue;

            break;
        }

        /*
         * Flush asynchronous engine output.
         *
         * This is important because search results can be
         * generated without the client sending another command.
         */
        if (!FlushOutput())
            break;

        if (ready == 0)
            continue;

        const std::string command =
            ReceiveLine(client_socket);

        if (command.empty())
            break;

        const std::string response =
            uci->Respond(command);

        if (!response.empty())
        {
            if (!Send(client_socket, response))
                break;
        }

        /*
         * Respond() may have generated output immediately.
         */
        if (!FlushOutput())
            break;
    }

    /*
     * Stop the client's search before destroying UCI.
     *
     * UCI's destructor should normally handle this too, but
     * this makes the ownership/lifetime relationship explicit.
     */
    if (uci->IsSearching())
        uci->Respond("stop");

    shutdown(
        client_socket,
        SHUT_RDWR);

    close(client_socket);
}


void UCIServer::Run()
{
    if (running.exchange(true))
        return;

    if (!CreateSocket())
    {
        running = false;
        return;
    }

    while (running)
    {
        sockaddr_in client_address{};

        socklen_t address_length =
            sizeof(client_address);

        const int client_socket = accept(
            server_socket,
            reinterpret_cast<sockaddr*>(&client_address),
            &address_length);

        if (client_socket < 0)
        {
            if (!running)
                break;

            if (errno == EINTR)
                continue;

            continue;
        }

        /*
         * The server socket can be closed concurrently by
         * Stop(). Check running after accept() returns.
         */
        if (!running)
        {
            shutdown(
                client_socket,
                SHUT_RDWR);

            close(client_socket);

            break;
        }

        /*
         * Each client gets its own thread.
         */
        std::thread client_thread(
            &UCIServer::ClientThread,
            this,
            client_socket);

        {
            std::lock_guard lock(clients_mutex);

            client_threads.emplace_back(
                std::move(client_thread));
        }

        /*
         * Reclaim completed client threads.
         *
         * We cannot simply destroy a joinable std::thread.
         * Check which threads have finished indirectly by
         * periodically joining them below.
         *
         * Since std::thread has no portable "is finished"
         * operation, don't attempt to join here.
         */
    }

    CloseServer();

    running = false;

    /*
     * Wait for every client.
     *
     * This happens after accepting has stopped, so no new
     * clients can be added.
     */
    std::lock_guard lock(clients_mutex);

    for (std::thread& thread : client_threads)
    {
        if (thread.joinable())
            thread.join();
    }

    client_threads.clear();
}


void UCIServer::Stop()
{
    if (!running.exchange(false))
        return;

    /*
     * Closing the listening socket causes accept() in Run()
     * to wake up.
     */
    CloseServer();
}
