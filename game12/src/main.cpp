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

// Total: 19:30
// Started: 10:00

#define MAX_ENTITIES 1000

f32 PLAYER_ACCELERATION = 12;
f32 PLAYER_MAX_SPEED =  30;
f32 PLAYER_DRAG = 0.25;

#define LEVEL_INSTANCE_ID 0
#define SERVER_INSTANCE_ID 1

#define GAME_SERVER_MS_PER_TICK 16

enum ModelType : u32 {
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

struct RaycastIterator {
    Ray ray;
    f32 distance;
    v3 check_position;
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
enum GameClientMode {
    GC_EDITOR,
    GC_HOSTED,
    GC_CLIENT
};

struct GameClient {
    GameClientMode mode;
    struct {
        Entity *selected_entity; // may break stuff because pointer 
    } editor;
    State state;
};

// created in game_*_entry, use GS() and GC()
GameServer *g_game_server = NULL;
GameClient *g_game_client = NULL;

GameServer *GS();
GameClient *GC();

AtomicSnapshot<Sampler> server_events_snapshot;

void game_server_start();
void game_server_entry();
void game_client_entry();

void game_server_stop();

void poll_user_input(State *state);
void poll_network(State *state);
void process_events(State *state);
void sync_clients(State *state);

void update_entities(State *state, f32 delta_time);
void update_editor(State *state);
void draw(State *state);
void draw_ui(State *state);
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

void start_as_host();
void connect_as_client();

void load_level(State *state);

bool is_server(State *state);
bool is_client(State *state);
void server_on_new_connection(NetworkLayer *net, Server *server, ConnectionId id);

RaycastIterator raycast_iterator_create(Ray ray, f32 distance);
Entity *next(RaycastIterator *it, State *state);

void imgui_v3_control(const char *label, v3 *vector);
void imgui_v4_control(const char *label, v4 *vector);

void clear_level(State *state);
void serialise_level(State *state);
void serialise_entity(YAML::Emitter &out, Entity *entity);
void deserialise_level(State *state);

YAML::Emitter &operator<<(YAML::Emitter &out, v3 value);
YAML::Emitter &operator<<(YAML::Emitter &out, v4 value);

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

    game_client_entry();
    
    if (g_game_server != NULL) {
        game_server_stop();
        network_layer_stop_server(NET());
        network_layer_stop_client(NET());
    }
    else {
        network_layer_stop_client(NET());
    }

