#include "libs/libs.h"
#include "ack.cpp"
#include "libs/raylib/include/raylib.h"
#include "math.cpp"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <thread>
#include <chrono>
#include <mutex>
#include <queue>

// Total: 18:30
// Started: 14:00

#define MAX_ENTITIES 2000
#define MAX_BUBBLES 10
#define NETWORK_DELAY_MS 50
#define SERVER_ID 1

enum Team {
    TEAM_RED,
    TEAM_BLUE
};

// @entity
struct Entity {
    // meta
    u64 flags;
    u32 id;
    u32 owner;

    // base
    v3 position;
    v3 size;
    v3 velocity;

    // rendering
    Color colour;

    // bullet
    u32 bullet_origin;
};

typedef HSteamNetConnection ConnectionId;

enum NetworkMessageType {
    ASSIGN_CLIENT_ID,
    CLIENT_CONNECTED,
    SPAWN_ENTITY,
    SYNC_ENTITY,
    DELETE_ENTITY,
}; 

struct NetworkMessage {
    NetworkMessageType type;
    
    union {
        ConnectionId assign_client_id;
        u32 client_connected;
        Entity spawn_entity;
        Entity sync_entity;
        u32 delete_entity;
    };
}; 

enum EntityFlags {
    EF_PLAYER           = 1 << 0,
    EF_BULLET           = 1 << 1,
    EF_DELETE           = 1 << 16,
};

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
    ISteamNetworkingSockets *interface;
    SteamNetworkingMicroseconds start_time;
    HSteamListenSocket socket;
    HSteamNetPollGroup poll_group;
    StackArray<HSteamNetConnection, 10> connections;
    NetworkQueue in_queue;
};

// @client
struct Client {
    inline static Client *instance = NULL; // used to keep client instance around for network callbacks

    Arena arena;
    bool running;
    std::thread network_thread;
    ISteamNetworkingSockets *interface;
    SteamNetworkingMicroseconds start_time;
    HSteamNetConnection connection;
    NetworkQueue in_queue;
};

// @state
struct State {
    u32 id;
    f64 time;

    const char *title;
    v2 window_size;

    Server server;
    Client client;

    StackArray<Entity, MAX_ENTITIES> entities;
} state = {};

void update(f32 delta_time);
void update_network();
void on_server_receive(NetworkMessage *message);
void on_client_receive(NetworkMessage *message);
void update_entities(f32 delta_time);

void draw_entities(f32 delta_time);
void physics(f32 delta_time);

u32 new_entity_id();
Entity *spawn_entity_local(Entity entity);
void spawn_entity_on_server(Entity entity);
void delete_entity_local(u32 id);
void delete_entity_on_server(u32 id);
Entity *get_entity_with_id(u32 id);
bool entities_overlap(Entity *a, Entity *b);

void network_queue_push(NetworkQueue *network_queue, Slice<u8> message);
bool network_queue_pop(NetworkQueue *network_queue, Slice<u8> *out_message);

bool is_server();
bool is_client();

void server_run(Server *server);
void server_thread_entry(Server *server);
void server_on_connection_changed(Server *server, SteamNetConnectionStatusChangedCallback_t *info);
void server_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info);
void server_graceful_shutdown(Server *server);
void server_send_to_client(Server *server, Slice<u8> message, ConnectionId id);
void server_send_to_all_clients(Server *server, Slice<u8> message, ConnectionId exclude = 0);

void client_run(Client *client);
void client_thread_entry(Client *client);
void client_on_connection_changed(Client *client, SteamNetConnectionStatusChangedCallback_t *info);
void client_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info);
void client_graceful_shutdown(Client *client);
void client_send_to_server(Client *client, Slice<u8> message);

void networking_debug_callback(ESteamNetworkingSocketsDebugOutputType type, const char *message);

v2 v2_cast(Vector2 v);
v3 v3_cast(Vector2 v);
v3 v3_cast(Vector3 v);

