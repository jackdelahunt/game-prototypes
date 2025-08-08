#include "libs/libs.h"
#include "ack.cpp"
#include "libs/raylib/include/raylib.h"
#include "math.cpp"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include <format>

// Total: 17:00
// Started: 17:30

#define MAX_ENTITIES 2000
#define NETWORK_DELAY_MS 50
#define SERVER_ID 1

struct Entity {
    // meta
    u64 flags;
    u32 owner;

    // base
    v3 position;
    v3 size;
    v3 rotation;
    v3 velocity;

    // rendering
    v4 colour;
};

enum NetworkMessageType {
    ASSIGN_PLAYER_ENTITY,
    CLIENT_CONNECTED,
    SPAWN_ENTITY,
    SYNC_ENTITY,
}; 

struct NetworkMessage {
    NetworkMessageType type;
    
    union {
        Entity assign_player_entity;
        u32 client_connected;
        Entity spawn_entity;
        Entity sync_entity;
    };
}; 

enum EntityFlags {
    EF_PLAYER           = 1 << 0,
    EF_DELETE           = 1 << 16,
};

struct NetworkQueue {
    std::mutex mutex;
    std::queue<Slice<u8>> messages;
};

struct Server {
    inline static Server *instance = NULL; // used to keep server instance around for network callbacks

    Arena arena;
    bool running;

    std::thread network_thread;
    ISteamNetworkingSockets *interface;
    SteamNetworkingMicroseconds start_time;
    HSteamListenSocket socket;
    HSteamNetPollGroup poll_group;
    StackArray<HSteamNetConnection, 10> connections;

    NetworkQueue in_queue;
    NetworkQueue out_queue;
};

struct Client {
    inline static Client *instance = NULL; // used to keep client instance around for network callbacks

    Arena arena;
    bool running;
    std::thread network_thread;
    ISteamNetworkingSockets *interface;
    SteamNetworkingMicroseconds start_time;
    HSteamNetConnection connection;

    NetworkQueue in_queue;
    NetworkQueue out_queue;
};

struct State {
    u32 id;
    f64 time;

    Server server;
    Client client;

    StackArray<Entity, MAX_ENTITIES> entities;
} state = {};

void update_network();
void update_entities(f32 delta_time);
void draw_entities(f32 delta_time);
void physics(f32 delta_time);

Entity *spawn_entity(Entity entity);

void network_queue_push(NetworkQueue *network_queue, Slice<u8> message);
bool network_queue_pop(NetworkQueue *network_queue, Slice<u8> *out_message);

bool is_server();
bool is_client();

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
    const char *window_title = NULL;

#ifdef SERVER
    server_run(&state.server);
    window_title = "Game11 [Server]";
    state.id = SERVER_ID;
#endif

#ifdef CLIENT
    client_run(&state.client);
    window_title = "Game11 [Client]";
#endif

    {
        Arena arena = arena_create(1024);
        str s = fmt(&arena, "My name is {} and I am {} years old. Am I irish? {}", "jack", 24, true);
        logln(s);
    }

    srand(time(NULL));

    InitWindow(1080, 720, window_title);
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        f32 delta_time = 0;

        if (is_client()) {
        }

        update_network();
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