    network_layer_stop();
}

GameServer *GS() {
    ASSERT(g_game_server);
    return g_game_server;
}

GameClient *GC() {
    ASSERT(g_game_client);
    return g_game_client;
}

// @startserver
void game_server_start() {
    ASSERT(g_game_server == NULL);

    // my strategy for this is init everything in the instance
    // besides the state object before starting the new thread
    // then it is up to the server thread to init the state
    // and go from there
    atomic_snapshot_init(&server_events_snapshot);

    g_game_server = new GameServer {};
    g_game_server->shutdown_signal = false;
    g_game_server->thread = std::thread(game_server_entry); 
}

// @entrygs @gs
void game_server_entry() {
    log_set_thread_options(LogOptions {
        .thread_name = "SERVER",
        .thread_colour = YELLOW_ASCII_CODE,
    });

    GS()->state = State {
        .instance_type = IT_SERVER,
        .instance_id = SERVER_INSTANCE_ID,
        .arena = arena_create(10 * 1024 * 1024),
        .event_sampler = {},
        .events = std::queue<Event>(),
        .entities = stack_array_create<Entity, MAX_ENTITIES>(),
    };

    Timer tick_timer = timer_create_ms(GAME_SERVER_MS_PER_TICK);

    logln_fmt(&GS()->state.arena, "Started game server [thread={}]", get_current_thread_id());
    logln_fmt(&GS()->state.arena, "Server running at {}t/s", i64(1000.0f / f32(GAME_SERVER_MS_PER_TICK)));

    load_level(&GS()->state);

    while (!GS()->shutdown_signal) {
        f32 delta_time = 0;

        if (!timer_is_complete(&tick_timer, &delta_time)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // get any incoming events
        poll_network(&GS()->state);

        // process any events
        process_events(&GS()->state);

        // update local state 
        update_entities(&GS()->state, delta_time);
        physics(&GS()->state, delta_time);

        sync_clients(&GS()->state);

        { // update event sampler snapshot
            Sampler *s = atomic_snapshot_write(&server_events_snapshot);
            *s = GS()->state.event_sampler;
            atomic_snapshot_swap(&server_events_snapshot);
        }

        arena_reset(&GS()->state.arena);
    }

    logln("Game server was given shutdown signal.. stopping");
}

// @entrygc @gc
void game_client_entry() {
    g_game_client = new GameClient {
        .mode = GC_EDITOR,
        .state = State {
            .instance_type = IT_CLIENT,
            .instance_id = 0,
            .arena = arena_create(10 * 1024 * 1024),
            .event_sampler = {},
            .events = std::queue<Event>(),
            .entities = stack_array_create<Entity, MAX_ENTITIES>(),
        }
    };

    { // init all the global stuff
        bool ok = false;

        ok = window_init("Game12", 1920, 1080);
        if (!ok) {
            logln("Failed when trying to init the window");
        }

        ok = camera_init(CameraMode::FIRST_PERSON, 80, v3{0, 0, -3}, 0.1, 200);
        if (!ok) {
            logln("Failed when trying to init the camera");
        }

        ok = renderer_init(WIN(), rgb(97, 123, 219), v3{0.6, 0.6, 0.6}, v3{0.5, 0.5, 0.5}, v3{50, 100, -100}, v3{-1, -1, 0.5}, 0.8, 0.025, v2{480, 270});
        if (!ok) {
            logln("Failed when trying to init the renderer");
        }

        g_models[MT_CUBE] = load_model(REN(), "resources/models/cuber/cube.obj");
    }

    Timer tick_timer = timer_create_ms(GAME_SERVER_MS_PER_TICK);

    logln_fmt(&GC()->state.arena, "Started game client [thread={}]", get_current_thread_id());
    logln_fmt(&GC()->state.arena, "Client running at {}t/s", i64(1000.0f / f32(GAME_SERVER_MS_PER_TICK)));

    deserialise_level(&GC()->state);

    while (!glfwWindowShouldClose(WIN()->glfw_window)) {
        new_frame(REN(), WIN(), CAM());

        if (KEYS[GLFW_KEY_ESCAPE] == InputState::DOWN) {
            glfwSetWindowShouldClose(WIN()->glfw_window, GLFW_TRUE);
        }

        if (KEYS[GLFW_KEY_F1] == InputState::DOWN) {
            set_mouse_captured(WIN(), !WIN()->mouse_captured);
        }

        if (GC()->mode == GC_EDITOR) {
            poll_inputs();
            update_editor(&GC()->state);
        }
        else if (GC()->mode == GC_HOSTED || GC()->mode == GC_CLIENT) {
            f32 delta_time = 0;
            if (timer_is_complete(&tick_timer, &delta_time)) {
                // get any incoming events
                poll_user_input(&GC()->state);
                poll_network(&GC()->state);
     
                // process any events
                process_events(&GC()->state);
         
                // update local state 
                update_entities(&GC()->state, delta_time);
            }
        } else { ASSERT(0); }

        // draw
        draw(&GC()->state);
        draw_ui(&GC()->state);

        draw_frame(REN(), WIN());
        swap_buffers(WIN());
        arena_reset(&GC()->state.arena);
    }

    glfwTerminate();
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
        // entity is static and is created from the level
        // no need to sync with clients
        if (entity.owner == LEVEL_INSTANCE_ID) {
            continue;
        }

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
    }
}

bool start = false;
Ray stored_ray = {};

void update_editor(State *state) {
    ASSERT(is_client(state) && GC()->mode == GC_EDITOR);

    v3 m_start = screen_position_to_world_position(v3{MOUSE.position.x, MOUSE.position.y, -1}, REN(), WIN());
    v3 m_end = screen_position_to_world_position(v3{MOUSE.position.x, MOUSE.position.y, 1}, REN(), WIN());

    draw_model(REN(), g_models[MT_CUBE], m_start, v3{1, 1, 1} * 0.005f, {}, BLACK);
    draw_model(REN(), g_models[MT_CUBE], m_start, v3{1, 1, 1} * 0.002f, {}, RED);
    // draw_model(REN(), g_models[MT_CUBE], m_end, v3{1, 1, 1} * 4, {}, RED);

    if (MOUSE.buttons[GLFW_MOUSE_BUTTON_1] == InputState::DOWN) {
        Ray ray = ray_from_screen_position({MOUSE.position.x, MOUSE.position.y, -1});
        RaycastIterator it = raycast_iterator_create(ray, CAM()->far_plane - CAM()->near_plane);

        stored_ray = ray;
        start = true;

        while (false) {
            Entity *entity = next(&it, state);
            if (!entity) {
                break;
            }

            GC()->editor.selected_entity = entity; 
        }
    }

    if (start) {
        RaycastIterator it = raycast_iterator_create(stored_ray, CAM()->far_plane - CAM()->near_plane);

        while (true) {
            Entity *entity = next(&it, state);
            if (!entity) {
                break;
            }

            GC()->editor.selected_entity = entity; 
        }
    }



    // ctrl-N: new level
    if (KEYS[GLFW_KEY_LEFT_CONTROL] == InputState::PRESSED && 
        KEYS[GLFW_KEY_N] == InputState::DOWN) {
        clear_level(state);
    }

    // ctrl-S: save level
    if (KEYS[GLFW_KEY_LEFT_CONTROL] == InputState::PRESSED && 
        KEYS[GLFW_KEY_S] == InputState::DOWN) {
        serialise_level(state);
    }

    // ctrl-O: load level
    if (KEYS[GLFW_KEY_LEFT_CONTROL] == InputState::PRESSED && 
        KEYS[GLFW_KEY_O] == InputState::DOWN) {
        deserialise_level(state);
    }

    { // camera look
        bool free_look = WIN()->mouse_captured;
        f32 sensitivity = free_look ? 0.07 : 0.15;

        if (free_look || MOUSE.buttons[GLFW_MOUSE_BUTTON_2] == InputState::PRESSED) {
            v2 mouse_input = MOUSE.delta;
        
            if (length(mouse_input) > 0) {
                CAM()->rotation += v3{mouse_input.y, mouse_input.x, 0} * sensitivity;
                CAM()->rotation.x = clamp(-90, CAM()->rotation.x, 90);
            }
        }
    }

    { // camera movement
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
   
        v3 forward = get_forward_direction(CAM());
        v3 up = {0, 1, 0};
        v3 right = get_right_direction(CAM());
        v3 movement = v3{};

        movement += right * keyboard_input.x;
        movement += up * keyboard_input.y;
        movement += forward * keyboard_input.z;
        
        CAM()->position += movement;
    }
}

void draw(State *state) {
    ASSERT(is_client(state));

    for (Entity &entity : state->entities) {
        // don't draw the player if is owned by this client
        if (BIT_SET(entity.flags, EF_PLAYER) && entity.owner == state->instance_id) {
            continue;
        }

        v4 draw_colour = entity.colour; 

        if (GC()->mode == GC_EDITOR) {
            if (GC()->editor.selected_entity && GC()->editor.selected_entity->id == entity.id) {
                draw_colour = RED;
            }
        }
            
        draw_model(REN(), g_models[entity.model], entity.position, entity.size, entity.rotation, draw_colour);
    }
}

void draw_ui(State *state) {
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingOverCentralNode);
    // ImGui::ShowDemoWindow();

