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

// Total: 43:30
// Started: 17:30
//
//
// What do a programmer do?:
// - ui text rendering combined with rendering on top of scene 
// - combine vs and fs in the one file
// - define assets in editor and get handles in game

#define MAX_ENTITIES 500

f32 PLAYER_MOVE_ACCELERATION = 12;
f32 PLAYER_JUMP_ACCELERATION = 25;
f32 PLAYER_MAX_SPEED = 20;
f32 PLAYER_DRAG = 0.25;
f32 GRAVITY = 2.1;
v3 WEAPON_DISPLAY_OFFSET = v3{1.18, -0.67, 1.1};
f32 WEAPON_DISPLAY_SIZE = 0.37;

#define LEVEL_INSTANCE_ID 0
#define SERVER_INSTANCE_ID 1

#define GAME_SERVER_MS_PER_TICK 16

enum ModelType : u32 {
    MT_CUBE,
    _MT_COUNT
};

struct Weapon {
    f32 damage;
    v4 colour;
};

enum WeaponType : u32 {
    WT_NONE,
    WT_DEAGLE,
    WT_M4,
    _WT_COUNT
};

Weapon g_weapons[_WT_COUNT] = {
    {},
    Weapon {
        .damage = 30,
        .colour = v4 {0.8, 0.8, 0.8, 1},
    },
    Weapon {
        .damage = 15,
        .colour = v4 {0.2, 0.2, 0.2, 1},
    }
};

// @entity
enum EntityFlag : u32 {
    EF_PLAYER           = 1 << 0,
    EF_SPAWN_POINT      = 1 << 1,
    EF_SOLID_HITBOX     = 1 << 2,
    EF_STATIC_HITBOX    = 1 << 3,
    EF_HAS_HEALTH       = 1 << 4,
    EF_HAS_WEAPON       = 1 << 5,
    EF_DELETE           = 1 << 16,
};

template <>
struct magic_enum::customize::enum_range<EntityFlag> {
    static constexpr bool is_flags = true;
};

struct Entity {
    // meta
    u32 flags;
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

    // flag: has_health
    f32 max_health;
    f32 health;

    // flag: has_weapon
    WeaponType weapon;
};

enum NetworkMessageType {
    NM_ASSIGN_CLIENT_ID,
    NM_CLIENT_CONNECTED,
    NM_SPAWN_ENTITY,
    NM_SYNC_ENTITY,
    NM_DELETE_ENTITY,
    NM_MOVE_PLAYER,
    NM_PLAYER_HIT,
    NM_SPAWN_TARGET,
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
        u32 player_hit;
        v3 spawn_target;
    };
};

struct RaycastIterator {
    Ray ray;
    f32 distance;
    v3 check_position;
};

struct CubeCollision {
    bool collision;
    v3 overlap;
    v3 distance;
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
    Sampler network_in_sampler;

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

// @editor
struct Editor {
    Camera camera;

    Viewport viewport;
    FrameBuffer g_buffer;
    FrameBuffer lighting_buffer;
    FrameBuffer ui_buffer;

    Entity *selected_entity;
};

struct GameClient {
    GameClientMode mode;

    Camera camera;
    Viewport viewport;
    FrameBuffer g_buffer;
    FrameBuffer lighting_buffer;
    FrameBuffer ui_buffer;

    State state;
};

GameServer *g_game_server = NULL;
GameClient *g_game_client = NULL;
Editor     *g_editor      = NULL;

GameServer *GS();
GameClient *GC();
Editor *ED();

AtomicSnapshot<Sampler> server_messages_snapshot;

void game_server_start();
void game_server_entry();
void game_client_entry();

void game_server_stop();

void poll_user_input(State *state);
void process_network(State *state);
void sync_clients(State *state);

void game_server_update(State *state, f32 delta_time);
void game_server_physics(State *state, f32 delta_time);

void game_client_update(State *state, f32 delta_time);
void game_client_draw(State *state);

void editor_update(State *state);
void editor_draw_ui(State *state);

void on_server_receive(State *state, NetworkMessage *message);
void on_client_receive(State *state, NetworkMessage *message);

u32 new_entity_id();
Entity *local_spawn_entity(State *state, Entity entity);
void server_spawn_entity(Entity entity);
Entity *get_client_player(State *state, u32 client_id);
Entity *get_entity_with_id(State *state, u32 id);
Entity *get_entity_with_flag(State *state, EntityFlag flag);
bool entities_overlap(Entity *a, Entity *b);

Entity *local_spawn_empty(State *state);
Entity *local_spawn_player(State *state);
Entity *local_spawn_spawn_point(State *state);
Entity *local_spawn_static_box(State *state);
Entity *local_spawn_has_health(State *state);

void start_as_host();
void connect_as_client();
void stop_game();

bool is_server(State *state);
bool is_client(State *state);
void server_on_new_connection(NetworkLayer *net, Server *server, ConnectionId id);

RaycastIterator raycast_iterator_create(Ray ray, f32 distance);
Entity *next(RaycastIterator *it, State *state);

CubeCollision cube_collision(v3 a_position, v3 a_size, v3 b_position, v3 b_size);

Viewport imgui_viewport(const char *label, u32 texture_id, bool force_focus);
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
        log("CRASH: failed to strart networking");
        return 1;
    }

    NET()->server.on_new_connection = server_on_new_connection;

    network_layer_start();

    game_client_entry();
    stop_game();
    
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

Editor *ED() {
    ASSERT(g_editor);
    return g_editor;
}

