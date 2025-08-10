#include "libs/libs.h"
#include "ack.cpp"
#include "math.cpp"
#include "net.cpp"
#include "platform.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// Total: 24:00
// Started: 11:00

#define MAX_ENTITIES 2000
#define SERVER_ID 1

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

// @state
struct State {
    u32 id;
    f64 time;

    Arena arena;

    const char *title;
    v2 window_size;

    Net net;

    Server server;
    Client client;

    StackArray<Entity, MAX_ENTITIES> entities;
};

thread_local State state = {};

void start_client_instance(i32 argc, const char **argv);
void start_server_instance(i32 argc, const char **argv);

void update(f32 delta_time);
void update_network();
void on_server_receive(NetworkMessage *message);
void on_client_receive(NetworkMessage *message);
void update_entities(f32 delta_time);
void cleanup_entities();  // Add this function declaration

void draw_entities(f32 delta_time);
void physics(f32 delta_time);

u32 new_entity_id();
Entity *local_spawn_entity(Entity entity);
void server_spawn_entity(Entity entity);
void local_delete_entity(u32 id);
void server_delete_entity(u32 id);
Entity *get_entity_with_id(u32 id);
bool entities_overlap(Entity *a, Entity *b);

void server_on_new_connection(Server *server, ConnectionId id);

v2 v2_cast(Vector2 v);
v3 v3_cast(Vector2 v);
v3 v3_cast(Vector3 v);

// @main
int main(i32 argc, const char **argv) {
    log_set_thread_options(LogOptions {
        .thread_name = "CLIENT",
        .thread_colour = GREEN_ASCII_CODE,
    });

    bool ok = init_networking(&state.net);
    if (!ok) {
        logln("CRASH: failed to strart networking");
        return 1;
    }

    net_run(&state.net);

    state.net.thread.join();
#if 0
    std::thread server_thread = std::thread([argc, argv] () {
        log_set_thread_options(LogOptions {
            .thread_name = "SERVER",
            .thread_colour = YELLOW_ASCII_CODE,
        });

        start_server_instance(argc, argv);
    });

    start_client_instance(argc, argv);
#endif
}

void start_client_instance(i32 argc, const char **argv) {
    state.title = "Game11";
    state.window_size = v2{1080, 720};
    state.arena = arena_create(10 * 1024 * 1024);

    logln_fmt(&state.arena, "Started client instance [thread={}]", get_current_thread_id());

    client_run(&state.client, argc > 1 ? argv[1] : NULL);

    srand(time(NULL));

    SetTraceLogLevel(LOG_ERROR);

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

    client_graceful_shutdown(&state.client);

    CloseWindow();
}

void start_server_instance(i32 argc, const char **argv) {
    state.id = SERVER_ID;
    state.arena = arena_create(10 * 1024 * 1024);
    state.server.on_new_connection = server_on_new_connection;

    logln_fmt(&state.arena, "Started server instance [thread={}]", get_current_thread_id());

    server_run(&state.server);

    while (true) {
        f32 delta_time = 0.05;

        update(delta_time);
        physics(delta_time);
    }

    server_graceful_shutdown(&state.server);
}