    ImGui::Begin("Player");

    ImGui::InputFloat("Acceleration", &PLAYER_ACCELERATION);
    ImGui::InputFloat("Max speed", &PLAYER_MAX_SPEED);
    ImGui::InputFloat("Drag", &PLAYER_DRAG);

    ImGui::End();

    ImGui::Begin("Network");

    if (ImGui::Button("Host")) {
        start_as_host();
    }

    ImGui::SameLine();

    if (ImGui::Button("Connect")) {
        connect_as_client();
    }

    ImGui::SeparatorText("Events");

    f32 event_in_MB = f32(sizeof(Event)) / (8.0f * 1024.0f);
 
    { // client events sampler info
        f32 average = sampler_average(&state->event_sampler);
        f32 samples_per_second = sampler_samples_per_second(&state->event_sampler);
        f32 events_per_second = average * samples_per_second;
        f32 MB_per_second = events_per_second * event_in_MB;
 
        ImGui::Text("Avg: %f", average);
        ImGui::Text("Samples/s: %f", samples_per_second);
        ImGui::Text("Events/s: %f", events_per_second);
        ImGui::Text("MB/s: %f", MB_per_second);
        ImGui::PlotLines("Client", state->event_sampler.samples, SAMPLER_SIZE, 0, NULL, FLT_MAX, FLT_MAX, ImVec2(0, 60));
    }
 
