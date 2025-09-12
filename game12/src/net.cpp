#ifndef NET_CPP
#define NET_CPP

#include "libs/libs.h"
#include "ack.cpp"
#include "math.cpp"
#include "platform.h"

#include <chrono>
#include <thread>
#include <mutex>
#include <queue>

#define NETWORK_MS_PER_TICK 8
#define DEFAULT_PORT 27020
#define DEFAULT_IP "192.168.0.171"

// I hate c++
struct Server; 
struct NetworkLayer; 

typedef HSteamNetConnection ConnectionId;
typedef void (*NewConnectionCallback)(NetworkLayer *, Server *, ConnectionId);

struct NetworkQueue {
    std::mutex mutex;
    std::queue<slice<u8>> messages;
};

struct NetworkConfig {
    u16 port;
    const char *server_address;

    bool run_server;
    bool run_client;
};

enum NetworkInstanceState {
    NOT_RUNNING,
    START,
    STOP,
    RUNNING,
};

// @server
struct Server {
    NetworkInstanceState server_state;
    HSteamListenSocket socket;
    HSteamNetPollGroup poll_group;
    StackArray<HSteamNetConnection, 10> connections;

    // NOTE: callback defined in game, called from network thread 
    NewConnectionCallback on_new_connection;
};

// @client
struct Client {
    NetworkInstanceState client_state;
    const char *server_address;
    HSteamNetConnection connection;
};

struct NetworkLayer {
    Arena arena;
    std::thread thread;
    bool running;

    ISteamNetworkingSockets *interface;

    Sampler mspt_sampler;
    Sampler client_in_messages_sampler;
    Sampler server_in_messages_sampler;

    AtomicSnapshot<Sampler> mspt_sampler_snapshot;
    AtomicSnapshot<Sampler> client_in_messages_sampler_snapshot;
    AtomicSnapshot<Sampler> server_in_messages_sampler_snaphot;

    Client client;
    Server server;

    NetworkQueue server_in_queue;
    NetworkQueue client_in_queue;
};

// call network_layer_init() and NET()
NetworkLayer *g_network_layer = NULL;

NetworkLayer *NET();

bool network_layer_init();

void network_layer_start();
void network_layer_stop();

void network_layer_start_client(NetworkLayer *net, const char *server_address);
void network_layer_stop_client(NetworkLayer *net);

void network_layer_start_server(NetworkLayer *net);
void network_layer_stop_server(NetworkLayer *net);

void network_layer_update_client(NetworkLayer *net);
void network_layer_update_server(NetworkLayer *net);

void server_on_connection_changed(NetworkLayer *net, Server *server, SteamNetConnectionStatusChangedCallback_t *info);
void client_on_connection_changed(NetworkLayer *net, Client *client, SteamNetConnectionStatusChangedCallback_t *info);

void server_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info);
void client_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info);

void network_queue_push(NetworkQueue *network_queue, slice<u8> message);
bool network_queue_pop(NetworkQueue *network_queue, slice<u8> *out_message);
i64 network_queue_size(NetworkQueue *network_queue);

void server_send_to_client(NetworkLayer *net, slice<u8> message, ConnectionId id);
void server_send_to_all_clients(NetworkLayer *net, slice<u8> message, ConnectionId exclude = 0);
void client_send_to_server(NetworkLayer *net, slice<u8> message);

void networking_debug_callback(ESteamNetworkingSocketsDebugOutputType type, const char *message);

NetworkLayer *NET() {
    Assert(g_network_layer != NULL);
    return g_network_layer;
}

bool network_layer_init() {
    g_network_layer = new NetworkLayer {};
    g_network_layer->arena = arena_create(MB(10));
    atomic_snapshot_init(&g_network_layer->mspt_sampler_snapshot);
    atomic_snapshot_init(&g_network_layer->client_in_messages_sampler_snapshot);
    atomic_snapshot_init(&g_network_layer->server_in_messages_sampler_snaphot);

    SteamDatagramErrMsg error_message;
    if (!GameNetworkingSockets_Init(nullptr, error_message)) {
        Logf("GameNetworkingSockets_Init failed: {}", error_message);
        return false;
    }

    g_network_layer->interface = SteamNetworkingSockets();
    
    SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, networking_debug_callback);

    Log("Initialised network layer");
    return true;
}

