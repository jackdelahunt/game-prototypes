#ifndef NET_CPP
#define NET_CPP

#include "libs/libs.h"
#include "ack.cpp"
#include "math.cpp"
#include "platform.h"

#include <thread>
#include <chrono>
#include <mutex>
#include <queue>

#define NETWORK_DELAY_MS 50

struct Server; // I hate c++

typedef HSteamNetConnection ConnectionId;
typedef void (*NewConnectionCallback)(Server *, ConnectionId);

ISteamNetworkingSockets *interface;

struct NetworkQueue {
    std::mutex mutex;
    std::queue<Slice<u8>> messages;
};

// @server
struct Server {
    inline static Server *instance = NULL; // used to keep server instance around for network callbacks

    Arena arena;
    bool running;

    std::thread network_thread;
    HSteamListenSocket socket;
    HSteamNetPollGroup poll_group;
    StackArray<HSteamNetConnection, 10> connections;
    NetworkQueue in_queue;

    // call backs to be defined in the game
    // !!! this is called from the server network thread
    NewConnectionCallback on_new_connection;
};

// @client
struct Client {
    inline static Client *instance = NULL; // used to keep client instance around for network callbacks

    Arena arena;
    bool running;

    std::thread network_thread;
    HSteamNetConnection connection;
    NetworkQueue in_queue;
};

struct Net {
    Arena arena;
    std::thread thread;
    bool running;

    ISteamNetworkingSockets *interface;

    Client client;
    Server server;

    NetworkQueue server_in_queue;
    NetworkQueue client_in_queue;
};

// call init_networking()
Net *net_global_instance = NULL;

bool init_networking();
void net_graceful_shutdown();

void net_run();
void neo_server_on_connection_changed(Net *net, Server *server, SteamNetConnectionStatusChangedCallback_t *info);
void neo_client_on_connection_changed(Net *net, Client *client, SteamNetConnectionStatusChangedCallback_t *info);

void neo_server_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info);
void neo_client_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info);

void network_queue_push(NetworkQueue *network_queue, Slice<u8> message);
bool network_queue_pop(NetworkQueue *network_queue, Slice<u8> *out_message);

bool is_server();
bool is_client();

void server_run(Server *server);
void server_on_connection_changed(Server *server, SteamNetConnectionStatusChangedCallback_t *info);
void server_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info);
void server_graceful_shutdown(Server *server);
void server_send_to_client(Server *server, Slice<u8> message, ConnectionId id);
void server_send_to_all_clients(Server *server, Slice<u8> message, ConnectionId exclude = 0);

void client_run(Client *client, const char *server_ip);
void client_on_connection_changed(Client *client, SteamNetConnectionStatusChangedCallback_t *info);
void client_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info);
void client_graceful_shutdown(Client *client);
void client_send_to_server(Client *client, Slice<u8> message);

void networking_debug_callback(ESteamNetworkingSocketsDebugOutputType type, const char *message);

void network_queue_push(NetworkQueue *network_queue, Slice<u8> message) {
    std::scoped_lock lock(network_queue->mutex);

    Slice<u8> message_copy = slice_create_malloc<u8>(message.len);
    slice_copy(message_copy, message);

    network_queue->messages.push(message_copy);
}

bool init_networking() {
    net_global_instance = new Net {};
    net_global_instance->arena = arena_create(10 * 1024 * 1024);

    SteamDatagramErrMsg error_message;
    if (!GameNetworkingSockets_Init(nullptr, error_message)) {
        logln_fmt(&net_global_instance->arena, "GameNetworkingSockets_Init failed: {}", error_message);
        return false;
    }

    net_global_instance->interface = SteamNetworkingSockets();
    
    SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, networking_debug_callback);

    logln("initialised networking and global net instance");
    return true;
}

void net_graceful_shutdown() {
    if (net_global_instance == NULL) {
        return;
    }

    net_global_instance->running = false;

    if (net_global_instance->thread.joinable()) {
        net_global_instance->thread.join();
    }

    GameNetworkingSockets_Kill();
}