    if (g_game_server != NULL) { // client events sampler info
        Sampler *sampler = atomic_snapshot_read(&server_events_snapshot);
        f32 average = sampler_average(sampler);
        f32 samples_per_second = sampler_samples_per_second(sampler);
        f32 events_per_second = average * samples_per_second;
        f32 MB_per_second = events_per_second * event_in_MB;
 
        ImGui::Text("Avg: %f", average);
        ImGui::Text("Samples/s: %f", samples_per_second);
        ImGui::Text("Events/s: %f", events_per_second);
        ImGui::Text("MB/s: %f", MB_per_second);
        ImGui::PlotLines("Server", sampler->samples, SAMPLER_SIZE, 0, NULL, FLT_MAX, FLT_MAX, ImVec2(0, 60));
    }

    ImGui::End();

    ImGui::Begin("Editor");

    if (ImGui::Button("New")) {
        clear_level(state);
    }

    ImGui::SameLine();

    if (ImGui::Button("Save")) {
        serialise_level(state);
    }

    ImGui::SameLine();

    if (ImGui::Button("Load")) {
        deserialise_level(state);
    }

    if (ImGui::Button("Create empty")) {
        Entity entity = Entity {
            .id = new_entity_id(),
            .owner = LEVEL_INSTANCE_ID,
            .size = v3{1, 1, 1},
            .colour = WHITE,
            .model = MT_CUBE
        };

        local_spawn_entity(state, entity);
    }

    if (GC()->editor.selected_entity) {

        ImGui::SeparatorText("Selected Entity");

        Entity *entity = GC()->editor.selected_entity;
        ImGui::Text("flags: %llu", entity->flags);
        ImGui::Text("id: %u", entity->id);
        ImGui::Text("owner: %u", entity->owner);
        imgui_v3_control("position", &entity->position);
        imgui_v3_control("size", &entity->size);
        imgui_v3_control("rotation", &entity->rotation);
        imgui_v3_control("velocity", &entity->velocity);
        imgui_v4_control("colour", &entity->colour);
        ImGui::Text("model: %u", (u32) entity->model);

        //  bool ImGui::SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
    }

    ImGui::SeparatorText("Entities");

    for (i64 i = 0; i < state->entities.len; i++) {
        Entity *entity = &state->entities[i];

        ImGui::PushID(i);

        const char *format = "{}";
        if (GC()->editor.selected_entity == entity) {
            // TODO: fmt does not work when this is [{}] correct format
            // is written but the generated fmt_values are writing \0
            format = "-> {}";
        }

        const char *title = fmt(&state->arena, format, entity->id).c();

        if (ImGui::Button(title, ImVec2(200, 20))) {
            GC()->editor.selected_entity = entity;
        }

        ImGui::PopID();
    }
        
    ImGui::End();
}

