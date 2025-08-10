#include "libs/libs.h"
#include "ack.cpp"
#include "math.cpp"
#include "net.cpp"
#include "platform.h"

#include <atomic>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <chrono>

// Total: 29:00
// Started: 21:30

#define MAX_ENTITIES 100
#define SERVER_ID 1

using chrono_clock = std::chrono::steady_clock;

// @entity
struct Entity {
    // meta
    u64 flags;
    u32 id;

    // networking
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
        u32 assign_client_id;  // Changed from ConnectionId to u32
        ConnectionId client_connected;
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

enum InstanceType {
    IT_CLIENT,
    IT_SERVER
};

// @state
struct State {
    InstanceType instance_type;
    u32 instance_id;

    Arena arena;
    StackArray<Entity, MAX_ENTITIES> entities;
};

// @server @gameserver
struct GameServer {
    std::thread thread;
    std::atomic<bool> shutdown_signal;

    State state;
};

// @client @gameclient
struct GameClient {
    const char *title;
    v2 window_size;

    State state;
};

GameServer *g_game_server = NULL;
GameClient *g_game_client = NULL;

void game_server_start();
void game_server_stop();

GameClient game_client_create();
void game_client_start(GameClient *instance);

void update_network(State *state);
void on_server_receive(State *state, NetworkMessage *message);
void on_client_receive(State *state, NetworkMessage *message);
void update_entities(State *state, f32 delta_time);

void draw(State *state, f32 delta_time);
void physics(State *state, f32 delta_time);

u32 new_entity_id();
Entity *local_spawn_entity(State *state, Entity entity);
void server_spawn_entity(Entity entity);
void local_delete_entity(State *state, u32 id);
void server_delete_entity(u32 id);
Entity *get_entity_with_id(State *state, u32 id);
bool entities_overlap(Entity *a, Entity *b);

bool is_server(State *state);
bool is_client(State *state);
void server_on_new_connection(NetworkLayer *net, Server *server, ConnectionId id);

v2 v2_cast(Vector2 v);
v3 v3_cast(Vector2 v);
v3 v3_cast(Vector3 v);

// @main
int main(i32 argc, const char **argv) {
    // client
    // - start client instance
    // - start game:
    //      - start server instance
    //      - start server on network
    //      - start client on network
    //  - end game:
    //      - stop server instance
    //      - stop server on network
    //      - stop client on network


    log_set_thread_options(LogOptions {
        .thread_name = "CLIENT",
        .thread_colour = GREEN_ASCII_CODE,
    });

    bool ok = network_layer_init();

    if (!ok) {
        logln("CRASH: failed to strart networking");
        return 1;
    }

    NET()->server.on_new_connection = server_on_new_connection;

    network_layer_start();

    GameClient client_instance = game_client_create();
    game_client_start(&client_instance);

    network_layer_stop();
}

void game_server_start() {
    ASSERT(g_game_server == NULL);

    // my strategy for this is init everything in the instance
    // besides the state object before starting the new thread
    // then it is up to the server thread to init the state
    // and go from there

    g_game_server = new GameServer {};
    g_game_server->shutdown_signal = false;

    g_game_server->thread = std::thread([] () {
    log_set_thread_options(LogOptions {
        .thread_name = "SERVER",
        .thread_colour = YELLOW_ASCII_CODE,
    });

    g_game_server->state = State {
        .instance_type = IT_SERVER,
        .instance_id = SERVER_ID,
        .arena = arena_create(10 * 1024 * 1024),
        .entities = stack_array_create<Entity, MAX_ENTITIES>(),
    };

    logln_fmt(&g_game_server->state.arena, "Started game server [thread={}]", get_current_thread_id());

    auto tick_interval = std::chrono::milliseconds(20);     // 20ms/tick -> 50/s
    auto network_interval = std::chrono::milliseconds(100); // 100ms/tick -> 10/s

    auto tick_timer = std::chrono::milliseconds(0);
    auto network_timer = std::chrono::milliseconds(0);

    auto previous_time = chrono_clock::now();

    while (!g_game_server->shutdown_signal) {
        auto current_time = chrono_clock::now();
        auto delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - previous_time);
        previous_time = current_time;

        tick_timer += delta_time;
        network_timer += delta_time;

        if (tick_timer >= tick_interval) {
            tick_timer -= tick_interval; // is this good??

            f32 delta_time_f32 = static_cast<f32>(std::chrono::duration<f32>(tick_interval).count());

            update_entities(&g_game_server->state, delta_time_f32);
            physics(&g_game_server->state, delta_time_f32);
        }

        if (network_timer >= network_interval) {
            network_timer -= network_interval; // is this good??

            update_network(&g_game_server->state);
        }

        arena_reset(&g_game_server->state.arena);

	std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    logln("Game server was given shutdown signal.. stopping");
    }); // thread lambda end
}

void game_server_stop() {
    if (g_game_server == NULL) {
        return;
    }

    g_game_server->shutdown_signal = true;
    g_game_server->thread.join();

    delete g_game_server;
    g_game_server = NULL;
}

GameClient game_client_create() {
    return GameClient {
        .title = "Game11",
        .window_size = v2{1080, 720},
        .state = {},
    };
}

void game_client_start(GameClient *instance) {
    instance->state = State {
        .instance_type = IT_CLIENT,
        .instance_id = 0,
        .arena = arena_create(10 * 1024 * 1024),
        .entities = stack_array_create<Entity, MAX_ENTITIES>(),
    };

    logln_fmt(&instance->state.arena, "Started client instance [thread={}]", get_current_thread_id());

    srand(time(NULL));

    SetTraceLogLevel(LOG_ERROR);
    SetTargetFPS(60);

    InitWindow(i32(instance->window_size.x), i32(instance->window_size.y), instance->title);

    while (!WindowShouldClose()) {
        f32 delta_time = GetFrameTime();

        if (IsKeyPressed(KEY_ONE)) {
            game_server_start();

            network_layer_start_server(NET());
            network_layer_start_client(NET(), "127.0.0.1");
        }

        update_network(&instance->state);
        update_entities(&instance->state, delta_time);
        physics(&instance->state, delta_time);
        draw(&instance->state, delta_time);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        EndDrawing();

        arena_reset(&instance->state.arena);
    }

    game_server_stop();
    network_layer_stop_server(NET());
    network_layer_stop_client(NET());

    CloseWindow();
}

void update_network(State *state) {
    if (is_server(state)) {
        Slice<u8> bytes;
        while (network_queue_pop(&NET()->server_in_queue, &bytes)) {
            NetworkMessage *message = (NetworkMessage *) bytes.ptr;
            on_server_receive(state, message);
            slice_free(bytes);
        }

        for (Entity &entity : state->entities) {
            if (entity.owner != SERVER_ID) {
                continue;
            }
            
            NetworkMessage message = NetworkMessage{.type = SYNC_ENTITY, .sync_entity = entity};
            server_send_to_all_clients(NET(), bytes_from_ptr(&message));
        }
    }

    if (is_client(state)) {
        Slice<u8> bytes;
        while (network_queue_pop(&NET()->client_in_queue, &bytes)) {
            NetworkMessage *message = (NetworkMessage *) bytes.ptr;
            on_client_receive(state, message);
            slice_free(bytes);
        }

        for (Entity &entity : state->entities) {
            if (entity.owner != state->instance_id) {
                continue;
            }
        
            NetworkMessage message = NetworkMessage{.type = SYNC_ENTITY, .sync_entity = entity};
            client_send_to_server(NET(), bytes_from_ptr(&message));
        }
    }
}

void on_server_receive(State *state, NetworkMessage *message) {
    switch (message->type) {
        case CLIENT_CONNECTED: {
            // when client connects, the server generates this message and a few things are required to happen
            // 1. The client is assigned an id from the server
            // 2. Any existing entities are sent to the new client to spawn
            // 3. The player entity is spawn on all clients and is owned by the new client
            // - 09/08/25

            ConnectionId connection_id = message->client_connected;
            logln_fmt(&state->arena, "Processing new client connection: connection_id={}", connection_id);

            { // assign client id
                logln_fmt(&state->arena, "Assigning new client: id={}", connection_id);

                NetworkMessage message = NetworkMessage{.type = ASSIGN_CLIENT_ID, .assign_client_id = connection_id};
                server_send_to_client(NET(), bytes_from_ptr(&message), connection_id);
            }

            { // spawn any entities on new client
                logln_fmt(&state->arena, "Spawning {} existing entities on new client", state->entities.len);

                for (Entity &entity : state->entities) {
                    NetworkMessage message = NetworkMessage{.type = SPAWN_ENTITY, .spawn_entity = entity};
                    server_send_to_client(NET(), bytes_from_ptr(&message), connection_id);
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

                logln_fmt(&state->arena, "Spawning new player entity: entity_id={}, owner={}", new_player.id, new_player.owner);
                local_spawn_entity(state, new_player);

                NetworkMessage message = NetworkMessage{.type = SPAWN_ENTITY, .spawn_entity = new_player};
                server_send_to_all_clients(NET(), bytes_from_ptr(&message));
            }
        } break;
        case SPAWN_ENTITY: {
            Entity entity = message->spawn_entity;
            entity.id = new_entity_id();

            logln_fmt(&state->arena, "Server spawning entity: id={}, owner={}", entity.id, entity.owner);
            local_spawn_entity(state, entity);
            NetworkMessage message = NetworkMessage{.type = SPAWN_ENTITY, .spawn_entity = entity};
            server_send_to_all_clients(NET(), bytes_from_ptr(&message));
        } break;
        case SYNC_ENTITY: {
            Entity *entity = get_entity_with_id(state, message->sync_entity.id);
            if (entity != NULL && entity->owner != SERVER_ID) {
                *entity = message->sync_entity;

                NetworkMessage message = NetworkMessage{.type = SYNC_ENTITY, .sync_entity = *entity};
                server_send_to_all_clients(NET(), bytes_from_ptr(&message), entity->owner);
            }
        } break;
        case DELETE_ENTITY: {
            u32 id = message->delete_entity;

            logln_fmt(&state->arena, "Server deleting entity: id={}", id);
            local_delete_entity(state, id);
            NetworkMessage message = NetworkMessage{.type = DELETE_ENTITY, .delete_entity = id};
            server_send_to_all_clients(NET(), bytes_from_ptr(&message));
        } break;
        default: {
            logln("WARNING unknown message sent");
        } break;
    }
}

void on_client_receive(State *state, NetworkMessage *message) {
    switch (message->type) {
        case ASSIGN_CLIENT_ID: {
            state->instance_id = message->assign_client_id;
            logln_fmt(&state->arena, "Client assigned id={}", state->instance_id);
        } break;
        case SPAWN_ENTITY: {
            logln_fmt(&state->arena, "Client spawning entity: id={}, owner={}", message->spawn_entity.id, message->spawn_entity.owner);
            local_spawn_entity(state, message->spawn_entity);
        } break;
        case SYNC_ENTITY: {
            Entity *entity = get_entity_with_id(state, message->sync_entity.id);
            if (entity != NULL) {
                *entity = message->sync_entity;
            }
        } break;
        case DELETE_ENTITY: {
            logln_fmt(&state->arena, "Client deleting entity: id={}", message->delete_entity);
            local_delete_entity(state, message->delete_entity);
        } break;
        default: {
            logln("WARNING unknown message sent");
        } break;
    }
}

void update_entities(State *state, f32 delta_time) {
    for (Entity &entity : state->entities) {
        if (state->instance_id != entity.owner) {
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
                f32 bullet_speed = 1000;
                v3 direction = norm(v3_cast(GetMousePosition()) - entity.position);

                Entity bullet = Entity {
                    .flags = EF_BULLET,
                    .owner = SERVER_ID,
                    .position = entity.position,
                    .size = v3{20, 20, 0},
                    .velocity = v3{direction.x, direction.y, 0} * bullet_speed,
                    .colour = RED,
                    .bullet_origin = entity.id,
                };

                server_spawn_entity(bullet);
            }
        }

        // if (is_server() && BIT_SET(entity.flags, EF_BULLET)) {
            // if ((entity.position.x < -entity.size.x || entity.position.x > state.window_size.x + entity.size.x) ||
                // (entity.position.y < -entity.size.y || entity.position.y > state.window_size.y + entity.size.y)) {
                // SET_BIT(entity.flags, EF_DELETE);
            // }
        // }
    }
}

void draw(State *state, f32 delta_time) {
    for (Entity &entity : state->entities) {
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

void physics(State *state, f32 delta_time) {
    for (Entity &entity : state->entities) {
        entity.position += entity.velocity * delta_time;
    }
}

u32 new_entity_id() {
    static u32 id = 0;
    return id++;
}

Entity *local_spawn_entity(State *state, Entity entity) {
    Entity *ptr = push(&state->entities);
    *ptr = entity;

    return ptr;
}

void local_delete_entity(State *state, u32 id) {
    for (i64 i = 0; i < state->entities.len; i++) {
        Entity *entity = &state->entities[i];

        if (entity->id == id) {
            swap_remove(&state->entities, i);
            return;
        }
    }
}

void server_spawn_entity(Entity entity) {
    NetworkMessage message = NetworkMessage{.type = SPAWN_ENTITY, .spawn_entity = entity};
    client_send_to_server(NET(), bytes_from_ptr(&message));
}

void server_delete_entity(u32 id) {
    NetworkMessage message = NetworkMessage{.type = DELETE_ENTITY, .delete_entity = id};
    client_send_to_server(NET(), bytes_from_ptr(&message));
}

Entity *get_entity_with_id(State *state, u32 id) {
    for (Entity &entity : state->entities) {
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

bool is_server(State *state) {
    return state->instance_type == IT_SERVER;
}

bool is_client(State *state) {
    return state->instance_type == IT_CLIENT;
}

void server_on_new_connection(NetworkLayer *net, Server *server, ConnectionId id) {
    logln_fmt(&net->arena, "New connection received, sending server new connection message [thread={}]", get_current_thread_id());

    NetworkMessage message = NetworkMessage {.type = CLIENT_CONNECTED, .client_connected = id};
    network_queue_push(&net->server_in_queue, bytes_from_ptr(&message));
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