void net_run() {
    Net *net = net_global_instance;
    ASSERT(net != NULL && net->running == false);

net->thread = std::thread([net] () {
    log_set_thread_options(LogOptions {
        .thread_name = "NETWORK",
        .thread_colour = CYAN_ASCII_CODE,
    });

    logln_fmt(&net->arena, "Started networking thread [thread={}]", get_current_thread_id());

    net->running = true;

    { // start server
        net->server.running = true;
    
        // init server 
        u16 port = 27020;
    
        SteamNetworkingIPAddr address; 
        address.Clear();
    
        address.ParseString("::1");
        address.m_port = port;
    
        SteamNetworkingConfigValue_t opt;
        opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)neo_server_network_connection_status_changed_callback);
    
        net->server.socket = net->interface->CreateListenSocketIP(address, 1, &opt);
        if (net->server.socket == k_HSteamListenSocket_Invalid) {
            logln_fmt(&net->arena, "Error creating server socket, failed to listen on port {}", port);
            net->server.running = false;
            return;
        }
        
        net->server.poll_group = net->interface->CreatePollGroup();
        if (net->server.poll_group == k_HSteamNetPollGroup_Invalid) {
            logln_fmt(&net->arena, "Error creating poll group, failed to listen on port {}", port);
            net->server.running = false;
            return;
        }
        
        logln_fmt(&net->arena, "Server listening on port {}", port);
    }

    { // start client
        net->client.running = true;
    
        // init client 
        u16 port = 27020;
    
        SteamNetworkingIPAddr address; 
        address.Clear();
    
        ASSERT(address.ParseString("::1"));
        address.m_port = port;
    
        char address_buffer[ SteamNetworkingIPAddr::k_cchMaxString ];
        address.ToString(address_buffer, sizeof(address_buffer), true);
    
        SteamNetworkingConfigValue_t opt;
        opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)neo_client_network_connection_status_changed_callback);
    
        net->client.connection = net->interface->ConnectByIPAddress(address, 1, &opt);
        if (net->client.connection == k_HSteamNetConnection_Invalid ) {
            logln("Tried to create an invalid connection to server");
            net->client.running = false;
            return;
        }
    }

    { // poll incoming messages
        while (net->running) {
            // poll incoming messages 
            while (net->server.running) {
                ISteamNetworkingMessage *incoming_message = NULL;
                int message_count = net->interface->ReceiveMessagesOnPollGroup(net->server.poll_group, &incoming_message, 1);
    
                if (message_count == 0) {
                    break;
                }
    
                ASSERT(message_count == 1 && incoming_message != NULL);
    
                { // copy message contents to byte slice and add to network queue
                    Slice<u8> bytes = slice_create_malloc<u8>(incoming_message->m_cbSize);
                    slice_copy_raw_ptr(bytes, incoming_message->m_pData);
                    network_queue_push(&net->server_in_queue, bytes);
                }
    
                incoming_message->Release();
            }

            while (net->client.running) {
                ISteamNetworkingMessage *incoming_message = NULL;
                i64 message_count = net->interface->ReceiveMessagesOnConnection(net->client.connection, &incoming_message, 1);
    
                if (message_count == 0) {
                    break;
                }
    
                ASSERT(message_count == 1 && incoming_message != NULL);
    
                { // copy message contents to byte slice and add to network queue
                    Slice<u8> bytes = slice_create_malloc<u8>(incoming_message->m_cbSize);
                    slice_copy_raw_ptr(bytes, incoming_message->m_pData);
                    network_queue_push(&net->client_in_queue, bytes);
                }
    
                incoming_message->Release();
            }
    
            net->interface->RunCallbacks();
    
	    std::this_thread::sleep_for(std::chrono::milliseconds(NETWORK_DELAY_MS));
        }
    }

    { // shutdown client
        logln("Shutting down client gracefully");
    
        net->interface->CloseConnection(net->client.connection, 0, nullptr, false);
        net->client.connection = k_HSteamNetConnection_Invalid;
    }

    { // shutdown server
        logln("Shutting down server gracefully");
    
        for (HSteamNetConnection connection : net->server.connections) {
            net->interface->CloseConnection(connection, 0, "Server shutdown", true);
        }
    
        reset(&net->server.connections);
    
        net->interface->CloseListenSocket(net->server.socket);
        net->server.socket = k_HSteamListenSocket_Invalid;
    
        net->interface->DestroyPollGroup(net->server.poll_group);
        net->server.poll_group = k_HSteamNetPollGroup_Invalid;
    }
});
}