void update(f32 delta_time) {
    update_network();
    update_entities(delta_time);
    cleanup_entities();  // Add cleanup call
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

        // Only send sync messages if we have a valid client ID
        if (state.id != 0) {
            for (Entity &entity : state.entities) {
                if (entity.owner != state.id) {
                    continue;
                }
        
                NetworkMessage message = NetworkMessage{.type = SYNC_ENTITY, .sync_entity = entity};
                client_send_to_server(&state.client, bytes_from_ptr(&message));
            }
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
            u32 new_client_id = new_entity_id();  // Generate a unique client ID

            logln_fmt(&state.arena, "Processing new client connection: connection_id={}, client_id={}", connection_id, new_client_id);

            { // assign client id
                NetworkMessage message = NetworkMessage{.type = ASSIGN_CLIENT_ID, .assign_client_id = new_client_id};
                server_send_to_client(&state.server, bytes_from_ptr(&message), connection_id);
                logln_fmt(&state.arena, "Assigning new client id={}", new_client_id);
            }

            { // spawn any entities on new client
                logln_fmt(&state.arena, "Spawning {} existing entities on new client", state.entities.len);
                for (Entity &entity : state.entities) {
                    NetworkMessage message = NetworkMessage{.type = SPAWN_ENTITY, .spawn_entity = entity};
                    server_send_to_client(&state.server, bytes_from_ptr(&message), connection_id);
                }
            }

            { // spawn new player entity on all clients
                Entity new_player = Entity {
                    .flags = EF_PLAYER,
                    .id = new_entity_id(),
                    .owner = new_client_id,  // Use the generated client ID
                    .position = {50, 50, 0},
                    .size = {50, 50},
                    .colour = RED 
                };

                logln_fmt(&state.arena, "Spawning new player entity: id={}, owner={}", new_player.id, new_player.owner);
                local_spawn_entity(new_player);

                NetworkMessage message = NetworkMessage{.type = SPAWN_ENTITY, .spawn_entity = new_player};
                server_send_to_all_clients(&state.server, bytes_from_ptr(&message));
            }
        } break;
        case SPAWN_ENTITY: {
            Entity entity = message->spawn_entity;
            entity.id = new_entity_id();

            logln_fmt(&state.arena, "Server spawning entity: id={}, owner={}", entity.id, entity.owner);
            local_spawn_entity(entity);
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

            logln_fmt(&state.arena, "Server deleting entity: id={}", id);
            local_delete_entity(id);
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
            logln_fmt(&state.arena, "Client assigned id={}", state.id);
        } break;
        case SPAWN_ENTITY: {
            logln_fmt(&state.arena, "Client spawning entity: id={}, owner={}", message->spawn_entity.id, message->spawn_entity.owner);
            local_spawn_entity(message->spawn_entity);
        } break;
        case SYNC_ENTITY: {
            Entity *entity = get_entity_with_id(message->sync_entity.id);
            if (entity != NULL) {
                *entity = message->sync_entity;
            }
        } break;
        case DELETE_ENTITY: {
            logln_fmt(&state.arena, "Client deleting entity: id={}", message->delete_entity);
            local_delete_entity(message->delete_entity);
        } break;
        default: {
            logln("WARNING unknown message sent");
        } break;
    }
}

void update_entities(f32 delta_time) {
    for (Entity &entity : state.entities) {
        // Only allow control if we have a valid ID (for client) or if we're the server
        if ((is_client() && state.id == 0) || (is_client() && entity.owner != state.id) || (is_server() && entity.owner != SERVER_ID)) {
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

                server_spawn_entity(bullet);
            }

            // bullet collision detection - only run on server to prevent conflicts
            if (is_server()) {
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

                    server_delete_entity(other.id);
                }
            }
        }

        // bullet cleanup - only run on server to prevent conflicts
        if (is_server() && BIT_SET(entity.flags, EF_BULLET)) {
            if ((entity.position.x < -entity.size.x || entity.position.x > state.window_size.x + entity.size.x) ||
                (entity.position.y < -entity.size.y || entity.position.y > state.window_size.y + entity.size.y)) {
                SET_BIT(entity.flags, EF_DELETE);
            }
        }
    }
}

void cleanup_entities() {
    // Remove entities marked for deletion
    for (i64 i = state.entities.len - 1; i >= 0; i--) {
        Entity *entity = &state.entities[i];
        if (BIT_SET(entity->flags, EF_DELETE)) {
            swap_remove(&state.entities, i);
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
        // Only update physics for entities we own
        if ((is_client() && entity.owner == state.id) || (is_server() && entity.owner == SERVER_ID)) {
            entity.position += entity.velocity * delta_time;
        }
    }
}

u32 new_entity_id() {
    static u32 id = 0;
    return id++;
}

Entity *local_spawn_entity(Entity entity) {
    Entity *ptr = push(&state.entities);
    *ptr = entity;

    return ptr;
}

void local_delete_entity(u32 id) {
    for (i64 i = 0; i < state.entities.len; i++) {
        Entity *entity = &state.entities[i];

        if (entity->id == id) {
            swap_remove(&state.entities, i);
            return;
        }
    }
}

void server_spawn_entity(Entity entity) {
    NetworkMessage message = NetworkMessage{.type = SPAWN_ENTITY, .spawn_entity = entity};
    client_send_to_server(&state.client, bytes_from_ptr(&message));
}

void server_delete_entity(u32 id) {
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

void server_on_new_connection(Server *server, ConnectionId id) {
    logln_fmt(&server->arena, "New connection received, sending server new connection message [thread={}]", get_current_thread_id());
    NetworkMessage message = NetworkMessage {.type = CLIENT_CONNECTED, .client_connected = id};
    network_queue_push(&server->in_queue, bytes_from_ptr(&message));
    logln_fmt(&server->arena, "Queued CLIENT_CONNECTED message for connection {}", id);
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
