#include "libs/libs.h"
#include "ack.cpp"
#include "math.cpp"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <string>
#include <thread>

// Total: 6:30
// Started: 13:00

#define MAX_ENTITIES 2000

struct Entity {
    // meta
    u64 flags;

    // base
    v3 position;
    v3 size;
    v3 rotation;
    v3 velocity;

    // rendering
    v4 colour;
};

enum EntityFlags {
    EF_PLAYER           = 1 << 0,
    EF_DELETE           = 1 << 16,
};

struct Server {
    inline static Server *instance = NULL; // used to keep server instance around for network callbacks

    std::thread network_thread;
    bool running;
    ISteamNetworkingSockets *interface;
    SteamNetworkingMicroseconds start_time;
    HSteamListenSocket socket;
    HSteamNetPollGroup poll_group;

    StackArray<HSteamNetConnection, 10> connections;
};

struct Client {
    inline static Client *instance = NULL; // used to keep client instance around for network callbacks

    std::thread network_thread;
    bool running;
    ISteamNetworkingSockets *interface;
    SteamNetworkingMicroseconds start_time;
    HSteamNetConnection connection;
};

struct State {
    f64 time;

    Server server;
    Client client;

    StackArray<Entity, MAX_ENTITIES> entities;

} state = {};

void update_entities(f32 delta_time);
void draw_entities(f32 delta_time);
void physics(f32 delta_time);

Entity *spawn_entity(Entity entity);

void server_run(Server *server);
void server_thread_entry(Server *server);
void server_on_connection_changed(Server *server, SteamNetConnectionStatusChangedCallback_t *info);
void server_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info);
void server_graceful_shutdown(Server *server);

void client_run(Client *client);
void client_thread_entry(Client *client);
void client_on_connection_changed(Client *client, SteamNetConnectionStatusChangedCallback_t *info);
void client_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info);
void client_graceful_shutdown(Client *client);

void networking_debug_callback(ESteamNetworkingSocketsDebugOutputType type, const char *message);

int main() {
    state = State {};
    const char *window_title = NULL;

#ifdef SERVER
    server_run(&state.server);
    window_title = "Game11 [Server]";
#endif

#ifdef CLIENT
    client_run(&state.client);
    window_title = "Game11 [Client]";
#endif

    srand(time(NULL));

    InitWindow(1080, 720, window_title);

    while (!WindowShouldClose()) {
        f32 delta_time = 0;
        update_entities(delta_time);
        physics(delta_time);
        draw_entities(delta_time);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);
        EndDrawing();
    }

#ifdef SERVER
    server_graceful_shutdown(&state.server);
#endif

#ifdef CLIENT
    client_graceful_shutdown(&state.client);
#endif

    CloseWindow();

    return 0;
}

void update_entities(f32 delta_time) {
    for (Entity &entity : state.entities) {
    }
}

void draw_entities(f32 delta_time) {
    for (Entity &entity : state.entities) {
    }
}

// AABB detection for a point against a box where the position is centred on the box
bool point_collision(v3 point, v3 collider_position, v3 collider_size) {
    v3 delta_position = point - collider_position;
    v3 bounding_box = collider_size * 0.5;

    return (
        delta_position.x >= -bounding_box.x && delta_position.x < bounding_box.x &&
        delta_position.y >= -bounding_box.y && delta_position.y < bounding_box.y &&
        delta_position.z >= -bounding_box.z && delta_position.z < bounding_box.z
    );
}

struct CubeCollision {
    bool collision;
    v3 overlap;
    v3 distance;
};

CubeCollision cube_collision(v3 a_position, v3 a_size, v3 b_position, v3 b_size) {
    v3 distance = b_position - a_position;
    v3 distance_abs = v3{ABS(distance.x), ABS(distance.y), ABS(distance.z)};
    v3 distance_for_collision = (a_size + b_size) * 0.5; 

    bool collision = distance_for_collision.x >= distance_abs.x && distance_for_collision.y >= distance_abs.y && distance_for_collision.z >= distance_abs.z;
    v3 overlap = distance_for_collision - distance_abs;

    return CubeCollision {
        .collision = collision,
        .overlap = overlap,
        .distance = distance
    };
}