void neo_server_on_connection_changed(Net *net, Server *server, SteamNetConnectionStatusChangedCallback_t *info) {
    switch (info->m_info.m_eState) {
        case k_ESteamNetworkingConnectionState_None:                    break;
        case k_ESteamNetworkingConnectionState_ClosedByPeer:            break;
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:  break;
        case k_ESteamNetworkingConnectionState_Connected:               break;
        case k_ESteamNetworkingConnectionState_Connecting: {
            logln_fmt(&net->arena, "Server received connection request from {}", info->m_info.m_szConnectionDescription);
    
            if (net->interface->AcceptConnection(info->m_hConn) != k_EResultOK) {
                // This could fail.  If the remote host tried to connect, but then
                // disconnected, the connection may already be half closed.  Just
                // destroy whatever we have on our side.
                net->interface->CloseConnection(info->m_hConn, 0, nullptr, false);
                logln("Server could not accept connection");
                break;
            }
    
            if (!net->interface->SetConnectionPollGroup(info->m_hConn, server->poll_group)) {
                net->interface->CloseConnection(info->m_hConn, 0, nullptr, false );
                logln("Server failed to set poll group for connection");
                break;
            }
    
            append(&server->connections, info->m_hConn);
            // server->on_new_connection(server, info->m_hConn);
        } break;
        default: break;
    }
}

void neo_client_on_connection_changed(Net *net, Client *client, SteamNetConnectionStatusChangedCallback_t *info) {
    ASSERT(info->m_hConn == client->connection || client->connection == k_HSteamNetConnection_Invalid);

    switch (info->m_info.m_eState) {
        case k_ESteamNetworkingConnectionState_None: {
            logln_fmt(&net->arena, "Client connection is in a none state: {}", info->m_info.m_szEndDebug);
        } break;
        case k_ESteamNetworkingConnectionState_ClosedByPeer: {
            logln_fmt(&net->arena, "Client connection closed by peer: {}", info->m_info.m_szEndDebug);
        } break;
        case k_ESteamNetworkingConnectionState_Connecting: {
            logln("Client is trying to connect");
        } break;
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
            client->running = false;
    
            if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting ) {
                logln_fmt(&net->arena, "Client tried to connect but failed: {}", info->m_info.m_szEndDebug);
            }
            else if (info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
                logln_fmt(&net->arena, "Client lost contact with the host: {}", info->m_info.m_szEndDebug);
            }
            else {
                logln_fmt(&net->arena, "Client disconnected from server: {}", info->m_info.m_szEndDebug);
            }
    
            // Clean up the connection.  This is important!
            // The connection is "closed" in the network sense, but
            // it has not been destroyed.  We must close it on our end, too
            // to finish up.  The reason information do not matter in this case,
            // and we cannot linger because it's already closed on the other end,
            // so we just pass 0's.
            net->interface->CloseConnection(info->m_hConn, 0, nullptr, false );
            client->connection = k_HSteamNetConnection_Invalid;
        } break;
        case k_ESteamNetworkingConnectionState_Connected: {
            logln("Client connected to server");
        } break;
        default: break;
    }
}

void neo_server_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info) {
    ASSERT(net_global_instance != NULL);

    neo_server_on_connection_changed(net_global_instance, &net_global_instance->server, info);
}

void neo_client_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info) {
    ASSERT(net_global_instance != NULL);

    neo_client_on_connection_changed(net_global_instance, &net_global_instance->client, info);
}

bool network_queue_pop(NetworkQueue *network_queue, Slice<u8> *out_message) {
    std::scoped_lock lock(network_queue->mutex);

    if (network_queue->messages.empty()) {
        return false;
    }

    *out_message = network_queue->messages.front();
    network_queue->messages.pop();

    return true;
}

bool is_server() {
#ifdef SERVER
    return true;
#else
    return false;
#endif
}

bool is_client() {
#ifdef CLIENT
    return true;
#else
    return false;
#endif
}