// @startserver
void game_server_start() {
    ASSERT(g_game_server == NULL);

    // my strategy for this is init everything in the instance
    // besides the state object before starting the new thread
    // then it is up to the server thread to init the state
    // and go from there
    atomic_snapshot_init(&server_messages_snapshot);

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
        .network_in_sampler = {},
        .entities = stack_array_create<Entity, MAX_ENTITIES>(),
    };

    Timer tick_timer = timer_create_ms(GAME_SERVER_MS_PER_TICK);

    logf("Started game server [thread={}]", get_current_thread_id());
    logf("Server running at {}t/s", i64(1000.0f / f32(GAME_SERVER_MS_PER_TICK)));

    deserialise_level(&GS()->state);

    while (!GS()->shutdown_signal) {
        f32 delta_time = 0;

        if (!timer_is_complete(&tick_timer, &delta_time)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        process_network(&GS()->state);
        game_server_update(&GS()->state, delta_time);
        game_server_physics(&GS()->state, delta_time);
        sync_clients(&GS()->state);

        { // update event sampler snapshot
            Sampler *s = atomic_snapshot_write(&server_messages_snapshot);
            *s = GS()->state.network_in_sampler;
            atomic_snapshot_swap(&server_messages_snapshot);
        }

        arena_reset(&GS()->state.arena);
    }

    log("Game server was given shutdown signal.. stopping");
}