// @main
int main() {
    state.title = is_server() ? "Game11 [Server]" : "Game11 [Client]";
    state.window_size = v2{1080, 720};

#ifdef SERVER
    state.id = SERVER_ID;
    server_run(&state.server);
#endif

#ifdef CLIENT
    client_run(&state.client);
#endif

    srand(time(NULL));

    InitWindow(i32(state.window_size.x), i32(state.window_size.y), state.title);
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        f32 delta_time = GetFrameTime();

        update(delta_time);
        physics(delta_time);
        draw_entities(delta_time);

        BeginDrawing();
        ClearBackground(RAYWHITE);
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

void update(f32 delta_time) {
    update_network();
    update_entities(delta_time);
}

void update_network() {
    if (is_server()) {
        Slice<u8> bytes;
        while (network_queue_pop(&state.server.in_queue, &bytes)) {
            NetworkMessage *message = (NetworkMessage *) bytes.ptr;
            on_server_receive(message);
            slice_free(bytes);
        }

        for (Entity &entity : state.entities) {
            if (entity.owner != SERVER_ID) {
                continue;
            }
    
            NetworkMessage message = NetworkMessage{.type = SYNC_ENTITY, .sync_entity = entity};
            server_send_to_all_clients(&state.server, bytes_from_ptr(&message));
        }
    }

    if (is_client()) {
        Slice<u8> bytes;
        while (network_queue_pop(&state.client.in_queue, &bytes)) {
            NetworkMessage *message = (NetworkMessage *) bytes.ptr;
            on_client_receive(message);
            slice_free(bytes);
        }

        for (Entity &entity : state.entities) {
            if (entity.owner != state.id) {
                continue;
            }
    
            NetworkMessage message = NetworkMessage{.type = SYNC_ENTITY, .sync_entity = entity};
            client_send_to_server(&state.client, bytes_from_ptr(&message));
        }
    }
}

void on_server_receive(NetworkMessage *message) {
    switch (message->type) {
        case CLIENT_CONNECTED: {
            // when client connects, the server generates this message and a few things are required to happen
            // 1. The client is assigned an id from the server
            // 2. Any existing entities are sent to the new client to spawn
            // 3. The player entity is spawn on all clients and is owned by the new client
            // - 09/08/25

            ConnectionId connection_id = message->client_connected;

            { // assign client id
                NetworkMessage message = NetworkMessage{.type = ASSIGN_CLIENT_ID, .assign_client_id = connection_id};
                server_send_to_client(&state.server, bytes_from_ptr(&message), connection_id);
                printf("New client connected, assigning id of: %u\n", connection_id);
            }

            { // spawn any entities on new client
                for (Entity &entity : state.entities) {
                    NetworkMessage message = NetworkMessage{.type = SPAWN_ENTITY, .spawn_entity = entity};
                    server_send_to_client(&state.server, bytes_from_ptr(&message), connection_id);
                }
            }

            { // spawn new player entity on all clients
                Entity new_player = Entity {
                    .flags = EF_PLAYER,
                    .id = new_entity_id(),
                    .owner = connection_id,
                    .position = {50, 50, 0},
                    .size = {50, 50},
                    .colour = RED 
                };

                spawn_entity_local(new_player);

                NetworkMessage message = NetworkMessage{.type = SPAWN_ENTITY, .spawn_entity = new_player};
                server_send_to_all_clients(&state.server, bytes_from_ptr(&message));
            }
        } break;
        case SPAWN_ENTITY: {
            Entity entity = message->spawn_entity;
            entity.id = new_entity_id();

            spawn_entity_local(entity);
            NetworkMessage message = NetworkMessage{.type = SPAWN_ENTITY, .spawn_entity = entity};
            server_send_to_all_clients(&state.server, bytes_from_ptr(&message));
        } break;
        case SYNC_ENTITY: {
            Entity *entity = get_entity_with_id(message->sync_entity.id);
            if (entity != NULL && entity->owner != SERVER_ID) {
                *entity = message->sync_entity;

                NetworkMessage message = NetworkMessage{.type = SYNC_ENTITY, .sync_entity = *entity};
                server_send_to_all_clients(&state.server, bytes_from_ptr(&message), entity->owner);
            }
        } break;
        case DELETE_ENTITY: {
            u32 id = message->delete_entity;

            delete_entity_local(id);
            NetworkMessage message = NetworkMessage{.type = DELETE_ENTITY, .delete_entity = id};
            server_send_to_all_clients(&state.server, bytes_from_ptr(&message));
        } break;
        default: {
            logln("WARNING unknown message sent");
        } break;
    }
}

void on_client_receive(NetworkMessage *message) {
    switch (message->type) {
        case ASSIGN_CLIENT_ID: {
            state.id = message->assign_client_id;
            printf("Assigned id of: %u\n", state.id);
        } break;
        case SPAWN_ENTITY: {
            spawn_entity_local(message->spawn_entity);
        } break;
        case SYNC_ENTITY: {
            Entity *entity = get_entity_with_id(message->sync_entity.id);
            if (entity != NULL) {
                *entity = message->sync_entity;
            }
        } break;
        case DELETE_ENTITY: {
            delete_entity_local(message->delete_entity);
        } break;
        default: {
            logln("WARNING unknown message sent");
        } break;
    }
}

void update_entities(f32 delta_time) {
    for (Entity &entity : state.entities) {
        if (entity.owner != state.id) {
            continue;
        }

        if (BIT_SET(entity.flags, EF_PLAYER)) {
            { // movement
                f32 move_speed = 10;
    
                if (IsKeyDown(KEY_A)) {
                    entity.position.x -= move_speed;
                }
    
                if (IsKeyDown(KEY_D)) {
                    entity.position.x += move_speed;
                }
    
                if (IsKeyDown(KEY_W)) {
                    entity.position.y -= move_speed;
                }
    
                if (IsKeyDown(KEY_S)) {
                    entity.position.y += move_speed;
                }
            }

            // firing bullets
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                f32 bullet_speed = 500;
                v3 direction =  norm(v3_cast(GetMousePosition()) - entity.position);

                Entity bullet = Entity {
                    .flags = EF_BULLET,
                    .owner = SERVER_ID,
                    .position = entity.position,
                    .size = v3{20, 20, 0},
                    .velocity = v3{direction.x, direction.y, 0} * bullet_speed,
                    .colour = RED,
                    .bullet_origin = entity.id,
                };

                spawn_entity_on_server(bullet);
            }

            // bullet collision detection
            for (Entity &other : state.entities) {
                if (!BIT_SET(other.flags, EF_BULLET)) {
                    continue;
                }

                if (other.bullet_origin == entity.id) {
                    continue;
                }

                if (!entities_overlap(&entity, &other)) {
                    continue;
                }

                delete_entity_on_server(other.id);
            }
        }

        if (BIT_SET(entity.flags, EF_BULLET)) {
            if ((entity.position.x < -entity.size.x || entity.position.x > state.window_size.x + entity.size.x) ||
                (entity.position.y < -entity.size.y || entity.position.y > state.window_size.y + entity.size.y)) {
                SET_BIT(entity.flags, EF_DELETE);
            }
        }
    }
}