void physics(State *state, f32 delta_time) {
    ASSERT(is_server(state));

    for (Entity &entity : state->entities) {
        // currently only simming physics for the player
        if (!BIT_SET(entity.flags, EF_PLAYER)) {
            continue; 
        }

        f32 speed = length(entity.velocity);

        if (speed < 0.01f) {
            entity.velocity = v3 {};
            continue;
        }

        if (speed > PLAYER_MAX_SPEED) {
            entity.velocity = norm(entity.velocity) * PLAYER_MAX_SPEED;
        }

        if (speed > 0) {
            v3 drag = -entity.velocity * PLAYER_DRAG;
            entity.velocity += drag;
        }

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
        delta_position.x >= -bounding_box.x && delta_position.x <= bounding_box.x &&
        delta_position.y >= -bounding_box.y && delta_position.y <= bounding_box.y &&
        delta_position.z >= -bounding_box.z && delta_position.z <= bounding_box.z
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
                    .size = v3{1, 1, 1},
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

            player->velocity += message->move_player * PLAYER_ACCELERATION;
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

void start_as_host() {
    logln("starting hosted game");

    REN()->clear_colour = {0.3, 0.3, 1, 1};

    GC()->mode = GC_HOSTED;

    game_server_start();
    network_layer_start_server(NET());
    network_layer_start_client(NET(), "::1");
}

void connect_as_client() {
    logln("starting and connecting to local-hosted game");

    GC()->mode = GC_CLIENT;

    REN()->clear_colour = {0.3, 1, 0.3, 1};
    network_layer_start_client(NET(), "::1");
}

void load_level(State *state) {
    for (i64 i = 0; i < 30; i++) {
        v3 position_offset = v3 {rand_f32_negative(), rand_f32_negative(), rand_f32_negative()};
        v4 colour = v4 {rand_f32(), rand_f32(), rand_f32(), 1};
            
        Entity entity = Entity {
            .flags = 0,
            .id = new_entity_id(),
            .owner = LEVEL_INSTANCE_ID,
            .position = v3{30, 30, 30} * position_offset,
            .size = {1, 1, 1},
            .colour = colour,
            .model = MT_CUBE 
        };

        local_spawn_entity(&g_game_server->state, entity);
    }
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

RaycastIterator raycast_iterator_create(Ray ray, f32 distance) {
    return RaycastIterator {
        .ray = ray,
        .distance = distance,
        .check_position = ray.origin
    };
}

Entity *next(RaycastIterator *it, State *state) {
    // how much to step along the ray
    // I have no idea what is good here
    const f32 STEP = 0.05f;
    v3 v_step = it->ray.direction * STEP;

    i64 step_count = 0;

    while (length(it->check_position - it->ray.origin) <= it->distance) {
        step_count++;
        it->check_position += v_step;

        draw_model(REN(), g_models[MT_CUBE], it->check_position, v3{STEP, STEP, STEP} * 0.2f, {}, RED);

        for (Entity &entity : state->entities) {
            bool hit = point_collision(it->check_position, entity.position, entity.size * 1.2f);
            if (hit) {
                return &entity;
            }
        }
    }

    return NULL;
}

void imgui_v3_control(const char *label, v3 *vector) {
    const f32 reset_value = 0;

    ImGui::PushID(label);
	ImGui::Columns(2);

	ImGui::SetColumnWidth(0, 100);

    ImGui::Text(label);

	ImGui::NextColumn();

    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());

    if (ImGui::Button("X")) {
        vector->x = reset_value;        
    }

    ImGui::SameLine();
    ImGui::DragFloat("##X", &(*vector)[0]);
	ImGui::PopItemWidth();
    ImGui::SameLine();

    if (ImGui::Button("Y")) {
        vector->y = reset_value;        
    }

    ImGui::SameLine();
    ImGui::DragFloat("##Y", &(*vector)[1]);
	ImGui::PopItemWidth();
    ImGui::SameLine();

    if (ImGui::Button("Z")) {
        vector->z = reset_value;        
    }

    ImGui::SameLine();
    ImGui::DragFloat("##Z", &(*vector)[2]);
	ImGui::PopItemWidth();
    ImGui::SameLine();
		
    ImGui::Columns(1);
	ImGui::PopID();
}

void imgui_v4_control(const char *label, v4 *vector) {
    const f32 reset_value = 0;

    ImGui::PushID(label);
	ImGui::Columns(2);

	ImGui::SetColumnWidth(0, 100);

    ImGui::Text(label);

	ImGui::NextColumn();

    ImGui::PushMultiItemsWidths(4, ImGui::CalcItemWidth());

    if (ImGui::Button("R")) {
        vector->x = reset_value;        
    }

    ImGui::SameLine();
    ImGui::DragFloat("##R", &(*vector)[0]);
	ImGui::PopItemWidth();
    ImGui::SameLine();

    if (ImGui::Button("G")) {
        vector->y = reset_value;        
    }

    ImGui::SameLine();
    ImGui::DragFloat("##G", &(*vector)[1]);
	ImGui::PopItemWidth();
    ImGui::SameLine();

    if (ImGui::Button("B")) {
        vector->z = reset_value;        
    }

    ImGui::SameLine();
    ImGui::DragFloat("##B", &(*vector)[2]);
	ImGui::PopItemWidth();
    ImGui::SameLine();

    if (ImGui::Button("A")) {
        vector->z = reset_value;        
    }

    ImGui::SameLine();
    ImGui::DragFloat("##A", &(*vector)[3]);
	ImGui::PopItemWidth();
    ImGui::SameLine();
		
    ImGui::Columns(1);
	ImGui::PopID();
}

void clear_level(State *state) {
    reset(&state->entities);
}

void serialise_level(State *state) {
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "name" << YAML::Value << "<empty>";
    out << YAML::Key << "entities" << YAML::Value << YAML::BeginSeq;

    for (Entity &entity : state->entities) {
        serialise_entity(out, &entity);
    }

    out << YAML::EndSeq;
    out << YAML::EndMap;

    // writing to the file
    File file = new_file("resources/levels/main.yaml");

    bool ok = create_file(&file);
    if (!ok) {
        logln("Failed to create file for saving level");
        return;
    }

    Slice<u8> bytes = slice_create((u8 *) out.c_str(), out.size());

    ok = write_file(&file, bytes);
    if (!ok) {
        logln("Failed to write data to file when saving level");
        return;
    }

    close_file(&file);

    logln("Level was saved");
}

void serialise_entity(YAML::Emitter &out, Entity *entity) {
    out << YAML::BeginMap;
    out << YAML::Key << "flags"     << YAML::Value << entity->flags;
    out << YAML::Key << "id"        << YAML::Value << entity->id;
    out << YAML::Key << "owner"     << YAML::Value << entity->owner;
    out << YAML::Key << "position"  << YAML::Value << entity->position;
    out << YAML::Key << "size"      << YAML::Value << entity->size;
    out << YAML::Key << "rotation"  << YAML::Value << entity->rotation;
    out << YAML::Key << "velocity"  << YAML::Value << entity->velocity;
    out << YAML::Key << "colour"    << YAML::Value << entity->colour;
    out << YAML::Key << "model"     << YAML::Value << entity->model;
    out << YAML::EndMap;
}

void deserialise_level(State *state) {
    YAML::Node root = YAML::LoadFile("resources/levels/main.yaml");

    YAML::Node entities = root["entities"];
    if (!entities) {
        logln("No entities field in level file");
        return;
    }

    reset(&state->entities);

    for (auto entity : entities) {
        Entity e = Entity {};

        e.flags = entity["flags"].as<u64>();
        e.id = entity["id"].as<u32>();
        e.owner = entity["owner"].as<u32>();
        e.position = entity["position"].as<v3>();
        e.size = entity["size"].as<v3>();
        e.rotation = entity["rotation"].as<v3>();
        e.velocity = entity["velocity"].as<v3>();
        e.colour = entity["colour"].as<v4>();
        e.model = (ModelType) entity["model"].as<u32>();

        append(&state->entities, e);
    }

    logln("Level was loaded");
}

YAML::Emitter &operator<<(YAML::Emitter &out, v3 vector) {
    out << YAML::Flow;
    out << YAML::BeginSeq << vector.x << vector.y << vector.z << YAML::EndSeq;
    return out;
}

YAML::Emitter &operator<<(YAML::Emitter &out, v4 vector) {
    out << YAML::Flow;
    out << YAML::BeginSeq << vector.x << vector.y << vector.z << vector.w << YAML::EndSeq;
    return out;
}

template<>
struct YAML::convert<v3> {
    static bool decode(const YAML::Node &node, v3 &vector) {
        if (!node.IsSequence() || node.size() != 3) {
            return false;
        }

        vector.x = node[0].as<f32>();
        vector.y = node[1].as<f32>();
        vector.z = node[2].as<f32>();

        return true;
    }
};

template<>
struct YAML::convert<v4> {
    static bool decode(const YAML::Node &node, v4 &vector) {
        if (!node.IsSequence() || node.size() != 4) {
            return false;
        }

        vector.x = node[0].as<f32>();
        vector.y = node[1].as<f32>();
        vector.z = node[2].as<f32>();
        vector.w = node[3].as<f32>();

        return true;
    }
};

// bool decode(const YAML::Node &node, v4 &vector) {
    // return false;
// }

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