void network_layer_start() {
    NetworkLayer *net = NET();
    Assert(net->running == false);

net->thread = std::thread([net] () {
    log_set_thread_name("network");

    Timer tick_timer = timer_create_ms(NETWORK_MS_PER_TICK);

    Infof("Started networking thread @ {}tps [thread={}]", i64(1000.0f / f32(NETWORK_MS_PER_TICK)), get_current_thread_id());

    net->running = true;

    while (net->running) {
        f32 delta_time = 0;

        if (!timer_is_complete(&tick_timer, &delta_time)) {
            continue;
        }

        sampler_append(&net->mspt_sampler, delta_time * 1000.0f);

        network_layer_update_server(net);
        network_layer_update_client(net); 
        net->interface->RunCallbacks();

        sampler_append(&net->client_in_messages_sampler, f32(network_queue_size(&net->client_in_queue)));
        sampler_append(&net->server_in_messages_sampler, f32(network_queue_size(&net->server_in_queue)));

        atomic_snapshot_copy_and_swap(&net->mspt_sampler_snapshot, &net->mspt_sampler);
        atomic_snapshot_copy_and_swap(&net->client_in_messages_sampler_snapshot, &net->client_in_messages_sampler);
        atomic_snapshot_copy_and_swap(&net->server_in_messages_sampler_snaphot, &net->server_in_messages_sampler);

        std::this_thread::sleep_for(std::chrono::microseconds(NETWORK_MS_PER_TICK - 1));
    }
});
}

void network_layer_stop() {
    if (g_network_layer == NULL) {
        return;
    }

    g_network_layer->running = false;

    if (g_network_layer->thread.joinable()) {
        g_network_layer->thread.join();
    }

    GameNetworkingSockets_Kill();
}

void network_layer_start_client(NetworkLayer *net, const char *server_address) {
    Assert(net && net->running);
    Assert(net->client.client_state == NOT_RUNNING);

    net->client.client_state = START;
    net->client.server_address = server_address;
}

void network_layer_stop_client(NetworkLayer *net) {
    Assert(net && net->running);

    net->client.client_state = STOP;
}

void network_layer_start_server(NetworkLayer *net) {
    Assert(net && net->running);
    Assert(net->server.server_state == NOT_RUNNING);

    net->server.server_state = START;
}

void network_layer_stop_server(NetworkLayer *net) {
    Assert(net && net->running);

    net->server.server_state = STOP;
}

void network_layer_update_client(NetworkLayer *net) {
    if (net->client.client_state == START) {

        // set when changing state to start from main thread 
        Assert(net->client.server_address);

        // set to not running, if we return early then this is how
        // the game client knows, if this all works then set to running
        // - 10/08/25
        net->client.client_state = NOT_RUNNING; 

        SteamNetworkingIPAddr address;
        HSteamNetConnection connection;

        { // parse ip address
            address.Clear();
        
            bool ok = address.ParseString(net->client.server_address);
            if (!ok) {
                Logf("Could not parse server address supplied: {}", net->client.server_address);
                return;
            }
    
            address.m_port = DEFAULT_PORT;
        }

        { // attempt to connect
            SteamNetworkingConfigValue_t connect_options;
            connect_options.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)client_network_connection_status_changed_callback);
            
            connection = net->interface->ConnectByIPAddress(address, 1, &connect_options);
            if (connection == k_HSteamNetConnection_Invalid ) {
                Log("Problem when creating network client, invalid connection to server");
                return;
            }
        }
       
        net->client = Client {.client_state = RUNNING, .connection = connection};
    }

    if (net->client.client_state == STOP) {
        Log("Shutting down network client");

        net->interface->CloseConnection(net->client.connection, 0, nullptr, false);
        net->client.connection = k_HSteamNetConnection_Invalid;

        net->client.client_state = NOT_RUNNING;
    }

    if (net->client.client_state == RUNNING) {
        while (net->client.client_state == RUNNING) {
            ISteamNetworkingMessage *incoming_message = NULL;
            i64 message_count = net->interface->ReceiveMessagesOnConnection(net->client.connection, &incoming_message, 1);
 
            if (message_count == 0) {
                break;
            }
 
            Assert(message_count == 1 && incoming_message != NULL);
 
            { // copy message contents to byte slice and add to network queue
                slice<u8> bytes = slice_create_malloc<u8>(incoming_message->m_cbSize);
                slice_copy_raw_ptr(bytes, incoming_message->m_pData);
                network_queue_push(&net->client_in_queue, bytes);
            }
 
            incoming_message->Release();
        }
    }
}