void draw_entities(f32 delta_time) {
    for (Entity &entity : state.entities) {
        if (BIT_SET(entity.flags, EF_PLAYER)) {
            DrawRectangle(i32(entity.position.x), i32(entity.position.y), i32(entity.size.x), i32(entity.size.y), entity.colour);
        }
        else {
            DrawCircle(i32(entity.position.x), i32(entity.position.y), i32(entity.size.x), entity.colour);
        }
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

void physics(f32 delta_time) {
    for (Entity &entity : state.entities) {
        entity.position += entity.velocity * delta_time;
    }
}

u32 new_entity_id() {
    static u32 id = 0;
    return id++;
}

Entity *spawn_entity_local(Entity entity) {
    Entity *ptr = push(&state.entities);
    *ptr = entity;

    return ptr;
}

void delete_entity_local(u32 id) {
    for (i64 i = 0; i < state.entities.len; i++) {
        Entity *entity = &state.entities[i];

        if (entity->id == id) {
            swap_remove(&state.entities, i);
            return;
        }
    }
}

void spawn_entity_on_server(Entity entity) {
    NetworkMessage message = NetworkMessage{.type = SPAWN_ENTITY, .spawn_entity = entity};
    client_send_to_server(&state.client, bytes_from_ptr(&message));
}

void delete_entity_on_server(u32 id) {
    NetworkMessage message = NetworkMessage{.type = DELETE_ENTITY, .delete_entity = id};
    client_send_to_server(&state.client, bytes_from_ptr(&message));
}

Entity *get_entity_with_id(u32 id) {
    for (Entity &entity : state.entities) {
        if (entity.id == id) {
            return &entity;
        }
    }

    return NULL;
}

bool entities_overlap(Entity *a, Entity *b) {
    // Compute edges
    float a_min_x = a->position.x;
    float a_max_x = a->position.x + a->size.x;
    float a_min_y = a->position.y;
    float a_max_y = a->position.y + a->size.y;

    float b_min_x = b->position.x;
    float b_max_x = b->position.x + b->size.x;
    float b_min_y = b->position.y;
    float b_max_y = b->position.y + b->size.y;

    // Check for separation along x and y axes
    bool overlapX = (a_min_x < b_max_x) && (a_max_x > b_min_x);
    bool overlapY = (a_min_y < b_max_y) && (a_max_y > b_min_y);

    return overlapX && overlapY;
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

void server_send_to_client(Server *server, Slice<u8> message, ConnectionId id) {
    server->interface->SendMessageToConnection(id, message.ptr, message.len, k_nSteamNetworkingSend_Reliable, NULL);
}

void server_send_to_all_clients(Server *server, Slice<u8> message, ConnectionId exclude) {
    for (ConnectionId id : server->connections) {
        if (exclude != 0 && id == exclude) continue;

        server->interface->SendMessageToConnection(id, message.ptr, message.len, k_nSteamNetworkingSend_Reliable, NULL);
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

void client_send_to_server(Client *client, Slice<u8> message) {
    client->interface->SendMessageToConnection(client->connection, message.ptr, message.len, k_nSteamNetworkingSend_Reliable, NULL);
}

void networking_debug_callback(ESteamNetworkingSocketsDebugOutputType type, const char *message) {
    printf("[NETWORK]: %s\n", message);

    if (type == k_ESteamNetworkingSocketsDebugOutputType_Bug) {
        printf("[NETWORK]: fatal error\n");
        BREAKPOINT;
    }
}

v2 v2_cast(Vector2 v) {
    return v2{v.x, v.y};
}

v3 v3_cast(Vector2 v) {
    return v3{v.x, v.y, 0};
}

v3 v3_cast(Vector3 v) {
    return v3{v.x, v.y, v.z};
}