void update_network() {
    // while these get assign based on type of the game it is, and therefore are used
    // when ever sending in or out messages, that does not mean that every type of
    // message is valid to be sent or recieved. This is just for ease of use
    NetworkQueue *in_queue = is_server() ? &state.server.in_queue : &state.client.in_queue;
    NetworkQueue *out_queue = is_server() ? &state.server.out_queue : &state.client.out_queue;

    // check incoming messages
    Slice<u8> bytes;
    while (network_queue_pop(in_queue, &bytes)) {
        NetworkMessage *in = (NetworkMessage *) bytes.ptr;

        switch (in->type) {
            case ASSIGN_PLAYER_ENTITY: {
                printf("Assigned id of: %u\n", in->assign_player_entity.owner);
                state.id = in->assign_player_entity.owner;

                spawn_entity(in->assign_player_entity);
            } break;
            case CLIENT_CONNECTED: { // generated by server when a new client connects
                u32 client_id = in->client_connected;
                printf("New client connected, assigning id of: %u\n", client_id);

                Entity e = Entity {
                    .flags = EF_PLAYER,
                    .owner = client_id,
                    .position = {50, 50, 0},
                    .size = {50, 50},
                    .colour = {1, 0, 0, 1}
                };

                NetworkMessage message = NetworkMessage{.type = ASSIGN_PLAYER_ENTITY, .assign_player_entity = e};
                network_queue_push(out_queue, bytes_from_ptr(&message));

                spawn_entity(e);
                network_queue_push(out_queue, bytes_from_ptr(&message));
            } break;
            case SPAWN_ENTITY: {
                spawn_entity(in->spawn_entity);
            } break;
            case SYNC_ENTITY: {
                for (Entity &entity : state.entities) {
                    if (entity.owner != in->sync_entity.owner) {
                        continue;
                    }

                    entity = in->sync_entity;
                }
            } break;
            default: {
                logln("WARNING unknown message sent");
            } break;
        }

        slice_free(bytes);
    }

    // sync any entities that need to 
    for (Entity &entity : state.entities) {
        if (entity.owner != state.id) {
            continue;
        }

        NetworkMessage message = NetworkMessage{.type = SYNC_ENTITY, .sync_entity = entity};
        network_queue_push(out_queue, bytes_from_ptr(&message));
    }
}

void update_entities(f32 delta_time) {
    for (Entity &entity : state.entities) {
        if (BIT_SET(entity.flags, EF_PLAYER)) {
            if (entity.owner != state.id) {
                continue;
            }

            f32 speed = 10;

            if (IsKeyDown(KEY_A)) {
                entity.position.x -= speed;
            }

            if (IsKeyDown(KEY_D)) {
                entity.position.x += speed;
            }

            if (IsKeyDown(KEY_W)) {
                entity.position.y -= speed;
            }

            if (IsKeyDown(KEY_S)) {
                entity.position.y += speed;
            }
        }
    }
}