void network_layer_update_server(NetworkLayer *net) {
    if (net->server.server_state == START) {

        // set to not running, if we return early then this is how
        // the game client knows, if this all works then set to running
        // - 10/08/25
        net->server.server_state = NOT_RUNNING; 

        SteamNetworkingIPAddr address;

        { // parse ip address
            address.Clear();
            Assert(address.ParseString(DEFAULT_IP));
            address.m_port = DEFAULT_PORT;
        }

        { // attempt to create socket and poll group
            SteamNetworkingConfigValue_t connect_options;
            connect_options.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)server_network_connection_status_changed_callback);
            
            net->server.socket = net->interface->CreateListenSocketIP(address, 1, &connect_options);
            if (net->server.socket == k_HSteamListenSocket_Invalid) {
                Logf("Error creating server socket, failed to listen on port {}", DEFAULT_PORT);
                return;
            }

            net->server.poll_group = net->interface->CreatePollGroup();
            if (net->server.poll_group == k_HSteamNetPollGroup_Invalid) {
                Log("Error creating poll group for network server");
                return;
            }

            Logf("Started server and listening on port {}", DEFAULT_PORT);
        }
    
        net->server.server_state = RUNNING;
    }

    if (net->server.server_state == STOP) {
        Log("Shutting down server gracefully");
    
        for (HSteamNetConnection connection : net->server.connections) {
            net->interface->CloseConnection(connection, 0, "Server shutdown", true);
        }
    
        reset(&net->server.connections);
    
        net->interface->CloseListenSocket(net->server.socket);
        net->server.socket = k_HSteamListenSocket_Invalid;
    
        net->interface->DestroyPollGroup(net->server.poll_group);
        net->server.poll_group = k_HSteamNetPollGroup_Invalid;

        net->server.server_state = NOT_RUNNING;
    }

    if (net->server.server_state == RUNNING) {
        while (net->client.client_state == RUNNING) {
            ISteamNetworkingMessage *incoming_message = NULL;
            i32 message_count = net->interface->ReceiveMessagesOnPollGroup(net->server.poll_group, &incoming_message, 1);
    
            if (message_count == 0) {
                break;
            }

            Assert(message_count == 1 && incoming_message != NULL);
    
            { // copy message contents to byte slice and add to network queue
                slice<u8> bytes = slice_create_malloc<u8>(incoming_message->m_cbSize);
                slice_copy_raw_ptr(bytes, incoming_message->m_pData);
                network_queue_push(&net->server_in_queue, bytes);
            }
    
            incoming_message->Release();
        }
    }
}

