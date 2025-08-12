#include "libs/libs.h"
#include "ack.cpp"
#include "math.cpp"
#include "net.cpp"
#include "engine.cpp"
#include "platform.h"

#include <atomic>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <time.h>
#include <stdlib.h>
#include <chrono>
#include <queue>
#include <atomic>
#include <type_traits>
#include <utility>

// Total: 10:30
// Started: 18:00

#define MAX_ENTITIES 200
#define SERVER_ID 1

enum ModelType {
    MT_CUBE,
    _MT_COUNT
};

Model *g_models[_MT_COUNT];

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
    v3 rotation;
    v3 velocity;

    // rendering
    v4 colour;
    ModelType model;

    // bullet
    u32 bullet_origin;
};

enum NetworkMessageType {
    NM_ASSIGN_CLIENT_ID,
    NM_CLIENT_CONNECTED,
    NM_SPAWN_ENTITY,
    NM_SYNC_ENTITY,
    NM_DELETE_ENTITY,
    NM_MOVE_PLAYER,
}; 

struct NetworkMessage {
    ConnectionId client_id;
    NetworkMessageType type;
    
    union {
        u32 assign_client_id;
        ConnectionId client_connected;
        Entity spawn_entity;
        Entity sync_entity;
        u32 delete_entity;
        v3 move_player;
    };
};

enum EntityFlags {
    EF_PLAYER           = 1 << 0,
    EF_DELETE           = 1 << 16,
};

enum InstanceType {
    IT_CLIENT,
    IT_SERVER
};

enum EventType {
    EV_KEYBOARD_INPUT,
    EV_MOUSE_INPUT,
    EV_NETWORK_MESSAGE
};

struct Event {
    EventType type;

    union {
        v3 keyboard_input;
        v2 mouse_input;
        NetworkMessage network_message;
    };
};

// @state
struct State {
    InstanceType instance_type;
    u32 instance_id;
    
    f64 time;
    Arena arena;

    Sampler event_sampler;

    std::queue<Event> events;
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
    State state;
};

GameServer *g_game_server = NULL;
GameClient *g_game_client = NULL;

AtomicSnapshot<Sampler> server_events_snapshot;

void game_server_start();
void game_server_stop();

void game_client_start();

void poll_user_input(State *state);
void poll_network(State *state);

void process_events(State *state);

void sync_clients(State *state);

void update_entities(State *state, f32 delta_time);
void draw(State *state, f32 delta_time);
void draw_ui(State *state, f32 delta_time);
void physics(State *state, f32 delta_time);

void events_push(State *state, Event event);
bool events_pop(State *state, Event *out);

void on_server_receive(State *state, NetworkMessage *message);
void on_client_receive(State *state, NetworkMessage *message);

u32 new_entity_id();
Entity *local_spawn_entity(State *state, Entity entity);
void server_spawn_entity(Entity entity);
void local_delete_entity(State *state, u32 id);
void server_delete_entity(u32 id);
Entity *get_client_player(State *state, u32 client_id);
Entity *get_entity_with_id(State *state, u32 id);
Entity *get_entity_with_flag(State *state, EntityFlags flag);
bool entities_overlap(Entity *a, Entity *b);

bool is_server(State *state);
bool is_client(State *state);
void server_on_new_connection(NetworkLayer *net, Server *server, ConnectionId id);

// @main
int main(i32 argc, const char **argv) {
    log_set_thread_options(LogOptions {
        .thread_name = "CLIENT",
        .thread_colour = GREEN_ASCII_CODE,
    });

    srand(time(NULL));

    bool ok = network_layer_init();

    if (!ok) {
        logln("CRASH: failed to strart networking");
        return 1;
    }

    NET()->server.on_new_connection = server_on_new_connection;

    network_layer_start();

    game_client_start();

    network_layer_stop();
}