void draw_entities(f32 delta_time) {
    for (Entity &entity : state.entities) {
        DrawRectangle(i32(entity.position.x), i32(entity.position.y), i32(entity.size.x), i32(entity.size.y), BLUE);
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

void network_queue_push(NetworkQueue *network_queue, Slice<u8> message) {
    std::scoped_lock lock(network_queue->mutex);

    Slice<u8> message_copy = slice_create_malloc<u8>(message.len);
    slice_copy(message_copy, message);

    network_queue->messages.push(message_copy);
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

    server->network_thread = std::thread([server] () { server_thread_entry(server); });
}

void server_thread_entry(Server *server) {
    // set this so any callbacks can then refer to the current running server
    // this should not be used directly unless for those callbacks and needs to
    // be cleaned up when the thread ends or aborts
    // - 31/07/25
    Server::instance = server;
    server->running = true;

    logln("Created server thread");

    SteamDatagramErrMsg error_message;
    if (!GameNetworkingSockets_Init(nullptr, error_message)) {
        logln_fmt(&server->arena, "GameNetworkingSockets_Init failed {}", error_message);
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
        logln_fmt(&server->arena, "Error creating socket, failed to listen on port {}", port);
        server->running = false;
        return;
    }
    
    state.server.poll_group = state.server.interface->CreatePollGroup();
    if (state.server.poll_group == k_HSteamNetPollGroup_Invalid) {
        logln_fmt(&server->arena, "Error creating poll group, failed to listen on port {}", port);
        server->running = false;
        return;
    }
    
    logln_fmt(&server->arena, "Server listening on port {}", port);

    while (server->running) {
        // poll incoming messages 
        while (server->running) {
            ISteamNetworkingMessage *incoming_message = NULL;
            int message_count = server->interface->ReceiveMessagesOnPollGroup(server->poll_group, &incoming_message, 1);

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

        // send outbound messages
        while (server->running) {
            Slice<u8> message;
            bool message_exists = network_queue_pop(&server->out_queue, &message);

            if (!message_exists) {
                break;
            }
           
            for (HSteamNetConnection connection : server->connections) {
                server->interface->SendMessageToConnection(connection, message.ptr, message.len, k_nSteamNetworkingSend_Reliable, NULL);
            }

            slice_free(message);
        }

        server->interface->RunCallbacks();

	std::this_thread::sleep_for(std::chrono::milliseconds(NETWORK_DELAY_MS));
    }

    logln("Shutting down server gracefully");

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
            logln_fmt(&server->arena, "Connection request from {}", info->m_info.m_szConnectionDescription);
    
            if (server->interface->AcceptConnection(info->m_hConn) != k_EResultOK) {
                // This could fail.  If the remote host tried to connect, but then
                // disconnected, the connection may already be half closed.  Just
                // destroy whatever we have on our side.
                server->interface->CloseConnection(info->m_hConn, 0, nullptr, false);
                logln("Can't accept connection");
                break;
            }
    
            if (!server->interface->SetConnectionPollGroup(info->m_hConn, server->poll_group)) {
                server->interface->CloseConnection(info->m_hConn, 0, nullptr, false );
                logln("Failed to set poll group for connection");
                break;
            }
    
            append(&server->connections, info->m_hConn);
            NetworkMessage message = NetworkMessage{.type = CLIENT_CONNECTED, .client_connected = info->m_hConn};
            network_queue_push(&state.server.in_queue, bytes_from_ptr(&message));
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

    client->arena = arena_create(20 * 1024 * 1024); // 20MB

    client->network_thread = std::thread([client] () { client_thread_entry(client); });
}

void client_thread_entry(Client *client) {
    // set this so any callbacks can then refer to the current running client
    // this should not be used directly unless for those callbacks and needs to
    // be cleaned up when the thread ends or aborts
    // - 31/07/25
    Client::instance = client;
    client->running = true;

    logln("Created client thread");

    SteamDatagramErrMsg error_message;
    if (!GameNetworkingSockets_Init(nullptr, error_message)) {
        logln_fmt(&client->arena, "GameNetworkingSockets_Init failed: {}", error_message);
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
    logln_fmt(&client->arena, "Connecting to server at {}", address_buffer);

    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)client_network_connection_status_changed_callback);

    client->connection = client->interface->ConnectByIPAddress(address, 1, &opt);
    if (client->connection == k_HSteamNetConnection_Invalid ) {
        logln("Failed to create connection to server");
        client->running = false;
        return;
    }

    logln_fmt(&client->arena, "Client connected on port {}", port);

    while (client->running) {
        // poll incoming messages 
        while (client->running) {
            ISteamNetworkingMessage *incoming_message = NULL;
            i64 message_count = client->interface->ReceiveMessagesOnConnection(client->connection, &incoming_message, 1);

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

        // send outbound messages
        while (client->running) {
            Slice<u8> message;
            bool message_exists = network_queue_pop(&client->out_queue, &message);

            if (!message_exists) {
                break;
            }

            client->interface->SendMessageToConnection(client->connection, message.ptr, message.len, k_nSteamNetworkingSend_Reliable, NULL);

            slice_free(message);
        }

        client->interface->RunCallbacks();

	std::this_thread::sleep_for(std::chrono::milliseconds(NETWORK_DELAY_MS));
    }

    logln("Shutting down client gracefully");

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
                logln_fmt(&client->arena, "Tried to connect but failed: {}", info->m_info.m_szEndDebug);
            }
            else if (info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
                logln_fmt(&client->arena, "Lost contact with the host: {}", info->m_info.m_szEndDebug);
            }
            else {
                logln_fmt(&client->arena, "Disconnected from server {}", info->m_info.m_szEndDebug);
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

void networking_debug_callback(ESteamNetworkingSocketsDebugOutputType type, const char *message) {
    printf("[NETWORK]: %s\n", message);

    if (type == k_ESteamNetworkingSocketsDebugOutputType_Bug) {
        printf("[NETWORK]: fatal error\n");
        BREAKPOINT;
    }
}