void server_run(Server *server) {
    if (server->running) return;

    server->arena = arena_create(20 * 1024 * 1024); // 20MB

    server->network_thread = std::thread([server] () {
    log_set_thread_options(LogOptions {
        .thread_name = "SERVER NET",
        .thread_colour = CYAN_ASCII_CODE,
    });

    logln_fmt(&server->arena, "Started server network [thread={}]", get_current_thread_id());

    // set this so any callbacks can then refer to the current running server
    // this should not be used directly unless for those callbacks and needs to
    // be cleaned up when the thread ends or aborts
    // - 31/07/25
    Server::instance = server;
    server->running = true;

    // init server 
    u16 port = 27020;

    SteamNetworkingIPAddr address; 
    address.Clear();

    address.ParseString("::1");
    address.m_port = port;

    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)server_network_connection_status_changed_callback);

    server->socket = interface->CreateListenSocketIP(address, 1, &opt);
    if (server->socket == k_HSteamListenSocket_Invalid) {
        logln_fmt(&server->arena, "Error creating socket, failed to listen on port {}", port);
        server->running = false;
        return;
    }
    
    server->poll_group = interface->CreatePollGroup();
    if (server->poll_group == k_HSteamNetPollGroup_Invalid) {
        logln_fmt(&server->arena, "Error creating poll group, failed to listen on port {}", port);
        server->running = false;
        return;
    }
    
    logln_fmt(&server->arena, "Server listening on port {}", port);

    while (server->running) {
        // poll incoming messages 
        while (server->running) {
            ISteamNetworkingMessage *incoming_message = NULL;
            int message_count = interface->ReceiveMessagesOnPollGroup(server->poll_group, &incoming_message, 1);

            if (message_count == 0) {
                break;
            }

            ASSERT(message_count == 1 && incoming_message != NULL);

            { // copy message contents to byte slice and add to network queue
                Slice<u8> bytes = slice_create_malloc<u8>(incoming_message->m_cbSize);
                slice_copy_raw_ptr(bytes, incoming_message->m_pData);
                network_queue_push(&server->in_queue, bytes);
            }

            incoming_message->Release();
        }

        interface->RunCallbacks();

	std::this_thread::sleep_for(std::chrono::milliseconds(NETWORK_DELAY_MS));
    }

    logln("Shutting down server gracefully");

    for (HSteamNetConnection connection : server->connections) {
        interface->CloseConnection(connection, 0, "Server shutdown", true);
    }

    reset(&server->connections);

    interface->CloseListenSocket(server->socket);
    server->socket = k_HSteamListenSocket_Invalid;

    interface->DestroyPollGroup(server->poll_group);
    server->poll_group = k_HSteamNetPollGroup_Invalid;
    });
}

void server_on_connection_changed(Server *server, SteamNetConnectionStatusChangedCallback_t *info) {
    switch (info->m_info.m_eState) {
        case k_ESteamNetworkingConnectionState_None:                    break;
        case k_ESteamNetworkingConnectionState_ClosedByPeer:            break;
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:  break;
        case k_ESteamNetworkingConnectionState_Connected:               break;
        case k_ESteamNetworkingConnectionState_Connecting: {
            logln_fmt(&server->arena, "Connection request from {}", info->m_info.m_szConnectionDescription);
    
            if (interface->AcceptConnection(info->m_hConn) != k_EResultOK) {
                // This could fail.  If the remote host tried to connect, but then
                // disconnected, the connection may already be half closed.  Just
                // destroy whatever we have on our side.
                interface->CloseConnection(info->m_hConn, 0, nullptr, false);
                logln("Can't accept connection");
                break;
            }
    
            if (!interface->SetConnectionPollGroup(info->m_hConn, server->poll_group)) {
                interface->CloseConnection(info->m_hConn, 0, nullptr, false );
                logln("Failed to set poll group for connection");
                break;
            }
    
            append(&server->connections, info->m_hConn);
            server->on_new_connection(server, info->m_hConn);
        } break;
        default: break;
    }
}

void server_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info) {
    ASSERT(Server::instance != NULL && Server::instance->running);

    server_on_connection_changed(Server::instance, info);
}

void server_graceful_shutdown(Server *server) {
    server->running = false;

    if (server->network_thread.joinable()) {
        server->network_thread.join();
    }
}

void server_send_to_client(Server *server, Slice<u8> message, ConnectionId id) {
    interface->SendMessageToConnection(id, message.ptr, message.len, k_nSteamNetworkingSend_Reliable, NULL);
}

void server_send_to_all_clients(Server *server, Slice<u8> message, ConnectionId exclude) {
    for (ConnectionId id : server->connections) {
        if (exclude != 0 && id == exclude) continue;

        interface->SendMessageToConnection(id, message.ptr, message.len, k_nSteamNetworkingSend_Reliable, NULL);
    }
}