// @entrygc @gc
void game_client_entry() {
    { // init all the global stuff
        bool ok = false;

        ok = window_init("Game12", 1280, 720);
        ASSERT(ok);

        if (!ok) {
            log("Failed when trying to init the window");
            return;
        }

        ok = renderer_init(WIN(), v4{1, 1, 1, 1} * 0.8, v3{0.1, 0.1, 0.1}, v3{0.5, 0.5, 0.5}, v3{50, 100, -100}, v3{-1, -1, 0.5});
        ASSERT(ok);

        if (!ok) {
            log("Failed when trying to init the renderer");
            return;
        }
    }

    g_game_client = new GameClient {
        .mode = GC_EDITOR,
        .camera = camera_create(CameraMode::FIRST_PERSON, 90, v3{0, 0, 0}, 0.1, 300),
        .viewport =  Viewport {
            .focused = false,
            .size = WIN()->frame_buffer_size
        },
        .g_buffer = FrameBuffer {.size = WIN()->frame_buffer_size},
        .lighting_buffer = FrameBuffer {.size = WIN()->frame_buffer_size},
        .ui_buffer = FrameBuffer {.size = WIN()->frame_buffer_size},
        .state = State {
            .instance_type = IT_CLIENT,
            .instance_id = 0,
            .arena = arena_create(10 * 1024 * 1024),
            .network_in_sampler = {},
            .entities = stack_array_create<Entity, MAX_ENTITIES>(),
        }
    };

    g_editor = new Editor {
        .camera = camera_create(CameraMode::FIRST_PERSON, 90, v3{0, 5, -20}, 0.1, 300),
        .viewport =  Viewport {
            .focused = false,
            .size = WIN()->frame_buffer_size
        },
        .g_buffer = FrameBuffer {.size = WIN()->frame_buffer_size},
        .lighting_buffer = FrameBuffer {.size = WIN()->frame_buffer_size},
        .ui_buffer = FrameBuffer {.size = WIN()->frame_buffer_size},
        .selected_entity = NULL,
    };

    { // init editor and client frame buffer
        bool ok = frame_buffer_init(&g_game_client->g_buffer);
        if (!ok) {
            log("failed to init game client gBuffer");
            return;
        }

        ok = frame_buffer_init(&g_game_client->lighting_buffer);
        if (!ok) {
            log("failed to init game lighting buffer");
            return;
        }

        ok = frame_buffer_init(&g_game_client->ui_buffer);
        if (!ok) {
            log("failed to init game ui buffer");
            return;
        }

        ok = frame_buffer_init(&g_editor->g_buffer);
        if (!ok) {
            log("failed to init editor gBuffer");
            return;
        }

        ok = frame_buffer_init(&g_editor->lighting_buffer);
        if (!ok) {
            log("failed to init editor lighting buffer");
            return;
        }

        ok = frame_buffer_init(&g_editor->ui_buffer);
        if (!ok) {
            log("failed to init editor ui buffer");
            return;
        }
    }

    Timer tick_timer = timer_create_ms(GAME_SERVER_MS_PER_TICK);

    logf("Started game client [thread={}]", get_current_thread_id());
    logf("Client running at {}t/s", i64(1000.0f / f32(GAME_SERVER_MS_PER_TICK)));

    deserialise_level(&GC()->state);

    while (!glfwWindowShouldClose(WIN()->glfw_window)) {
        f32 delta_time = 0;
        if (timer_is_complete(&tick_timer, &delta_time)) {
            poll_inputs();

            if (KEYS[GLFW_KEY_ESCAPE] == InputState::DOWN) {
                glfwSetWindowShouldClose(WIN()->glfw_window, GLFW_TRUE);
            }

            if (KEYS[GLFW_KEY_F1] == InputState::DOWN) {
                set_mouse_captured(WIN(), !WIN()->mouse_captured);
            }

            if (ED()->viewport.focused) {
                editor_update(&GC()->state);
            }
        
            if (GC()->mode == GC_HOSTED || GC()->mode == GC_CLIENT) {
                process_network(&GC()->state);
                game_client_update(&GC()->state, delta_time);
            }
        }

        // draw
        renderer_start_frame(REN());

        game_client_draw(&GC()->state);

        // renderer_draw_geometry(REN(), &GC()->camera, GC()->viewport, &GC()->g_buffer);
        // renderer_draw_geometry(REN(), &ED()->camera, ED()->viewport, &ED()->g_buffer);

        // renderer_draw_lighting(REN(), &GC()->camera, GC()->viewport, &GC()->lighting_buffer, &GC()->g_buffer);
        // renderer_draw_lighting(REN(), &ED()->camera, ED()->viewport, &ED()->lighting_buffer, &ED()->g_buffer);

        draw_text_ui(REN(), "Ammo 05/10", {10, 10, 0}, 50, RED);
        renderer_draw_ui(REN(), &GC()->camera, GC()->viewport, &GC()->ui_buffer, &GC()->lighting_buffer);

        renderer_end_frame(REN());

        editor_draw_ui(&GC()->state);

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

}

void process_network(State *state) {
    if (is_client(state)) {
        sampler_append(&state->network_in_sampler, f32(network_queue_size(&NET()->client_in_queue)));

        Slice<u8> bytes;
        while (network_queue_pop(&NET()->client_in_queue, &bytes)) {
            NetworkMessage *message = (NetworkMessage *) bytes.ptr;
            on_client_receive(state, message);
            slice_free(bytes);
        }
    }

    if (is_server(state)) {
        sampler_append(&state->network_in_sampler, f32(network_queue_size(&NET()->server_in_queue)));

        Slice<u8> bytes;
        while (network_queue_pop(&NET()->server_in_queue, &bytes)) {
            NetworkMessage *message = (NetworkMessage *) bytes.ptr;
            on_server_receive(state, message);
            slice_free(bytes);
        }
    }
}

void sync_clients(State *state) {
    ASSERT(is_server(state));

    i64 index = 0;
    while (index < state->entities.len) {
        Entity &entity = state->entities[index];

        // entity is static and is created from the level
        // no need to sync with clients
        if (entity.owner == LEVEL_INSTANCE_ID) {
            index++;
            continue;
        }

        if (BIT_SET(state->entities[index].flags, EF_DELETE)) {
            NetworkMessage message = NetworkMessage{.type = NM_DELETE_ENTITY, .delete_entity = entity.id};
            server_send_to_all_clients(NET(), bytes_from_ptr(&message));

            swap_remove(&state->entities, index);
            continue;
        }

        NetworkMessage message = NetworkMessage{.type = NM_SYNC_ENTITY, .sync_entity = entity};
        server_send_to_all_clients(NET(), bytes_from_ptr(&message));

        index++;
    }
}

void game_server_update(State *state, f32 delta_time) {
    ASSERT(is_server(state));

    for (Entity &entity : state->entities) {
        if (BIT_SET(entity.flags, EF_HAS_HEALTH)) {
            if (entity.health <= 0) {
                SET_BIT(entity.flags, EF_DELETE);
            }
        }
    }
}

void game_server_physics(State *state, f32 delta_time) {
    ASSERT(is_server(state));

    for (Entity &entity : state->entities) {
        // currently only simming physics for the player
        if (!BIT_SET(entity.flags, EF_PLAYER)) {
            continue; 
        }

        v3 h_velocity = v3{entity.velocity.x, 0, entity.velocity.z};
        f32 h_speed = length(h_velocity);

        //  cap velocity
        if (length(h_velocity) > PLAYER_MAX_SPEED) {
            v3 max_h_velocity = norm(h_velocity) * PLAYER_MAX_SPEED;

            entity.velocity.x = max_h_velocity.x;
            entity.velocity.z = max_h_velocity.z;
        }

        //  apply drag to velocity
        if (h_speed > 0) {
            v3 drag = -h_velocity * PLAYER_DRAG;
            entity.velocity += drag;
        }

        entity.velocity += v3{0, -GRAVITY, 0};
        entity.position += entity.velocity * delta_time;

        for (Entity &other : state->entities) {
            if (&entity == &other) {
                continue;
            }

            if (!BIT_SET(other.flags, EF_STATIC_HITBOX)) {
                continue;
            }

            auto [collided, overlap, distance] = cube_collision(entity.position, entity.size, other.position, other.size);
            if (!collided) {
                continue;
            }

            if (overlap.x < overlap.y && overlap.x < overlap.z) {
                entity.position.x -= sign(distance.x) * overlap.x;
                entity.velocity.x = 0;
            } 
            else if (overlap.y < overlap.x && overlap.y < overlap.z) {
                entity.position.y -= sign(distance.y) * overlap.y;
                entity.velocity.y = 0;
            } 
            else if (overlap.z < overlap.x && overlap.z < overlap.y) {
                entity.position.z -= sign(distance.z) * overlap.z;
                entity.velocity.z = 0;
            }
        }
    }
}

void game_client_update(State *state, f32 delta_time) {
    Entity *player = get_client_player(state, state->instance_id);
    if (player != NULL) {
        GC()->camera.position = player->position + v3{0, 0.7, 0};
    }

    // check player input
    if (GC()->viewport.focused) {
        if (KEYS[GLFW_KEY_T] == InputState::DOWN) {
            NetworkMessage message = NetworkMessage{.client_id = state->instance_id, .type = NM_SPAWN_TARGET, .spawn_target = {0, 3, 0}};
            client_send_to_server(NET(), bytes_from_ptr(&message));
        }

        if (WIN()->mouse_captured) {
            v2 mouse_input = MOUSE.delta;
    
            if (length(mouse_input) > 0) {
                f32 sensitivity = 0.09;
    
                GC()->camera.rotation += v3{mouse_input.y, mouse_input.x, 0} * sensitivity;
                GC()->camera.rotation.x = clamp(-90, GC()->camera.rotation.x, 90);
            }

            // shooting
            if (MOUSE.buttons[GLFW_MOUSE_BUTTON_1] == InputState::DOWN) { 
                Ray ray = ray_create(GC()->camera.position, get_forward_direction(&GC()->camera));
                draw_line(REN(), ray.origin, ray.direction, 0.08, 0.5, GREEN);

                RaycastIterator it = raycast_iterator_create(ray, GC()->camera.far_plane - GC()->camera.near_plane);

                Entity *hit_entity = NULL;
                while (true) {
                    hit_entity = next(&it, state);
                    if (!hit_entity) {
                        break;
                    }

                    if (BIT_SET(hit_entity->flags, EF_PLAYER) && hit_entity->owner == state->instance_id) {
                        continue; // the player shot temselves
                    }

                    if (BIT_SET(hit_entity->flags, EF_HAS_HEALTH)) {
                        break;
                    }
                }

                if (hit_entity) {
                    NetworkMessage message = NetworkMessage{.client_id = state->instance_id, .type = NM_PLAYER_HIT, .player_hit = hit_entity->id};
                    client_send_to_server(NET(), bytes_from_ptr(&message));
                }
            }
        }

        v3 keyboard_input = {};
     
        if (KEYS[GLFW_KEY_A] == InputState::PRESSED) {
            keyboard_input.x -= 1;
        }
             
        if (KEYS[GLFW_KEY_D] == InputState::PRESSED) {
            keyboard_input.x += 1;
        }
                 
        if (KEYS[GLFW_KEY_SPACE] == InputState::DOWN) {
            keyboard_input.y += 1;
        }
                 
        if (KEYS[GLFW_KEY_W] == InputState::PRESSED) {
            keyboard_input.z += 1;
        }
         
        if (KEYS[GLFW_KEY_S] == InputState::PRESSED) {
            keyboard_input.z -= 1;
        }

        if (length(keyboard_input) > 0) {
            v3 forward = get_forward_direction(&GC()->camera);
            v3 up = {0, 1, 0};
            v3 right = get_right_direction(&GC()->camera);
    
            forward.y = 0;
            forward = norm(forward);
    
            right.y = 0;
            right = norm(right);
        
            v3 movement = v3{};
            movement += right * keyboard_input.x;
            movement += up * keyboard_input.y;
            movement += forward * keyboard_input.z;
                 
            NetworkMessage message = NetworkMessage{.client_id = state->instance_id, .type = NM_MOVE_PLAYER, .move_player = movement};
            client_send_to_server(NET(), bytes_from_ptr(&message));
        }
    }
}

void game_client_draw(State *state) {
    ASSERT(is_client(state));

    draw_quad(REN(), {0, 5, 0}, {2, 2}, {}, RED);

    for (Entity &entity : state->entities) {
        v4 draw_colour = entity.colour;

        // client's player
        if (BIT_SET(entity.flags, EF_PLAYER) && entity.owner == state->instance_id) {

            v3 forward = get_forward_direction(&GC()->camera);
            v3 up = get_up_direction(&GC()->camera);
            v3 right = get_right_direction(&GC()->camera);
       
            // draw crosshair
            draw_sphere(REN(), GC()->camera.position + forward, 0.005, BLACK);
       
            // draw weapon
            if (BIT_SET(entity.flags, EF_HAS_WEAPON)) {
                v3 weapon_offset = v3{};
                weapon_offset += WEAPON_DISPLAY_OFFSET.x * right;
                weapon_offset += WEAPON_DISPLAY_OFFSET.y * up;
                weapon_offset += WEAPON_DISPLAY_OFFSET.z * forward;
       
                draw_sphere(REN(), GC()->camera.position + weapon_offset, WEAPON_DISPLAY_SIZE, g_weapons[entity.weapon].colour);
            }
        }

        // other players
        if (BIT_SET(entity.flags, EF_HAS_HEALTH)) {
            // draw_rectangle(REN(), entity.position + v3{0, 2, 0}, {1, 1}, BLACK);
        }

        if (ED()->selected_entity && ED()->selected_entity->id == entity.id) {
            draw_colour = RED;
        }

        draw_cube(REN(), entity.position, entity.size, entity.rotation, draw_colour);
    }
}

void editor_update(State *state) {
    ASSERT(is_client(state));

    Camera *camera = &ED()->camera;

    // mouse picking
    if (MOUSE.buttons[GLFW_MOUSE_BUTTON_1] == InputState::DOWN) {
        // TODO: there is still a problem with this not being exact...
        Ray ray = ray_from_screen_position(ED()->viewport, {ED()->viewport.mouse.x, ED()->viewport.mouse.y, -1});
        RaycastIterator it = raycast_iterator_create(ray, camera->far_plane - camera->near_plane);

        Entity *entity = next(&it, state);
        if (entity) {
            ED()->selected_entity = entity; 
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
                camera->rotation += v3{mouse_input.y, mouse_input.x, 0} * sensitivity;
                camera->rotation.x = clamp(-90, camera->rotation.x, 90);
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
   
        v3 forward = get_forward_direction(camera);
        v3 up = {0, 1, 0};
        v3 right = get_right_direction(camera);
        v3 movement = v3{};

        movement += right * keyboard_input.x;
        movement += up * keyboard_input.y;
        movement += forward * keyboard_input.z;
        
        camera->position += movement;
    }
}

void editor_draw_ui(State *state) {
    new_imgui_frame();

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), 0);

    // https://github.com/ocornut/imgui/blob/master/imgui_demo.cpp
    // ImGui::ShowDemoWindow();
    
    {
        i32 image_downscale = 4;
        Viewport *gcv = &GC()->viewport;
        Viewport *edv = &GC()->viewport;

        ImGui::Begin("Render output");

        if (ImGui::CollapsingHeader("Game g_buffer")) {
            ImGui::Text("Position  //  Normals");
            ImGui::Image(GC()->g_buffer.position_attachment, ImVec2(gcv->size.x / image_downscale, gcv->size.y / image_downscale), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
            ImGui::Image(GC()->g_buffer.normals_attachment, ImVec2(gcv->size.x / image_downscale, gcv->size.y / image_downscale), ImVec2(0, 1), ImVec2(1, 0));

            ImGui::Text("Albedo  //  Depth");
            ImGui::Image(GC()->g_buffer.albedo_attachment, ImVec2(gcv->size.x / image_downscale, gcv->size.y / image_downscale), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
            ImGui::Image(GC()->g_buffer.depth_attachment, ImVec2(gcv->size.x / image_downscale, gcv->size.y / image_downscale), ImVec2(0, 1), ImVec2(1, 0));
        }

        if (ImGui::CollapsingHeader("Game lighting")) {
            ImGui::Image(GC()->lighting_buffer.position_attachment, ImVec2(gcv->size.x / image_downscale, gcv->size.y / image_downscale), ImVec2(0, 1), ImVec2(1, 0));
        }

        if (ImGui::CollapsingHeader("Game ui")) {
            ImGui::Text("Albedo");
            ImGui::Image(GC()->ui_buffer.albedo_attachment, ImVec2(gcv->size.x / image_downscale, gcv->size.y / image_downscale), ImVec2(0, 1), ImVec2(1, 0));
        }

        if (ImGui::CollapsingHeader("Editor g_buffer")) {
            ImGui::Text("Position  //  Normals");
            ImGui::Image(ED()->g_buffer.position_attachment, ImVec2(edv->size.x / image_downscale, edv->size.y / image_downscale), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
            ImGui::Image(ED()->g_buffer.normals_attachment, ImVec2(edv->size.x / image_downscale, edv->size.y / image_downscale), ImVec2(0, 1), ImVec2(1, 0));

            ImGui::Text("Albedo  //  Depth");
            ImGui::Image(ED()->g_buffer.albedo_attachment, ImVec2(edv->size.x / image_downscale, edv->size.y / image_downscale), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
            ImGui::Image(ED()->g_buffer.depth_attachment, ImVec2(edv->size.x / image_downscale, edv->size.y / image_downscale), ImVec2(0, 1), ImVec2(1, 0));
        }

        if (ImGui::CollapsingHeader("Editor lighting")) {
            ImGui::Image(ED()->lighting_buffer.position_attachment, ImVec2(edv->size.x / image_downscale, edv->size.y / image_downscale), ImVec2(0, 1), ImVec2(1, 0));
        }

        if (ImGui::CollapsingHeader("Editor ui")) {
            ImGui::Text("Albedo");
            ImGui::Image(ED()->ui_buffer.albedo_attachment, ImVec2(edv->size.x / image_downscale, edv->size.y / image_downscale), ImVec2(0, 1), ImVec2(1, 0));
        }

        ImGui::End();
    }
    
    {
        ImGui::Begin("Debug info");

        { // main display info
            ImGui::SeparatorText("Display");
            ImGui::Text("Logical size: %dx%d", WIN()->logical_size.x, WIN()->logical_size.y);
            ImGui::Text("Frame buffer size: %dx%d", WIN()->frame_buffer_size.x, WIN()->frame_buffer_size.y);
    
            v3 mouse_position = v3{MOUSE.position.x, MOUSE.position.y, -1};
            v3 mouse_position_ndc = screen_position_to_ndc(Viewport {.size = WIN()->frame_buffer_size}, mouse_position);
    
            ImGui::Text("Mouse: [%4.0f, %4.0f]", mouse_position.x, mouse_position.y);
            ImGui::Text("Mouse (NDC): [%4.2f, %4.2f]", mouse_position_ndc.x, mouse_position_ndc.y);
        }

        { // scene viewport info
            ImGui::SeparatorText("Scene viewport");
            ImGui::Text("Size: %dx%d", ED()->viewport.size.x, ED()->viewport.size.y);
    
            v3 mouse_position = v3{ED()->viewport.mouse.x, ED()->viewport.mouse.y, -1};
            v3 mouse_position_ndc = screen_position_to_ndc(ED()->viewport, mouse_position);
    
            ImGui::Text("Mouse: [%4.0f, %4.0f]", mouse_position.x, mouse_position.y);
            ImGui::Text("Mouse (NDC): [%4.2f, %4.2f]", mouse_position_ndc.x, mouse_position_ndc.y);
        }

        { // game viewport info
            ImGui::SeparatorText("Game viewport");
            ImGui::Text("Size: %dx%d", GC()->viewport.size.x, GC()->viewport.size.y);
    
            v3 mouse_position = v3{GC()->viewport.mouse.x, GC()->viewport.mouse.y, -1};
            v3 mouse_position_ndc = screen_position_to_ndc(GC()->viewport, mouse_position);
    
            ImGui::Text("Mouse: [%4.0f, %4.0f]", mouse_position.x, mouse_position.y);
            ImGui::Text("Mouse (NDC): [%4.2f, %4.2f]", mouse_position_ndc.x, mouse_position_ndc.y);
        }

        { // game camera
            ImGui::SeparatorText("Game camera");
            imgui_v3_control("position", &GC()->camera.position);
            imgui_v3_control("rotation", &GC()->camera.rotation);

            v3 forward = get_forward_direction(&GC()->camera);
            v3 right = get_right_direction(&GC()->camera);
            v3 up = get_up_direction(&GC()->camera);

            ImGui::Text("Forward: [%.3f, %.3f, %.3f]", forward.x, forward.y, forward.z);
            ImGui::Text("Right: [%.3f, %.3f, %.3f]", right.x, right.y, right.z);
            ImGui::Text("Up: [%.3f, %.3f, %.3f]", up.x, up.y, up.z);
        }

        { // editor camera
            ImGui::SeparatorText("Editor camera");
            imgui_v3_control("position", &ED()->camera.position);
            imgui_v3_control("rotation", &ED()->camera.rotation);

            v3 forward = get_forward_direction(&ED()->camera);
            v3 right = get_right_direction(&ED()->camera);
            v3 up = get_up_direction(&ED()->camera);

            ImGui::Text("Forward: [%.3f, %.3f, %.3f]", forward.x, forward.y, forward.z);
            ImGui::Text("Right: [%.3f, %.3f, %.3f]", right.x, right.y, right.z);
            ImGui::Text("Up: [%.3f, %.3f, %.3f]", up.x, up.y, up.z);
        }

        ImGui::End();
    }

    {
        ImGui::Begin("Network");
    
        if (ImGui::Button("Host")) {
            clear_level(state);
            start_as_host();
        }

        ImGui::SameLine();
    
        if (ImGui::Button("Connect")) {
            clear_level(state);
            connect_as_client();
        }

        if (GC()->mode != GC_EDITOR) {
            ImGui::SameLine();
        
            if (ImGui::Button("Stop game")) {
                stop_game();
                clear_level(state);
                deserialise_level(state);
            }
        }
    
        ImGui::SeparatorText("Network messages");

        f32 message_in_MB = f32(sizeof(NetworkMessage)) / (8.0f * 1024.0f);
     
        { // client events sampler info
            f32 average = sampler_average(&state->network_in_sampler);
            f32 samples_per_second = sampler_samples_per_second(&state->network_in_sampler);
            f32 events_per_second = average * samples_per_second;
            f32 MB_per_second = events_per_second * message_in_MB;
     
            ImGui::Text("Avg: %f", average);
            ImGui::Text("Messages/s: %f", samples_per_second);
            ImGui::Text("Events/s: %f", events_per_second);
            ImGui::Text("MB/s: %f", MB_per_second);
            ImGui::PlotLines("Client", state->network_in_sampler.samples, SAMPLER_SIZE, 0, NULL, FLT_MAX, FLT_MAX, ImVec2(0, 60));
        }
     
        if (g_game_server != NULL) { // client events sampler info
            Sampler *sampler = atomic_snapshot_read(&server_messages_snapshot);
            f32 average = sampler_average(sampler);
            f32 samples_per_second = sampler_samples_per_second(sampler);
            f32 events_per_second = average * samples_per_second;
            f32 MB_per_second = events_per_second * message_in_MB;
     
            ImGui::Text("Avg: %f", average);
            ImGui::Text("Samples/s: %f", samples_per_second);
            ImGui::Text("Messages/s: %f", events_per_second);
            ImGui::Text("MB/s: %f", MB_per_second);
            ImGui::PlotLines("Server", sampler->samples, SAMPLER_SIZE, 0, NULL, FLT_MAX, FLT_MAX, ImVec2(0, 60));
        }

        ImGui::End();
    }

    {
        ImGui::Begin("Tools");
    
        ImGui::SeparatorText("Level");
    
        if (ImGui::Button("New")) {
            ED()->selected_entity = NULL;
            clear_level(state);
        }
    
        ImGui::SameLine();
    
        if (ImGui::Button("Save")) {
            serialise_level(state);
        }
    
        ImGui::SameLine();
    
        if (ImGui::Button("Load")) {
            ED()->selected_entity = NULL;
            deserialise_level(state);
        }

        ImGui::SeparatorText("Player");
        ImGui::InputFloat("Move acceleration", &PLAYER_MOVE_ACCELERATION);
        ImGui::InputFloat("Jump acceleration", &PLAYER_JUMP_ACCELERATION);
        ImGui::InputFloat("Max speed", &PLAYER_MAX_SPEED);
        ImGui::InputFloat("Drag", &PLAYER_DRAG);
        ImGui::InputFloat("Gravity", &GRAVITY);
        ImGui::SliderFloat3("Weapon offset", &WEAPON_DISPLAY_OFFSET.x, -2, 2);
        ImGui::SliderFloat("Weapon size", &WEAPON_DISPLAY_SIZE, -1, 1);
    
        ImGui::SeparatorText("Spawn Entities");

        if (ImGui::Button("New empty")) {
            ED()->selected_entity = local_spawn_empty(state);
        }

        ImGui::SameLine();

        if (ImGui::Button("New spawn point")) {
            ED()->selected_entity = local_spawn_spawn_point(state);
        }

        if (ImGui::Button("New static box")) {
            ED()->selected_entity = local_spawn_static_box(state);
        }

        ImGui::SameLine();

        if (ImGui::Button("New has health")) {
            ED()->selected_entity = local_spawn_has_health(state);
        }
    
        if (ED()->selected_entity) {
    
            ImGui::SeparatorText("Selected Entity");
    
            Entity *entity = ED()->selected_entity;
            ImGui::Text("flags: %llu", entity->flags);
            ImGui::Text("id: %u", entity->id);
            ImGui::Text("owner: %u", entity->owner);
            imgui_v3_control("position", &entity->position);
            imgui_v3_control("size", &entity->size);
            imgui_v3_control("rotation", &entity->rotation);
            imgui_v3_control("velocity", &entity->velocity);
            ImGui::Text("model: %u", (u32) entity->model);
            ImGui::Text("colour: ");
            ImGui::SameLine();
            ImGui::ColorEdit4("##colour", &entity->colour[0], ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::InputFloat("max health", &entity->max_health);
            ImGui::InputFloat("health", &entity->health);
            ImGui::Text("weapon: %u", (u32) entity->weapon);
        }

        ImGui::SeparatorText("Entities in level");
    
        for (i64 i = 0; i < state->entities.len; i++) {
            Entity *entity = &state->entities[i];
    
            ImGui::PushID(i);
    
            bool is_selected = ED()->selected_entity == entity;
            const char *format = is_selected ? "-> {}" : "{}";
            const char *label = fmt(&state->arena, format, entity->id).c();
    
            if (ImGui::Button(label, ImVec2(200, 20))) {
                ED()->selected_entity = entity;
            }
    
            ImGui::SameLine();
            if (ImGui::Button("O")) {
                ED()->camera.position = entity->position;
            }
    
            if (is_selected) {
                ImGui::SameLine();
                if (ImGui::Button("X")) {
                    ED()->selected_entity = NULL;
                }
            }
    
            ImGui::PopID();
        }
        
        ImGui::End();
    }

    GC()->viewport = imgui_viewport("Game", GC()->ui_buffer.albedo_attachment, WIN()->mouse_captured);
    ED()->viewport = imgui_viewport("Editor", ED()->ui_buffer.albedo_attachment, false);

    draw_imgui_frame();
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
            // when client connects, the server generates this message and a few things are required to happen
            // 1. The client is assigned an id from the server
            // 2. Any existing entities are sent to the new client to spawn
            // 3. The player entity is spawn on all clients and is owned by the new client
            // - 09/08/25
            ConnectionId connection_id = message->client_connected;
            logf("Processing new client connection: connection_id={}", connection_id);
            
            { // assign client id
                logf( "Assigning new client: id={}", connection_id);

                NetworkMessage message = NetworkMessage{.type = NM_ASSIGN_CLIENT_ID, .assign_client_id = connection_id};
                server_send_to_client(NET(), bytes_from_ptr(&message), connection_id);
            }

            { // spawn any entities on new client
                logf( "Spawning {} existing entities on new client", state->entities.len);

                for (Entity &entity : state->entities) {
                    NetworkMessage message = NetworkMessage{.type = NM_SPAWN_ENTITY, .spawn_entity = entity};
                    server_send_to_client(NET(), bytes_from_ptr(&message), connection_id);
                }
            }

            { // spawn new player entity on all clients
                v3 spawn_position = {};

                Entity *spawn_point = get_entity_with_flag(state, EF_SPAWN_POINT);
                if (spawn_point) {
                    spawn_position = spawn_point->position;
                }

                Entity *new_player = local_spawn_player(state);
                new_player->position = spawn_position;
                new_player->owner = connection_id;

                logf( "Spawning new player entity: entity_id={}, owner={} position={}", new_player->id, new_player->owner, new_player->position);

                NetworkMessage message = NetworkMessage{.type = NM_SPAWN_ENTITY, .spawn_entity = *new_player};
                server_send_to_all_clients(NET(), bytes_from_ptr(&message));
            }
        } break;
        case NM_SPAWN_ENTITY: {
            Entity entity = message->spawn_entity;
            entity.id = new_entity_id();

            logf("Server spawning entity: id={}, owner={}", entity.id, entity.owner);
            local_spawn_entity(state, entity);
            NetworkMessage message = NetworkMessage{.type = NM_SPAWN_ENTITY, .spawn_entity = entity};
            server_send_to_all_clients(NET(), bytes_from_ptr(&message));
        } break;
        case NM_MOVE_PLAYER: {
            Entity *player = get_client_player(state, message->client_id);
            if (!player) {
                return;
            }

            player->velocity.x += message->move_player.x * PLAYER_MOVE_ACCELERATION;
            player->velocity.z += message->move_player.z * PLAYER_MOVE_ACCELERATION;

            player->velocity.y += message->move_player.y * PLAYER_JUMP_ACCELERATION;
        } break;
        case NM_PLAYER_HIT: {
            Entity *entity = get_entity_with_id(state, message->player_hit);
            if (entity && BIT_SET(entity->flags, EF_HAS_HEALTH)) {
                entity->health -= 10;
            }
        } break;
        case NM_SPAWN_TARGET: {
            Entity *target = local_spawn_has_health(state);
            target->owner = SERVER_INSTANCE_ID;
            target->position = message->spawn_target;

            NetworkMessage message = NetworkMessage{.type = NM_SPAWN_ENTITY, .spawn_entity = *target};
            server_send_to_all_clients(NET(), bytes_from_ptr(&message));
        } break;
        default: {
            log("WARNING unknown message sent");
        } break;
    }
}

void on_client_receive(State *state, NetworkMessage *message) {
    switch (message->type) {
        case NM_ASSIGN_CLIENT_ID: {
            state->instance_id = message->assign_client_id;
            logf( "Client assigned id={}", state->instance_id);
        } break;
        case NM_SPAWN_ENTITY: {
            logf( "Client spawning entity: id={}, owner={}", message->spawn_entity.id, message->spawn_entity.owner);
            local_spawn_entity(state, message->spawn_entity);
        } break;
        case NM_SYNC_ENTITY: {
            Entity *entity = get_entity_with_id(state, message->sync_entity.id);
            if (entity != NULL) {
                *entity = message->sync_entity;
            }
        } break;
        case NM_DELETE_ENTITY: {
            logf("Client deleting entity: id={}", message->delete_entity);

            for (i64 i = 0; i < state->entities.len; i++) {
                Entity *entity = &state->entities[i];
        
                if (entity->id == message->delete_entity) {
                    swap_remove(&state->entities, i);
                    return;
                }
            }
        } break;
        default: {
            log("WARNING unknown message sent");
        } break;
    }
}

u32 new_entity_id() {
    return u32(rand_i64());
}

Entity *local_spawn_entity(State *state, Entity entity) {
    Entity *ptr = push(&state->entities);
    *ptr = entity;

    return ptr;
}

void server_spawn_entity(Entity entity) {
    NetworkMessage message = NetworkMessage{.type = NM_SPAWN_ENTITY, .spawn_entity = entity};
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

Entity *get_entity_with_flag(State *state, EntityFlag flag) {
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

Entity *local_spawn_empty(State *state) {
    Entity entity = Entity {
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = v3{1, 1, 1},
        .colour = v4{1, 1, 1, 1},
        .model = MT_CUBE 
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_player(State *state) {
    Entity entity = Entity {
        .flags = EF_PLAYER | EF_SOLID_HITBOX | EF_HAS_HEALTH | EF_HAS_WEAPON,
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = v3{1, 2, 1},
        .colour = v4{1, 0, 0, 1},
        .model = MT_CUBE,
        .max_health = 100,
        .health = 100,
        .weapon = WT_DEAGLE
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_spawn_point(State *state) {
    Entity entity = Entity {
        .flags = EF_SPAWN_POINT,
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = v3{1, 1, 1} * 0.3,
        .colour = HOT_PINK,
        .model = MT_CUBE
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_static_box(State *state) {
    Entity entity = Entity {
        .flags = EF_STATIC_HITBOX,
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = v3{1, 1, 1},
        .colour = v4{1, 1, 1, 1},
        .model = MT_CUBE 
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_has_health(State *state) {
    Entity entity = Entity {
        .flags = EF_HAS_HEALTH,
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = v3{2, 2, 2},
        .colour = GREEN,
        .model = MT_CUBE,
        .max_health = 100,
        .health = 100,
    };

    return local_spawn_entity(state, entity);
}

void start_as_host() {
    log("starting hosted game");

    GC()->mode = GC_HOSTED;

    game_server_start();
    network_layer_start_server(NET());
    network_layer_start_client(NET(), "::1");
}

void connect_as_client() {
    log("starting and connecting to local-hosted game");

    GC()->mode = GC_CLIENT;

    network_layer_start_client(NET(), "::1");
}

void stop_game() {
    if (GC()->mode == GC_HOSTED) {
        game_server_stop();
        network_layer_stop_server(NET());
        network_layer_stop_client(NET());
    }
    else if (GC()->mode == GC_CLIENT) {
        network_layer_stop_client(NET());
    }

    GC()->mode = GC_EDITOR;
}

bool is_server(State *state) {
    return state->instance_type == IT_SERVER;
}

bool is_client(State *state) {
    return state->instance_type == IT_CLIENT;
}

void server_on_new_connection(NetworkLayer *net, Server *server, ConnectionId id) {
    logf("New connection received, sending server new connection message [thread={}]", get_current_thread_id());

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

    while (length(it->check_position - it->ray.origin) <= it->distance) {
        it->check_position += v_step;

        for (Entity &entity : state->entities) {
            bool hit = point_collision(it->check_position, entity.position, entity.size);
            if (hit) {
                return &entity;
            }
        }
    }

    return NULL;
}

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

Viewport imgui_viewport(const char *label, u32 texture_id, bool force_focus) {
    // https://www.youtube.com/watch?v=Qbt-1rcSqZc&list=PLlrATfBNZ98dC-V-N3m0Go4deliWHPFwT&index=72&ab_channel=TheCherno
    // Mainly adapted from this video ^
    // He uses a differant size which is from the min and max bounds
    // for the panel but for me just using the region avail worked better
    // - 14/08/25

    Viewport viewport = {};

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin(label);

    // after ImGui::Begin is called the tab bar has been created and now the current
    // cursor should be the top left of the viewport, cursor in this context is
    // the location imgui is going to draw UI next. Not the mouse
    // - 14/08/25
    //
    // get viewport size
    ImVec2 tab_bar_offset = ImGui::GetCursorPos();
    ImVec2 im_viewport_size = ImGui::GetContentRegionAvail();
    ImGui::Image(texture_id, im_viewport_size, ImVec2(0, 1), ImVec2(1, 0));

    viewport.size = v2i {i32(im_viewport_size.x), i32(im_viewport_size.y)};

    // get viewport mouse position
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 min_bound = ImGui::GetWindowPos();
    min_bound.x += tab_bar_offset.x;
    min_bound.y += tab_bar_offset.y;
    
    ImVec2 max_bound = ImVec2{min_bound.x + window_size.x, min_bound.y + window_size.y};
  
    // other way to get the size
    // v2 viewport_size_alt = v2{max_bound.x, max_bound.y} - v2{min_bound.x, min_bound.y};
    // viewport.size_alt = v2i{i32(viewport_size_alt.x), i32(viewport_size_alt.y)};
    
    auto[mouse_x, mouse_y] = ImGui::GetMousePos();
    mouse_x -= min_bound.x;
    mouse_y -= min_bound.y;

    // convert to bottom left origin from top right origin
    viewport.mouse = v2{mouse_x, (-mouse_y) + im_viewport_size.y};

    // get viewport focused 
    if (force_focus) {
        ImGui::SetWindowFocus();
    }

    viewport.focused = ImGui::IsWindowFocused();

    ImGui::End();
    ImGui::PopStyleVar();

    return viewport;
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
        log("Failed to create file for saving level");
        return;
    }

    Slice<u8> bytes = slice_create((u8 *) out.c_str(), out.size());

    ok = write_file(&file, bytes);
    if (!ok) {
        log("Failed to write data to file when saving level");
        return;
    }

    close_file(&file);

    log("Level was saved");
}

void serialise_entity(YAML::Emitter &out, Entity *entity) {
    out << YAML::BeginMap;
    out << YAML::Key << "flags"         << YAML::Value << entity->flags;
    out << YAML::Key << "id"            << YAML::Value << entity->id;
    out << YAML::Key << "owner"         << YAML::Value << entity->owner;
    out << YAML::Key << "position"      << YAML::Value << entity->position;
    out << YAML::Key << "size"          << YAML::Value << entity->size;
    out << YAML::Key << "rotation"      << YAML::Value << entity->rotation;
    out << YAML::Key << "velocity"      << YAML::Value << entity->velocity;
    out << YAML::Key << "colour"        << YAML::Value << entity->colour;
    out << YAML::Key << "model"         << YAML::Value << entity->model;
    out << YAML::Key << "max_health"    << YAML::Value << entity->max_health;
    out << YAML::Key << "health"        << YAML::Value << entity->health;
    out << YAML::Key << "weapon"        << YAML::Value << entity->weapon;
    out << YAML::EndMap;
}

void deserialise_level(State *state) {
    YAML::Node root = YAML::LoadFile("resources/levels/main.yaml");

    YAML::Node entities = root["entities"];
    if (!entities) {
        log("No entities field in level file");
        return;
    }

    reset(&state->entities);

    for (auto entity : entities) {
        Entity e = Entity {};

        e.flags =               entity["flags"].as<u32>();
        e.id =                  entity["id"].as<u32>();
        e.owner =               entity["owner"].as<u32>();
        e.position =            entity["position"].as<v3>();
        e.size =                entity["size"].as<v3>();
        e.rotation =            entity["rotation"].as<v3>();
        e.velocity =            entity["velocity"].as<v3>();
        e.colour =              entity["colour"].as<v4>();
        e.model = (ModelType)   entity["model"].as<u32>();
        e.max_health =          entity["max_health"].as<f32>();
        e.health =              entity["health"].as<f32>();
        e.weapon = WT_NONE;
        // e.weapon = (WeaponType) entity["weapon"].as<u32>();

        append(&state->entities, e);
    }

    log("Level was loaded");
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

template<>
void fmt_value(DynamicArray<u8> *bytes, v2i value) {
    append_many(bytes, Slice<u8>("v2i {"));
    fmt_value(bytes, value.x);
    append_many(bytes, Slice<u8>(", "));
    fmt_value(bytes, value.y);
    append_many(bytes, Slice<u8>("}"));
}

template<>
void fmt_value(DynamicArray<u8> *bytes, v2 value) {
    append_many(bytes, Slice<u8>("v2 {"));
    fmt_value(bytes, value.x);
    append_many(bytes, Slice<u8>(", "));
    fmt_value(bytes, value.y);
    append_many(bytes, Slice<u8>("}"));
}

template<>
void fmt_value(DynamicArray<u8> *bytes, v3i value) {
    append_many(bytes, Slice<u8>("v3i {"));
    fmt_value(bytes, value.x);
    append_many(bytes, Slice<u8>(", "));
    fmt_value(bytes, value.y);
    append_many(bytes, Slice<u8>(", "));
    fmt_value(bytes, value.z);
    append_many(bytes, Slice<u8>("}"));
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