void physics(f32 delta_time) {
    for (int i = 0; i < state.entities.len; i++) {
        Entity* entity = &state.entities[i];

        const f32 MAX_SPEED = 50;
        if (length(entity->velocity) > MAX_SPEED) {
            entity->velocity = norm(entity->velocity) * MAX_SPEED;
        }

        entity->position += entity->velocity * delta_time;
    }
}

Entity *spawn_entity(Entity entity) {
    Entity *ptr = push(&state.entities);
    *ptr = entity;

    return ptr;
}

void server_run(Server *server) {
    if (server->running) return;

    server->network_thread = std::thread([server] () { server_thread_entry(server); });
}

void server_thread_entry(Server *server) {
    // set this so any callbacks can then refer to the current running server
    // this should not be used directly unless for those callbacks and needs to
    // be cleaned up when the thread ends or aborts
    // - 31/07/25
    Server::instance = server;
    server->running = true;

    printf("Created server thread: %d\n", server->network_thread.get_id());

    SteamDatagramErrMsg error_message;
    if (!GameNetworkingSockets_Init(nullptr, error_message)) {
        printf("GameNetworkingSockets_Init failed %s\n", error_message);
        server->running = false;
        return;
    }
    
    server->start_time = SteamNetworkingUtils()->GetLocalTimestamp();
    SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, networking_debug_callback);

    server->interface = SteamNetworkingSockets();

    // init server 
    u16 port = 27020;

    SteamNetworkingIPAddr address; 
    address.Clear();
    address.m_port = port;

    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)server_network_connection_status_changed_callback);

    server->socket = server->interface->CreateListenSocketIP(address, 1, &opt);
    if (server->socket == k_HSteamListenSocket_Invalid) {
        printf("Error creating socket, failed to listen on port %d\n", port);
        server->running = false;
        return;
    }
    
    state.server.poll_group = state.server.interface->CreatePollGroup();
    if (state.server.poll_group == k_HSteamNetPollGroup_Invalid) {
        printf("Error creating poll group, failed to listen on port %d\n", port);
        server->running = false;
        return;
    }
    
    printf("Server listening on port %d\n", port);

    while (server->running) {
        // poll incoming messages 
        {
            while (state.server.running) {
                ISteamNetworkingMessage *incoming_message = NULL;
                int message_count = server->interface->ReceiveMessagesOnPollGroup(server->poll_group, &incoming_message, 1);

                if (message_count == 0) {
                    break;
                }

                ASSERT(message_count == 1 && incoming_message != NULL);

                // using std::string to get easy null termination for ease-of-use 
                std::string message;
                message.assign((const char *)incoming_message->m_pData, incoming_message->m_cbSize);
                printf("Received message from client [%d]: %s\n", incoming_message->m_conn, message.c_str());

                incoming_message->Release();
            }
        }

        server->interface->RunCallbacks();
    }

    printf("Shutting down server gracefully\n");

    for (HSteamNetConnection connection : server->connections) {
        server->interface->CloseConnection(connection, 0, "Server shutdown", true);
    }

    reset(&server->connections);

    server->interface->CloseListenSocket(server->socket);
    server->socket = k_HSteamListenSocket_Invalid;

    server->interface->DestroyPollGroup(server->poll_group);
    server->poll_group = k_HSteamNetPollGroup_Invalid;

    GameNetworkingSockets_Kill();
}