void client_run(Client *client, const char *server_ip) {
    if (client->running) return;

    client->arena = arena_create(20 * 1024 * 1024); // 20MB

    client->network_thread = std::thread([client, server_ip] () {
    log_set_thread_options(LogOptions {
        .thread_name = "CLIENT NET",
        .thread_colour = MAGENTA_ASCII_CODE,
    });

    logln_fmt(&client->arena, "Started server network [thread={}]", get_current_thread_id());
    // set this so any callbacks can then refer to the current running client
    // this should not be used directly unless for those callbacks and needs to
    // be cleaned up when the thread ends or aborts
    // - 31/07/25
    Client::instance = client;
    client->running = true;

    // init client 
    u16 port = 27020;

    SteamNetworkingIPAddr address; 
    address.Clear();

    if (server_ip != NULL) {
        address.ParseString(server_ip);
        address.m_port = port;
    }
    else {
        ASSERT(address.ParseString("::1"));
        address.m_port = port;
    }

    char address_buffer[ SteamNetworkingIPAddr::k_cchMaxString ];
    address.ToString(address_buffer, sizeof(address_buffer), true);

    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)client_network_connection_status_changed_callback);

    client->connection = interface->ConnectByIPAddress(address, 1, &opt);
    if (client->connection == k_HSteamNetConnection_Invalid ) {
        logln("Failed to create connection to server");
        client->running = false;
        return;
    }

    while (client->running) {
        // poll incoming messages 
        while (client->running) {
            ISteamNetworkingMessage *incoming_message = NULL;
            i64 message_count = interface->ReceiveMessagesOnConnection(client->connection, &incoming_message, 1);

            if (message_count == 0) {
                break;
            }

            ASSERT(message_count == 1 && incoming_message != NULL);

            { // copy message contents to byte slice and add to network queue
                Slice<u8> bytes = slice_create_malloc<u8>(incoming_message->m_cbSize);
                slice_copy_raw_ptr(bytes, incoming_message->m_pData);
                network_queue_push(&client->in_queue, bytes);
            }

            incoming_message->Release();
        }

        interface->RunCallbacks();

	std::this_thread::sleep_for(std::chrono::milliseconds(NETWORK_DELAY_MS));
    }

    logln("Shutting down client gracefully");

    interface->CloseConnection(client->connection, 0, nullptr, false);
    client->connection = k_HSteamNetConnection_Invalid;
    });
}

void client_on_connection_changed(Client *client, SteamNetConnectionStatusChangedCallback_t *info) {
    ASSERT(info->m_hConn == client->connection || client->connection == k_HSteamNetConnection_Invalid);

    switch (info->m_info.m_eState) {
        case k_ESteamNetworkingConnectionState_None: {
            logln_fmt(&client->arena, "Connection is in a none state: {}", info->m_info.m_szEndDebug);
        } break;
        case k_ESteamNetworkingConnectionState_ClosedByPeer: {
            logln_fmt(&client->arena, "Connection closed by peer: {}", info->m_info.m_szEndDebug);
        } break;
        case k_ESteamNetworkingConnectionState_Connecting: {
            logln("Client is trying to connect");
        } break;
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
            client->running = false;
    
            if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting ) {
                logln_fmt(&client->arena, "Tried to connect but failed: {}", info->m_info.m_szEndDebug);
            }
            else if (info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
                logln_fmt(&client->arena, "Lost contact with the host: {}", info->m_info.m_szEndDebug);
            }
            else {
                logln_fmt(&client->arena, "Disconnected from server: {}", info->m_info.m_szEndDebug);
            }
    
            // Clean up the connection.  This is important!
            // The connection is "closed" in the network sense, but
            // it has not been destroyed.  We must close it on our end, too
            // to finish up.  The reason information do not matter in this case,
            // and we cannot linger because it's already closed on the other end,
            // so we just pass 0's.
            interface->CloseConnection(info->m_hConn, 0, nullptr, false );
            client->connection = k_HSteamNetConnection_Invalid;
        } break;
        case k_ESteamNetworkingConnectionState_Connected: {
            logln("Client connected to server");
        } break;
        default: break;
    }
}

void client_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info) {
    ASSERT(Client::instance != NULL && Client::instance->running);

    client_on_connection_changed(Client::instance, info);
}

void client_graceful_shutdown(Client *client) {
    client->running = false;

    if (client->network_thread.joinable()) {
        client->network_thread.join();
    }
}

void client_send_to_server(Client *client, Slice<u8> message) {
    interface->SendMessageToConnection(client->connection, message.ptr, message.len, k_nSteamNetworkingSend_Reliable, NULL);
}

void networking_debug_callback(ESteamNetworkingSocketsDebugOutputType type, const char *message) {
    log_mutex.lock();
    printf("[NETWORK DEBUG]: %s\n", message);

    if (type == k_ESteamNetworkingSocketsDebugOutputType_Bug) {
        printf("[NETWORK]: fatal error\n");
        BREAKPOINT;
    }
    log_mutex.unlock();
}

#endif