void server_on_connection_changed(NetworkLayer *net, Server *server, SteamNetConnectionStatusChangedCallback_t *info) {
    switch (info->m_info.m_eState) {
        case k_ESteamNetworkingConnectionState_None:                    break;
        case k_ESteamNetworkingConnectionState_ClosedByPeer:            break;
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:  break;
        case k_ESteamNetworkingConnectionState_Connected:               break;
        case k_ESteamNetworkingConnectionState_Connecting: {
            Infof("Server received connection request from {}", info->m_info.m_szConnectionDescription);
    
            if (net->interface->AcceptConnection(info->m_hConn) != k_EResultOK) {
                // This could fail.  If the remote host tried to connect, but then
                // disconnected, the connection may already be half closed.  Just
                // destroy whatever we have on our side.
                net->interface->CloseConnection(info->m_hConn, 0, nullptr, false);
                Err("Server could not accept connection");
                break;
            }
    
            if (!net->interface->SetConnectionPollGroup(info->m_hConn, server->poll_group)) {
                net->interface->CloseConnection(info->m_hConn, 0, nullptr, false );
                Err("Server failed to set poll group for connection");
                break;
            }
    
            append(&server->connections, info->m_hConn);
            server->on_new_connection(net, server, info->m_hConn);
        } break;
        default: break;
    }
}

void client_on_connection_changed(NetworkLayer *net, Client *client, SteamNetConnectionStatusChangedCallback_t *info) {
    Assert(info->m_hConn == client->connection || client->connection == k_HSteamNetConnection_Invalid);

    switch (info->m_info.m_eState) {
        case k_ESteamNetworkingConnectionState_None: {
            Warnf("Client connection is in a none state: {}", info->m_info.m_szEndDebug);
        } break;
        case k_ESteamNetworkingConnectionState_ClosedByPeer: {
            Errf("Client connection closed by peer: {}", info->m_info.m_szEndDebug);
        } break;
        case k_ESteamNetworkingConnectionState_Connecting: {
            Infof("Client is trying to connect");
        } break;
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
            // TODO: maybe its fine to set it to stop and print what happened
            // shouldn't the normal code that runs after a stop do whats needed?
            client->client_state = NOT_RUNNING;
    
            if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting ) {
                Errf("Client tried to connect but failed: {}", info->m_info.m_szEndDebug);
            }
            else if (info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
                Errf("Client lost contact with the host: {}", info->m_info.m_szEndDebug);
            }
            else {
                Infof("Client disconnected from server: {}", info->m_info.m_szEndDebug);
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
            Info("Client connected to server");
        } break;
        default: break;
    }
}

void server_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info) {
    Assert(g_network_layer != NULL);

    server_on_connection_changed(g_network_layer, &g_network_layer->server, info);
}

void client_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info) {
    Assert(g_network_layer != NULL);

    client_on_connection_changed(g_network_layer, &g_network_layer->client, info);
}

void network_queue_push(NetworkQueue *network_queue, slice<u8> message) {
    std::scoped_lock lock(network_queue->mutex);

    slice<u8> message_copy = slice_create_malloc<u8>(message.len);
    slice_copy(message_copy, message);

    network_queue->messages.push(message_copy);
}

bool network_queue_pop(NetworkQueue *network_queue, slice<u8> *out_message) {
    std::scoped_lock lock(network_queue->mutex);

    if (network_queue->messages.empty()) {
        return false;
    }

    *out_message = network_queue->messages.front();
    network_queue->messages.pop();

    return true;
}

i64 network_queue_size(NetworkQueue *network_queue) {
    return network_queue->messages.size();
}

void server_send_to_client(NetworkLayer *net, slice<u8> message, ConnectionId id) {
    net->interface->SendMessageToConnection(id, message.ptr, message.len, k_nSteamNetworkingSend_Reliable, NULL);
}

void server_send_to_all_clients(NetworkLayer *net, slice<u8> message, ConnectionId exclude) {
    for (ConnectionId id : net->server.connections) {
        if (exclude != 0 && id == exclude) {
            continue;
        }

        net->interface->SendMessageToConnection(id, message.ptr, message.len, k_nSteamNetworkingSend_Reliable, NULL);
    }
}

void client_send_to_server(NetworkLayer *net, slice<u8> message) {
    net->interface->SendMessageToConnection(net->client.connection, message.ptr, message.len, k_nSteamNetworkingSend_Reliable, NULL);
}

void networking_debug_callback(ESteamNetworkingSocketsDebugOutputType type, const char *message) {
    Logf("[NETWORK DEBUG]: {}", message);
    Assert(type != k_ESteamNetworkingSocketsDebugOutputType_Bug);
}

#endif