// @startserver
void game_server_start() {
    ASSERT(g_game_server == NULL);

    // my strategy for this is init everything in the instance
    // besides the state object before starting the new thread
    // then it is up to the server thread to init the state
    // and go from there

    g_game_server = new GameServer {};
    g_game_server->shutdown_signal = false;

    atomic_snapshot_init(&server_events_snapshot);

    g_game_server->thread = std::thread([] () {
    log_set_thread_options(LogOptions {
        .thread_name = "SERVER",
        .thread_colour = YELLOW_ASCII_CODE,
    });

    g_game_server->state = State {
        .instance_type = IT_SERVER,
        .instance_id = SERVER_ID,
        .time = 0,
        .arena = arena_create(10 * 1024 * 1024),
        .event_sampler = {},
        .events = std::queue<Event>(),
        .entities = stack_array_create<Entity, MAX_ENTITIES>(),
    };

    logln_fmt(&g_game_server->state.arena, "Started game server [thread={}]", get_current_thread_id());

    Timer tick_rate = timer_create_ms(16);


    { // generate random entities on startup
        for (i64 i = 0; i < 30; i++) {
            v3 position_offset = v3 {rand_f32_negative(), rand_f32_negative(), rand_f32_negative()};
            v4 colour = v4 {rand_f32(), rand_f32(), rand_f32(), 1};

            Entity entity = Entity {
                .flags = 0,
                .id = new_entity_id(),
                .owner = SERVER_ID,
                .position = v3{30, 30, 30} * position_offset,
                .size = {1, 1, 1},
                .colour = colour,
                .model = MT_CUBE 
            };

            local_spawn_entity(&g_game_server->state, entity);
        }
    }

    while (!g_game_server->shutdown_signal) {
        if (!timer_is_complete_reset(&tick_rate)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        f32 delta_time = 0.05;

        // The life of a frame on the game server:
        // - get any incoming events
        //      - poll input from user
        //      - poll network for messages
        // - process any events
        //      - send user input to network
        //      - process network messages
        // - update local state 
        //      - update entities
        //      - update physics 
        // - draw 

        // get any incoming events
        poll_network(&g_game_server->state);

        // process any events
        process_events(&g_game_server->state);

        // update local state 
        update_entities(&g_game_server->state, delta_time);
        physics(&g_game_server->state, delta_time);

        sync_clients(&g_game_server->state);

        // sync_clients(&g_game_server->state);

        { // update event sampler snapshot
            Sampler *s = atomic_snapshot_write(&server_events_snapshot);
            *s = g_game_server->state.event_sampler;
            atomic_snapshot_swap(&server_events_snapshot);
        }

        arena_reset(&g_game_server->state.arena);
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

// @startclient
void game_client_start() {
    GameClient game_client = {
        .state = State {
            .instance_type = IT_CLIENT,
            .instance_id = 0,
            .time = 0,
            .arena = arena_create(10 * 1024 * 1024),
            .event_sampler = {},
            .events = std::queue<Event>(),
            .entities = stack_array_create<Entity, MAX_ENTITIES>(),
        }
    };
    
    { // init all the global stuff
        bool ok = false;

        ok = window_init("Game12", 1280, 940);
        if (!ok) {
            logln("Failed when trying to init the window");
        }

        ok = camera_init(CameraMode::FIRST_PERSON, 100, v3{0, 0, -3}, 0.1, 200);
        if (!ok) {
            logln("Failed when trying to init the camera");
        }

        ok = renderer_init(WIN(), v4{1, 1, 1, 1}, v3{0.6, 0.6, 0.6}, v3{0.5, 0.5, 0.5}, v3{50, 100, -100}, v3{-1, -1, 0.5}, 0.8, 0.025, v2{480, 270});
        if (!ok) {
            logln("Failed when trying to init the renderer");
        }

        g_models[MT_CUBE] = load_model(REN(), "resources/models/cuber/cube.obj");
    }

    logln_fmt(&game_client.state.arena, "Started game client [thread={}]", get_current_thread_id());

    bool hosted = false;
    bool game_started = false;

    while (!glfwWindowShouldClose(WIN()->glfw_window)) {
        f64 current_time        = game_client.state.time;
        f64 new_time            = glfwGetTime();
        f32 delta_time          = (f32) (new_time - current_time);
        game_client.state.time    = new_time;

        if (KEYS[GLFW_KEY_ESCAPE] == InputState::DOWN) {
            glfwSetWindowShouldClose(WIN()->glfw_window, GLFW_TRUE);
        }

        // self host game server
        if (KEYS[GLFW_KEY_1] == InputState::DOWN && !game_started) {
            hosted = true;
            game_started = true;

            REN()->clear_colour = {0.3, 0.3, 1, 1};

            logln("starting hosted game");

            game_server_start();

            network_layer_start_server(NET());
            network_layer_start_client(NET(), "::1");
        }

        // join game server
        if (KEYS[GLFW_KEY_2] == InputState::DOWN && !game_started) {
            hosted = false;
            game_started = true;

            REN()->clear_colour = {0.3, 1, 0.3, 1};

            logln("starting and connecting to local-hosted game");

            network_layer_start_client(NET(), "::1");
        }

        if (KEYS[GLFW_KEY_F1] == InputState::DOWN) {
            set_mouse_captured(WIN(), !WIN()->mouse_captured);
        }

        // The life of a frame on the game client:
        // - get any incoming events
        //      - poll input from user
        //      - poll network for messages
        // - process any events
        //      - send user input to network
        //      - process network messages
        // - update local state 
        //      - update entities
        //      - update physics 
        // - draw 

        new_frame(REN(), WIN(), CAM());

        // get any incoming events
        poll_user_input(&game_client.state);
        poll_network(&game_client.state);

        // process any events
        process_events(&game_client.state);

        // update local state 
        update_entities(&game_client.state, delta_time);
        physics(&game_client.state, delta_time);

        // draw
        draw(&game_client.state, delta_time);
        draw_ui(&game_client.state, delta_time);

        draw_frame(REN(), WIN());
        swap_buffers(WIN());
        arena_reset(&game_client.state.arena);
    }

    if (hosted) {
        game_server_stop();
        network_layer_stop_server(NET());
        network_layer_stop_client(NET());
    }
    else {
        network_layer_stop_client(NET());
    }

    glfwTerminate();
}

void poll_user_input(State *state) {
    ASSERT(is_client(state)); // what is the server doing here?

    // actually get update inputs from glfw
    // wil change how all this works at some point
    // - 11/08/25
    poll_inputs();

    if (WIN()->mouse_captured) {
        v2 mouse_input = MOUSE.delta; 
    
        if (length(mouse_input) > 0) {
            events_push(state, Event {.type = EV_MOUSE_INPUT, .mouse_input = mouse_input});
        }
    }

    v3 keyboard_input = {};
 
    if (KEYS[GLFW_KEY_A] == InputState::PRESSED) {
        keyboard_input.x -= 1;
    }
         
    if (KEYS[GLFW_KEY_D] == InputState::PRESSED) {
        keyboard_input.x += 1;
    }
             
    if (KEYS[GLFW_KEY_SPACE] == InputState::PRESSED) {
        keyboard_input.y += 1;
    }
             
    if (KEYS[GLFW_KEY_LEFT_SHIFT] == InputState::PRESSED) {
        keyboard_input.y -= 1;
    }
         
    if (KEYS[GLFW_KEY_W] == InputState::PRESSED) {
        keyboard_input.z += 1;
    }
     
    if (KEYS[GLFW_KEY_S] == InputState::PRESSED) {
        keyboard_input.z -= 1;
    }

    if (length(keyboard_input) > 0) {
        events_push(state, Event {.type = EV_KEYBOARD_INPUT, .keyboard_input = keyboard_input});
    }
}

void poll_network(State *state) {
    NetworkQueue *in_queue = NULL;

    if (is_server(state)) {
        in_queue = &NET()->server_in_queue;
    }
    else if (is_client(state)) {
        in_queue = &NET()->client_in_queue;
    }

    ASSERT(in_queue);

    Slice<u8> bytes;
    while (network_queue_pop(in_queue, &bytes)) {
        NetworkMessage *message = (NetworkMessage *) bytes.ptr;
        events_push(state, Event {.type = EV_NETWORK_MESSAGE, .network_message = *message});
        slice_free(bytes);
    }
}

void process_events(State *state) {
    sampler_append(&state->event_sampler, f32(state->events.size()));

    Event event;

    if (is_client(state)) {
        while (events_pop(state, &event)) {
            switch (event.type) {
                case EV_KEYBOARD_INPUT: {
                    v3 forward = get_forward_direction(CAM());
                    v3 up = {0, 1, 0};
                    v3 right = get_right_direction(CAM());

                    v3 movement = v3{};
                    movement += right * event.keyboard_input.x;
                    movement += up * event.keyboard_input.y;
                    movement += forward * event.keyboard_input.z;
         
                    NetworkMessage message = NetworkMessage{.client_id = state->instance_id, .type = NM_MOVE_PLAYER, .move_player = movement};
                    client_send_to_server(NET(), bytes_from_ptr(&message));
                } break;
                case EV_MOUSE_INPUT: {
                    f32 sensitivity = 0.07;
        
                    CAM()->rotation += v3{event.mouse_input.y, event.mouse_input.x, 0} * sensitivity;
                    CAM()->rotation.x = clamp(-90, CAM()->rotation.x, 90);
                } break;
                case EV_NETWORK_MESSAGE: {
                    on_client_receive(state, &event.network_message);
                } break;
                default: ASSERT(0); break;
            } 
        }
    }

    if (is_server(state)) {
        while (events_pop(state, &event)) {
            switch (event.type) {
                case EV_NETWORK_MESSAGE: {
                    on_server_receive(state, &event.network_message);
                } break;
                default: ASSERT(0); break;
            } 
        }
    }
}

void sync_clients(State *state) {
    ASSERT(is_server(state));

    for (Entity &entity : state->entities) {
        NetworkMessage message = NetworkMessage{.type = NM_SYNC_ENTITY, .sync_entity = entity};
        server_send_to_all_clients(NET(), bytes_from_ptr(&message));
    }
}

void update_entities(State *state, f32 delta_time) {
    // nothing to do on the server for entities yet...
    if (is_server(state)) return;


    // update camera position to client player and rotate based on mouse input
    Entity *player = get_client_player(state, state->instance_id);
    if (player != NULL) {
        CAM()->position = player->position;
#if 0
        v3 looking_at = get_forward_direction(CAM());
        looking_at = norm(v3{looking_at.x, 0, looking_at.z});
        draw_model(REN(), g_models[MT_CUBE], player->position + looking_at * 3, {1, 1, 1}, {}, WHITE);
#endif
    }

#if 0
    for (Entity &entity : state->entities) {
        if (BIT_SET(entity.flags, EF_PLAYER)) {
            { // apply acceleration from input adjust so it is relative to forward direction
                f32 move_speed = 5;
                v3 forward = get_forward_direction(entity.rotation);
                v3 up = {0, 1, 0};
                v3 right = get_right_direction(entity.rotation);
     
                entity.position += right * (movement_input.x * move_speed * delta_time);
                entity.position += up * (movement_input.y * move_speed * delta_time);
                entity.position += forward * (movement_input.z * move_speed * delta_time);
            }
            
            // cap velocity 
            f32 max_velocity = 20;
            if (length(entity.velocity) > max_velocity) {
                entity.velocity = norm(entity.velocity) * max_velocity;
            }

            // make velocity decay over time 
            f32 valocity_decay = 80;
            if (length(entity.velocity) > 0) {
                entity.velocity += -norm(entity.velocity) * max_velocity * delta_time;
            }
        }
    }
#endif
}

void draw(State *state, f32 delta_time) {
    ASSERT(is_client(state));

    for (Entity &entity : state->entities) {
        // don't draw the player if is owned by this client
        if (BIT_SET(entity.flags, EF_PLAYER) && entity.owner == state->instance_id) {
            continue;
        }

        draw_model(REN(), g_models[entity.model], entity.position, entity.size, entity.rotation, entity.colour);
    }
}

void draw_ui(State *state, f32 delta_time) {
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingOverCentralNode);
    // ImGui::ShowDemoWindow();

    ImGui::Begin("Debug info");

    f32 event_in_MB = f32(sizeof(Event)) / (8.0f * 1024.0f);

    { // client events sampler info
        f32 average = sampler_average(&state->event_sampler);
        f32 samples_per_second = sampler_samples_per_second(&state->event_sampler);
        f32 events_per_second = average * samples_per_second;
        f32 MB_per_second = events_per_second * event_in_MB;

        ImGui::Text("Avg: %f, Samples/s: %f, Events/s: %f, MB/s: %f", average, samples_per_second, events_per_second, MB_per_second);
        ImGui::PlotLines("Client events", state->event_sampler.samples, SAMPLER_SIZE, 0, NULL, FLT_MAX, FLT_MAX, ImVec2(0, 60));
    }

    if (g_game_server != NULL) { // client events sampler info
        Sampler *sampler = atomic_snapshot_read(&server_events_snapshot);
        f32 average = sampler_average(sampler);
        f32 samples_per_second = sampler_samples_per_second(sampler);
        f32 events_per_second = average * samples_per_second;
        f32 MB_per_second = events_per_second * event_in_MB;

        ImGui::Text("Avg: %f, Samples/s: %f, Events/s: %f, MB/s: %f", average, samples_per_second, events_per_second, MB_per_second);
        ImGui::PlotLines("Server events", sampler->samples, SAMPLER_SIZE, 0, NULL, FLT_MAX, FLT_MAX, ImVec2(0, 60));
    }

    // static float arr[] = { 0.6f, 0.1f, 1.0f, 0.5f, 0.92f, 0.1f, 0.2f };
    // ImGui::PlotLines("Frame Times", arr, IM_ARRAYSIZE(arr));

    ImGui::End();
}

void physics(State *state, f32 delta_time) {
    for (Entity &entity : state->entities) {
        entity.position += entity.velocity * delta_time;
    }
}

void events_push(State *state, Event event) {
    state->events.push(event);
}

bool events_pop(State *state, Event *out) {
    if (state->events.empty()) {
        return false;
    }

    *out = state->events.front();
    state->events.pop();

    return true;
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

void on_server_receive(State *state, NetworkMessage *message) {
    switch (message->type) {
        case NM_CLIENT_CONNECTED: {
            static f32 player_count = 0;

            // when client connects, the server generates this message and a few things are required to happen
            // 1. The client is assigned an id from the server
            // 2. Any existing entities are sent to the new client to spawn
            // 3. The player entity is spawn on all clients and is owned by the new client
            // - 09/08/25
            ConnectionId connection_id = message->client_connected;
            logln_fmt(&state->arena, "Processing new client connection: connection_id={}", connection_id);
            
            { // assign client id
                logln_fmt(&state->arena, "Assigning new client: id={}", connection_id);

                NetworkMessage message = NetworkMessage{.type = NM_ASSIGN_CLIENT_ID, .assign_client_id = connection_id};
                server_send_to_client(NET(), bytes_from_ptr(&message), connection_id);
            }

            { // spawn any entities on new client
                logln_fmt(&state->arena, "Spawning {} existing entities on new client", state->entities.len);

                for (Entity &entity : state->entities) {
                    NetworkMessage message = NetworkMessage{.type = NM_SPAWN_ENTITY, .spawn_entity = entity};
                    server_send_to_client(NET(), bytes_from_ptr(&message), connection_id);
                }
            }

            { // spawn new player entity on all clients

                Entity new_player = Entity {
                    .flags = EF_PLAYER,
                    .id = new_entity_id(),
                    .owner = connection_id,
                    .position = v3{0, 0, 3} * player_count,
                    .size = {1, 1, 1},
                    .colour = v4{1, 0, 0, 1},
                    .model = MT_CUBE 
                };

                logln_fmt(&state->arena, "Spawning new player entity: entity_id={}, owner={} x={} y={} z={}", new_player.id, new_player.owner, new_player.position.x, new_player.position.y, new_player.position.z);
                local_spawn_entity(state, new_player);

                NetworkMessage message = NetworkMessage{.type = NM_SPAWN_ENTITY, .spawn_entity = new_player};
                server_send_to_all_clients(NET(), bytes_from_ptr(&message));
            }

            player_count += 1;
        } break;
        case NM_SPAWN_ENTITY: {
            Entity entity = message->spawn_entity;
            entity.id = new_entity_id();

            logln_fmt(&state->arena, "Server spawning entity: id={}, owner={}", entity.id, entity.owner);
            local_spawn_entity(state, entity);
            NetworkMessage message = NetworkMessage{.type = NM_SPAWN_ENTITY, .spawn_entity = entity};
            server_send_to_all_clients(NET(), bytes_from_ptr(&message));
        } break;
        case NM_SYNC_ENTITY: {
            Entity *entity = get_entity_with_id(state, message->sync_entity.id);
            if (entity != NULL && entity->owner != SERVER_ID) {
                *entity = message->sync_entity;

                NetworkMessage message = NetworkMessage{.type = NM_SYNC_ENTITY, .sync_entity = *entity};
                server_send_to_all_clients(NET(), bytes_from_ptr(&message), entity->owner);
            }
        } break;
        case NM_DELETE_ENTITY: {
            u32 id = message->delete_entity;

            logln_fmt(&state->arena, "Server deleting entity: id={}", id);
            local_delete_entity(state, id);
            NetworkMessage message = NetworkMessage{.type = NM_DELETE_ENTITY, .delete_entity = id};
            server_send_to_all_clients(NET(), bytes_from_ptr(&message));
        } break;
        case NM_MOVE_PLAYER: {
            Entity *player = get_client_player(state, message->client_id);
            if (!player) {
                return;
            }

            player->position += message->move_player * 0.5;
        } break;
        default: {
            logln("WARNING unknown message sent");
        } break;
    }
}

void on_client_receive(State *state, NetworkMessage *message) {
    switch (message->type) {
        case NM_ASSIGN_CLIENT_ID: {
            state->instance_id = message->assign_client_id;
            logln_fmt(&state->arena, "Client assigned id={}", state->instance_id);
        } break;
        case NM_SPAWN_ENTITY: {
            logln_fmt(&state->arena, "Client spawning entity: id={}, owner={}", message->spawn_entity.id, message->spawn_entity.owner);
            local_spawn_entity(state, message->spawn_entity);
        } break;
        case NM_SYNC_ENTITY: {
            Entity *entity = get_entity_with_id(state, message->sync_entity.id);
            if (entity != NULL) {
                *entity = message->sync_entity;
            }
        } break;
        case NM_DELETE_ENTITY: {
            logln_fmt(&state->arena, "Client deleting entity: id={}", message->delete_entity);
            local_delete_entity(state, message->delete_entity);
        } break;
        default: {
            logln("WARNING unknown message sent");
        } break;
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
    NetworkMessage message = NetworkMessage{.type = NM_SPAWN_ENTITY, .spawn_entity = entity};
    client_send_to_server(NET(), bytes_from_ptr(&message));
}

void server_delete_entity(u32 id) {
    NetworkMessage message = NetworkMessage{.type = NM_DELETE_ENTITY, .delete_entity = id};
    client_send_to_server(NET(), bytes_from_ptr(&message));
}

Entity *get_client_player(State *state, u32 client_id) {
    for (Entity &entity : state->entities) {
        if (BIT_SET(entity.flags, EF_PLAYER) && entity.owner == client_id) {
            return &entity;
        }
    }

    return NULL;
}

Entity *get_entity_with_id(State *state, u32 id) {
    for (Entity &entity : state->entities) {
        if (entity.id == id) {
            return &entity;
        }
    }

    return NULL;
}

Entity *get_entity_with_flag(State *state, EntityFlags flag) {
    for (Entity &entity : state->entities) {
        if (BIT_SET(entity.flags, flag)) {
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

    NetworkMessage message = NetworkMessage {.type = NM_CLIENT_CONNECTED, .client_connected = id};
    network_queue_push(&net->server_in_queue, bytes_from_ptr(&message));
}


template<>                                                                                      
void fmt_value(DynamicArray<u8> *bytes, v3 value) {
    append_many(bytes, Slice<u8>("v3 {"));
    fmt_value(bytes, value.x);
    append_many(bytes, Slice<u8>(", "));
    fmt_value(bytes, value.y);
    append_many(bytes, Slice<u8>(", "));
    fmt_value(bytes, value.z);
    append_many(bytes, Slice<u8>("}"));
}