void server_on_connection_changed(Server *server, SteamNetConnectionStatusChangedCallback_t *info) {
    switch (info->m_info.m_eState) {
        case k_ESteamNetworkingConnectionState_None:                    break;
        case k_ESteamNetworkingConnectionState_ClosedByPeer:            break;
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:  break;
        case k_ESteamNetworkingConnectionState_Connected:               break;
        case k_ESteamNetworkingConnectionState_Connecting: {
            printf("Connection request from %s\n", info->m_info.m_szConnectionDescription);
    
            if (server->interface->AcceptConnection(info->m_hConn) != k_EResultOK) {
                // This could fail.  If the remote host tried to connect, but then
                // disconnected, the connection may already be half closed.  Just
                // destroy whatever we have on our side.
                server->interface->CloseConnection(info->m_hConn, 0, nullptr, false);
                printf("Can't accept connection\n");
                break;
            }
    
            if (!server->interface->SetConnectionPollGroup(info->m_hConn, server->poll_group)) {
                server->interface->CloseConnection(info->m_hConn, 0, nullptr, false );
                printf("Failed to set poll group for connection\n");
                break;
            }
    
            append(&server->connections, info->m_hConn);
            printf("Client connected!\n");
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

void client_run(Client *client) {
    if (client->running) return;

    client->network_thread = std::thread([client] () { client_thread_entry(client); });
}

void client_thread_entry(Client *client) {
    // set this so any callbacks can then refer to the current running client
    // this should not be used directly unless for those callbacks and needs to
    // be cleaned up when the thread ends or aborts
    // - 31/07/25
    Client::instance = client;
    client->running = true;

    printf("Created client thread: %d\n", client->network_thread.get_id());

    SteamDatagramErrMsg error_message;
    if (!GameNetworkingSockets_Init(nullptr, error_message)) {
        printf("GameNetworkingSockets_Init failed %s\n", error_message);
        client->running = false;
        return;
    }
    
    client->start_time = SteamNetworkingUtils()->GetLocalTimestamp();
    SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, networking_debug_callback);

    client->interface = SteamNetworkingSockets();
        
    // init client 
    u16 port = 27020;

    SteamNetworkingIPAddr address; 
    address.Clear();
    ASSERT(address.ParseString("::1"));
    address.m_port = port;

    char address_buffer[ SteamNetworkingIPAddr::k_cchMaxString ];
    address.ToString(address_buffer, sizeof(address_buffer), true);
    printf("Connecting to server at %s\n", address_buffer);

    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)client_network_connection_status_changed_callback);

    client->connection = client->interface->ConnectByIPAddress(address, 1, &opt);
    if (client->connection == k_HSteamNetConnection_Invalid ) {
        printf("Failed to create connection to server\n");
        client->running = false;
        return;
    }

    printf("Client connected on port %d\n", port);

    while (client->running) {
        state.client.interface->RunCallbacks();
    }

    printf("Shutting down client gracefully\n");

    client->interface->CloseConnection(client->connection, 0, nullptr, false);
    client->connection = k_HSteamNetConnection_Invalid;
    GameNetworkingSockets_Kill();
}

void client_on_connection_changed(Client *client, SteamNetConnectionStatusChangedCallback_t *info) {
    ASSERT(info->m_hConn == client->connection || client->connection == k_HSteamNetConnection_Invalid);

    switch (info->m_info.m_eState) {
        case k_ESteamNetworkingConnectionState_None:            break;
        case k_ESteamNetworkingConnectionState_ClosedByPeer:    break;
        case k_ESteamNetworkingConnectionState_Connecting:      break;
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
            client->running = false;
    
            if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting ) {
                printf("Tried to connect but failed: %s\n", info->m_info.m_szEndDebug );
            }
            else if (info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally ) {
                printf("Lost contact with the host: %s\n", info->m_info.m_szEndDebug );
            }
            else {
                printf("Disconnected from server %s\n", info->m_info.m_szEndDebug);
            }
    
            // Clean up the connection.  This is important!
            // The connection is "closed" in the network sense, but
            // it has not been destroyed.  We must close it on our end, too
            // to finish up.  The reason information do not matter in this case,
            // and we cannot linger because it's already closed on the other end,
            // so we just pass 0's.
            client->interface->CloseConnection(info->m_hConn, 0, nullptr, false );
            client->connection = k_HSteamNetConnection_Invalid;
        } break;
        case k_ESteamNetworkingConnectionState_Connected: {
            printf("Client connected to server\n");
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

void networking_debug_callback(ESteamNetworkingSocketsDebugOutputType type, const char *message) {
    printf("[NETWORK]: %s\n", message);

    if (type == k_ESteamNetworkingSocketsDebugOutputType_Bug) {
        printf("[NETWORK]: fatal error\n");
        BREAKPOINT;
    }
}
