#include "imgui.h"
#include "libs/libs.h"
#include "ack.cpp"
#include "math.cpp"
#include "net.cpp"
#include "engine.cpp"
#include "platform.h"
#include "meta.h"

#include <atomic>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <time.h>
#include <stdlib.h>
#include <chrono>
#include <queue>
#include <atomic>
#include <iostream>

// Total: 87:30
// Started: 15:00
//
// What do a programmer do?:
// Game:
// - get movement feeling really good
//      - jump: longer press higher jump 
//      - sound: running, jumping, landing
// - ammo pickup
//	ammo: restore some amount of ammor for a gun (full for deagle, half for m4, 3 for sniper)
// - actually make a real map
// - TAP model
// - TAP sounds & explosion sounds
// - scoreboard
// - player sounds, running jumping, maybe taking damage?
//
// Editor:
// - setting to allow entity to be positioned so it is flush with the face of another 
//
// Engine:
// - combine vs and fs in the one file
// - fond out why camera yaw (Y) is flipped
// - switch to quaternions for rotation 
// - sky box
// - define assets in editor and get handles in game
// - load game as dll
//
// Meta program:
// - reduce the templating in the code gen
// - meta_name<EntityFlag>(flag) -> MetaEntityFlag::name(flag)
//
// Ack:
//  - asserts: enable/disable, assert with message, differnt actions to do on an assert
//  - basic type, include math types?
//  - strings, arrays, arenas
//  - formating
//  - Timers and profiling markers

#define MAX_ENTITIES 500
#define LEVEL_INSTANCE_ID 0
#define SERVER_INSTANCE_ID 1
#define GAME_SERVER_MS_PER_TICK 16
#define GAME_CLIENT_MS_PER_TICK 16

#define RUN_TESTS 0

f32 PLAYER_HEIGHT               = 2;
f32 PLAYER_WIDTH                = 0.65;
f32 PLAYER_EYES_OFFSET          = 0.8;

f32 PLAYER_DEATH_COOLDOWN       = 3;
f32 PLAYER_GROUND_ACCELERATION  = 4;
f32 PLAYER_JUMP_ACCELERATION    = 20;
f32 PLAYER_GROUND_DRAG          = 10;
f32 PLAYER_AIR_CONTROL          = 4;
f32 GRAVITY                     = 70;

v3 WEAPON_DISPLAY_OFFSET    = v3{1, -0.6, 0.98};
f32 WEAPON_SWITCH_COOLDOWN  = 1.5;

f32 WEAPON_PICKUP_COOLDOWN  = 9;
f32 HEALTH_PICKUP_COOLDOWN  = 6;

f32 CROSSHAIR_GAP           = 10;
f32 CROSSHAIR_LENGTH        = 12;
f32 CROSSHAIR_THICKNESS     = 3;
v4 CROSSHAIR_COLOUR         = RED;

f32 MISSLE_SPEED        = 35;
f32 EXPLOSION_RADIUS    = 12;
f32 EXPLOSION_FORCE     = 60;
f32 EXPLOSION_DAMAGE    = 200;

f32 JUMP_PAD_COOLDOWN = 1;
f32 JUMP_PAD_ACCELERATION = 55;

bool DEBUG_DRAW_OWNER       = false;

bool CHEAT_WEAPON_BINDS     = true;
bool CHEAT_INFINITE_AMMO    = true;
bool CHEAT_NO_DAMAGE        = false;

bool g_dual_wield_recoil_switch = true;

enum MeshHandle : u32 {
    MH_NONE,
    MH_DEAGLE,
    MH_M4,
    MH_CROSS,
    _MH_COUNT
};

Mesh *g_meshes[_MH_COUNT] = {};

// keep silenced sounds in order
enum SoundHandle : u32 {
    SH_FIRE_DEAGLE,
    SH_FIRE_SILENCED_GUN_HIGH,
    SH_FIRE_SILENCED_GUN_MID,
    SH_FIRE_SILENCED_GUN_LOW,
    SH_TARGET_HIT,
    SH_HEADSHOT_HIT,
    _SH_COUNT
};

Sound *g_sounds[_SH_COUNT] = {};

meta enum WeaponHandle : u32 {
    WH_DEAGLE,
    WH_M4,
    WH_TAP,
    WH_PAL,
    _WH_COUNT
};

struct Weapon {
    WeaponHandle handle;
    string display_name;
    v4 colour;
    f32 damage;
    f32 headshot_damage;
    i64 ammo_count;
    bool automatic;
    f32 firing_cooldown;
    MeshHandle mesh; 
    SoundHandle firing_sound;
    v3 recoil_offset;
    f32 speed_factor;
};

Weapon g_weapons[_WH_COUNT] = {
    Weapon {
        .handle = WH_DEAGLE,
        .display_name = "Deagle",
        .colour = brightness(WHITE, 0.6),
        .damage = 25,
        .headshot_damage = 55,
        .ammo_count = 7,
        .automatic = false,
        .firing_cooldown = 0.8,
        .mesh = MH_DEAGLE,
        .firing_sound = SH_FIRE_DEAGLE,
        .recoil_offset = v3{0, -0.08, -0.4},
        .speed_factor = 0.8,
    },
    Weapon {
        .handle = WH_M4,
        .display_name = "M4",
        .colour = v4 {0.2, 0.2, 0.2, 1},
        .damage = 8,
        .headshot_damage = 20,
        .ammo_count = 35,
        .automatic = true,
        .firing_cooldown = 0.10,
        .mesh = MH_M4,
        .firing_sound = SH_FIRE_SILENCED_GUN_HIGH,
        .recoil_offset = v3{0, -0.01, -0.15},
        .speed_factor = 0.7,
    },
    Weapon {
        .handle = WH_TAP,
        .display_name = "Thoughts & Prayers",
        .colour = v4 {0.05, 0.5, 0.05, 1},
        .damage = 100,
        .headshot_damage = 100,
        .ammo_count = 5,
        .automatic = false,
        .firing_cooldown = 1,
        .mesh = MH_DEAGLE,
        .firing_sound = SH_FIRE_DEAGLE,
        .recoil_offset = v3{0, -0.08, -0.4},
        .speed_factor = 0.5,
    },
    Weapon {
        .handle = WH_PAL,
        .display_name = "Peace & Love",
        .colour = ORANGE,
        .damage = 10,
        .headshot_damage = 20,
        .ammo_count = 30,
        .automatic = true,
        .firing_cooldown = 0.2,
        .mesh = MH_DEAGLE,
        .firing_sound = SH_FIRE_DEAGLE,
        .recoil_offset = v3{0, -0.08, -0.4},
        .speed_factor = 1,
    }
};

meta enum PickupType : u32 {
    PT_NONE,
    PT_M4,
    PT_TAP,
    PT_PAL,
    PT_HEALTH,
};


// @entity
meta enum EntityFlag : u32 {
    EF_PLAYER           = 1 << 0,
    EF_DUMMY            = 1 << 1,
    EF_SPAWN_POINT      = 1 << 2,
    EF_SOLID_HITBOX     = 1 << 3,
    EF_STATIC_HITBOX    = 1 << 4,
    EF_TRIGGER_HITBOX   = 1 << 5,
    EF_DEAD             = 1 << 6,
    EF_PICKUP           = 1 << 7,
    EF_MISSLE           = 1 << 8,
    EF_JUMP_PAD         = 1 << 9,
    EF_DELETE           = 1 << 16,
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

    // flag: player
    f32 max_health;
    f32 health;
    f32 death_cooldown; // not saved

    // flag: pickup
    PickupType pickup_type;
    f32 pickup_cooldown; // not saved

    // flag: jump pad
    f32 jump_pad_cooldown; // not saved
};

enum NetworkMessageType {
    NM_ASSIGN_CLIENT_ID,
    NM_CLIENT_CONNECTED,
    NM_SPAWN_ENTITY,
    NM_SYNC_ENTITY,
    NM_DELETE_ENTITY,
    NM_MOVE_PLAYER,
    NM_PLAYER_HIT,
    NM_SET_WEAPON,
    NM_SPAWN_MISSLE,
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
        struct {
            f32 speed_factor;
            f32 jump;
            v2 input_direction;
        } move_player;
        struct {
            u32 target_id;
            f32 damage;
        } player_hit;
        v3 spawn_dummy;
        WeaponHandle set_weapon;
        Ray spawn_missle;
    };
};

struct RaycastIterator {
    Ray ray;
    f32 distance;
    v3 check_position;
};

struct RaycastIteratorResult {
    Entity *entity;
    v3 hit_position;
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
    
    f32 time;
    Arena arena;
    Sampler network_in_sampler;

    WeaponHandle player_weapon;
    i64 player_ammo;
    f32 player_firing_cooldown;
    i64 spawn_point_count;
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
    FrameBuffer editor_view;

    Entity *selected_entity;
};

struct GameClient {
    GameClientMode mode;

    Camera camera;
    Viewport viewport;
    FrameBuffer game_view;

    State state;
};

#include "type_info.h"

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
void process_network(State *state, f32 delta_time);
void sync_clients(State *state);

void game_server_update(State *state, f32 delta_time);
void game_server_physics(State *state, f32 delta_time);
void game_server_on_trigger_collision(State *state, Entity *trigger, Entity *other);

void game_client_update(State *state, f32 delta_time);
void game_client_draw(State *state);

void editor_update(State *state);
void editor_draw_ui(State *state);

void on_server_receive(State *state, NetworkMessage *message, f32 delta_time);
void on_client_receive(State *state, NetworkMessage *message);

u32 new_entity_id();
Entity *local_spawn_entity(State *state, Entity entity);
void server_spawn_entity(Entity entity);
bool local_delete_entity(State *state, u32 id);
Entity *get_client_player(State *state, u32 client_id);
Entity *get_entity_with_id(State *state, u32 id);
Entity *get_entity_with_flag(State *state, EntityFlag flag);
bool entities_overlap(Entity *a, Entity *b);
void move_to_random_spawn_point(State *state, Entity *entity);
bool player_is_grounded(State *state, Entity *entity);
void log_entity_flags(Entity *entity);

Entity *local_duplicate_entity(State *state, Entity *entity);
Entity *local_spawn_empty(State *state);
Entity *local_spawn_player(State *state);
Entity *local_spawn_dummy(State *state);
Entity *local_spawn_spawn_point(State *state);
Entity *local_spawn_static_box(State *state);
Entity *local_spawn_pickup(State *state, PickupType type);
Entity *local_spawn_missle(State *state);
Entity *local_spawn_jump_pad(State *state);

void game_client_host();
void game_client_connect();
void game_client_stop_game();

bool is_server(State *state);
bool is_client(State *state);
void server_on_new_connection(NetworkLayer *net, Server *server, ConnectionId id);

RaycastIterator raycast_iterator_create(Ray ray, f32 distance);
RaycastIteratorResult next(RaycastIterator *it, State *state);

CubeCollision cube_collision(v3 a_position, v3 a_size, v3 b_position, v3 b_size);
bool point_collision(v3 point, v3 collider_position, v3 collider_size);

void imgui_entity(Entity *entity);
Viewport imgui_viewport(const char *label, u32 texture_id, bool force_focus);
void imgui_v3_control(const char *label, v3 *vector, f32 step = 1);

void clear_level(State *state);
void serialise_level(State *state);
void serialise_entity(YAML::Emitter &out, Entity *entity);
void deserialise_level(State *state);

YAML::Emitter &operator<<(YAML::Emitter &out, string value);
YAML::Emitter &operator<<(YAML::Emitter &out, v3 value);
YAML::Emitter &operator<<(YAML::Emitter &out, v4 value);

void draw_player_weapon(State *state, Weapon *weapon, v3 display_offset, bool show_recoil);
Weapon *get_player_weapon(State *state);
void set_player_weapon(State *state, WeaponHandle weapon, f32 cooldown);
void play_weapon_fire_sound(SoundHandle sound);
void fire_raycast_weapon(State *state, WeaponHandle weapon);
void fire_tap(State *state);

void run_tests();

// @main
int main(i32 argc, const char **argv) { 
    log_set_thread_name("client");

    srand((u32) time(NULL));

#if RUN_TESTS
    run_tests();
    return 0;
#endif

    bool ok = network_layer_init();
    if (!ok) {
        Log("CRASH: failed to strart networking");
        return 1;
    }

    NET()->server.on_new_connection = server_on_new_connection;

    network_layer_start();

    game_client_entry();
    game_client_stop_game();
    
    network_layer_stop();
}

GameServer *GS() {
    Assert(g_game_server);
    return g_game_server;
}

GameClient *GC() {
    Assert(g_game_client);
    return g_game_client;
}

Editor *ED() {
    Assert(g_editor);
    return g_editor;
}

// @startserver
void game_server_start() {
    Assert(g_game_server == NULL);

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
    log_set_thread_name("server");

    GS()->state = State {
        .instance_type = IT_SERVER,
        .instance_id = SERVER_INSTANCE_ID,
        .arena = arena_create(10 * 1024 * 1024),
        .network_in_sampler = {},
        .entities = stack_array_create<Entity, MAX_ENTITIES>(),
    };

    Timer tick_timer = timer_create_ms(GAME_SERVER_MS_PER_TICK);

    Infof("Started game server @ {}tps [thread={}]", i64(1000.0f / f32(GAME_SERVER_MS_PER_TICK)), get_current_thread_id());

    deserialise_level(&GS()->state);

    while (!GS()->shutdown_signal) {
        f32 delta_time = 0;

        if (!timer_is_complete(&tick_timer, &delta_time)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        GS()->state.time += delta_time;

        process_network(&GS()->state, delta_time);
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

    Log("Game server was given shutdown signal.. stopping");
}

// @entrygc @gc
void game_client_entry() {
    { // init all the global stuff
        bool ok = false;

        ok = window_init("Game12", 1280, 720);
        Assert(ok);

        if (!ok) {
            Log("Failed when trying to init the window");
            return;
        }

        ok = renderer_init(WIN(), v4{0.3, 0.45, 0.72, 1}, v3{0.38, 0.38, 0.38}, v3{0.61, 0.61, 0.61}, v3{50, 100, -100}, v3{0, 0, 0});
        Assert(ok);

        if (!ok) {
            Log("Failed when trying to init the renderer");
            return;
        }

        g_meshes[MH_DEAGLE] = mesh_create_from_file(REN(), "resources/models/deagle/deagle.obj");
        Assert(g_meshes[MH_DEAGLE]);

        g_meshes[MH_M4] = mesh_create_from_file(REN(), "resources/models/m4/m4.obj");
        Assert(g_meshes[MH_M4]);

        g_meshes[MH_CROSS] = mesh_create_from_file(REN(), "resources/models/cross/cross.obj");
        Assert(g_meshes[MH_CROSS]);

        ok = sound_engine_init();
        Assert(ok);

        if (!ok) {
            Log("Failed when trying to init the sound engine");
            return;
        }

        g_sounds[SH_FIRE_DEAGLE] = sound_engine_load(SE(), "resources/sounds/deagle_fire.wav");
        Assert(g_sounds[SH_FIRE_DEAGLE]);

        g_sounds[SH_FIRE_SILENCED_GUN_HIGH] = sound_engine_load(SE(), "resources/sounds/silenced_gun_high.wav");
        Assert(g_sounds[SH_FIRE_SILENCED_GUN_HIGH]);

        g_sounds[SH_FIRE_SILENCED_GUN_MID] = sound_engine_load(SE(), "resources/sounds/silenced_gun_mid.wav");
        Assert(g_sounds[SH_FIRE_SILENCED_GUN_MID]);

        g_sounds[SH_FIRE_SILENCED_GUN_LOW] = sound_engine_load(SE(), "resources/sounds/silenced_gun_low.wav");
        Assert(g_sounds[SH_FIRE_SILENCED_GUN_LOW]);

        g_sounds[SH_TARGET_HIT] = sound_engine_load(SE(), "resources/sounds/short_target_hit.wav");
        Assert(g_sounds[SH_TARGET_HIT]);

        g_sounds[SH_HEADSHOT_HIT] = sound_engine_load(SE(), "resources/sounds/short_headshot_hit.wav");
        Assert(g_sounds[SH_HEADSHOT_HIT]);
    }

    g_game_client = new GameClient {
        .mode = GC_EDITOR,
        .camera = camera_create(CameraMode::FIRST_PERSON, 90, v3{0, 0, 0}, 0.1, 300),
        .viewport =  Viewport {
            .focused = false,
            .size = WIN()->frame_buffer_size
        },
        .game_view = FrameBuffer {.size = WIN()->frame_buffer_size},
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
        .editor_view = FrameBuffer {.size = WIN()->frame_buffer_size},
        .selected_entity = NULL,
    };

    { // init editor and client frame buffer
        bool ok = frame_buffer_init(&g_game_client->game_view);
        if (!ok) {
            Log("failed to init game view frame buffer");
            return;
        }

        ok = frame_buffer_init(&g_editor->editor_view);
        if (!ok) {
            Log("failed to init editor view frame buffer");
            return;
        }
    }

    Timer tick_timer = timer_create_ms(GAME_CLIENT_MS_PER_TICK);

    Infof("Started game client @ {}tps [thread={}]", i64(1000.0f / f32(GAME_SERVER_MS_PER_TICK)), get_current_thread_id());

    deserialise_level(&GC()->state);

    while (!glfwWindowShouldClose(WIN()->glfw_window)) {
        f32 delta_time = 0;
        if (timer_is_complete(&tick_timer, &delta_time)) {
            GC()->state.time += delta_time;

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
                process_network(&GC()->state, delta_time);
                game_client_update(&GC()->state, delta_time);
            }
        }

        // draw
        renderer_start_frame(REN());

        game_client_draw(&GC()->state);

        renderer_draw_frame(REN(), &ED()->camera, ED()->viewport, &ED()->editor_view, false);
        renderer_draw_frame(REN(), &GC()->camera, GC()->viewport, &GC()->game_view, true);

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
    Assert(is_client(state)); // what is the server doing here?

}

void process_network(State *state, f32 delta_time) {
    if (is_client(state)) {
        sampler_append(&state->network_in_sampler, f32(network_queue_size(&NET()->client_in_queue)));

        slice<u8> bytes;
        while (network_queue_pop(&NET()->client_in_queue, &bytes)) {
            NetworkMessage *message = (NetworkMessage *) bytes.ptr;
            on_client_receive(state, message);
            slice_free(bytes);
        }
    }

    if (is_server(state)) {
        sampler_append(&state->network_in_sampler, f32(network_queue_size(&NET()->server_in_queue)));

        slice<u8> bytes;
        while (network_queue_pop(&NET()->server_in_queue, &bytes)) {
            NetworkMessage *message = (NetworkMessage *) bytes.ptr;
            on_server_receive(state, message, delta_time);
            slice_free(bytes);
        }
    }
}

void sync_clients(State *state) {
    Assert(is_server(state));

    i64 index = 0;
    while (index < state->entities.len) {
        Entity &entity = state->entities[index];

        // entity is static and is created from the level
        // no need to sync with clients
        if (entity.owner == LEVEL_INSTANCE_ID) {
            index++;
            continue;
        }

        if (BitSet(entity.flags, EF_DELETE)) {
            NetworkMessage message = NetworkMessage{.type = NM_DELETE_ENTITY, .delete_entity = entity.id};
            server_send_to_all_clients(NET(), bytes_from_ptr(&message));

            local_delete_entity(state, entity.id);

            continue;
        }

        NetworkMessage message = NetworkMessage{.type = NM_SYNC_ENTITY, .sync_entity = entity};
        server_send_to_all_clients(NET(), bytes_from_ptr(&message));

        index++;
    }
}

void game_server_update(State *state, f32 delta_time) {
    Assert(is_server(state));

    for (Entity &entity : state->entities) {

        if (BitSet(entity.flags, EF_PICKUP)) {
            entity.pickup_cooldown -= delta_time;
            if (entity.pickup_cooldown <= 0) {
                entity.pickup_cooldown = 0;
            }

            if (entity.pickup_cooldown == 0) {
                for (Entity &other : state->entities) {
                    if (!BitSet(other.flags, EF_PLAYER)) {
                        continue;
                    }
    
                    auto [collided, overlap, distance] = cube_collision(entity.position, v3{entity.size.x, 2, entity.size.z}, other.position, other.size);
                    if (!collided) {
                        continue;
                    }

                    switch (entity.pickup_type) {
                        case PT_M4: {
                            entity.pickup_cooldown = WEAPON_PICKUP_COOLDOWN;
                            NetworkMessage message = NetworkMessage{.type = NM_SET_WEAPON, .set_weapon = WH_M4};
                            server_send_to_client(NET(), bytes_from_ptr(&message), other.owner);
                        } break;
                        case PT_TAP: {
                            entity.pickup_cooldown = WEAPON_PICKUP_COOLDOWN;
                            NetworkMessage message = NetworkMessage{.type = NM_SET_WEAPON, .set_weapon = WH_TAP};
                            server_send_to_client(NET(), bytes_from_ptr(&message), other.owner);
                        } break;
                        case PT_PAL: {
                            entity.pickup_cooldown = WEAPON_PICKUP_COOLDOWN;
                            NetworkMessage message = NetworkMessage{.type = NM_SET_WEAPON, .set_weapon = WH_PAL};
                            server_send_to_client(NET(), bytes_from_ptr(&message), other.owner);
                        } break;
                        case PT_HEALTH: {
                            entity.pickup_cooldown = HEALTH_PICKUP_COOLDOWN;
                            other.health = other.max_health;
                        } break;
                        case PT_NONE: {
                            Warnf("Entity with id {} is marked as a pickup but has PT_NONE assigned", other.id);
                        } break;
                        default: Assertf(false, "Did you add a new pickup?");
                    }
                }
            }
        }

        if (BitSet(entity.flags, EF_JUMP_PAD)) {
            entity.jump_pad_cooldown -= delta_time;
            if (entity.jump_pad_cooldown <= 0) {
                entity.jump_pad_cooldown = 0;
            }

            if (entity.jump_pad_cooldown > 0) {
                continue;
            }

            for (Entity &other : state->entities) {
                if (!BitSet(other.flags, EF_PLAYER)) {
                    continue;
                }

                auto [collided, overlap, distance] = cube_collision(entity.position, v3{entity.size.x, 2, entity.size.z}, other.position, other.size);
                if (!collided) {
                    continue;
                }
                
                other.velocity.y = JUMP_PAD_ACCELERATION;
                entity.jump_pad_cooldown += JUMP_PAD_COOLDOWN;

                // means only one player per cooldown can be effected?
                break;
            }
        }

        if (BitSet(entity.flags, EF_DEAD)) {
            Assert(BitSet(entity.flags, EF_PLAYER));

            entity.death_cooldown -= delta_time;

            if (entity.death_cooldown < 0) {
                entity.health = entity.max_health;

                UnsetBit(entity.flags, EF_DEAD);
                entity.death_cooldown = 0;

                if (!BitSet(entity.flags, EF_DUMMY)) {
                    move_to_random_spawn_point(state, &entity);
                }
            }
        }

        if (BitSet(entity.flags, EF_PLAYER)) {
            if (CHEAT_NO_DAMAGE) {
                entity.health = entity.max_health;
            }

            if (entity.health <= 0 && entity.death_cooldown == 0) {
                entity.death_cooldown = PLAYER_DEATH_COOLDOWN;
                SetBit(entity.flags, EF_DEAD);
            }
        }
    }
}

void game_server_physics(State *state, f32 delta_time) {
    Assert(is_server(state));

    for (Entity &entity : state->entities) {
        if (BitSet(entity.flags, EF_STATIC_HITBOX)) {
            continue;
        }

        if (!BitSet(entity.flags, EF_SOLID_HITBOX) && !BitSet(entity.flags, EF_TRIGGER_HITBOX)) {
            continue;
        }

        if (BitSet(entity.flags, EF_PLAYER)) {
            if (player_is_grounded(state, &entity)) {
                v3 h_velocity = v3{entity.velocity.x, 0, entity.velocity.z};
                v3 drag = -h_velocity * PLAYER_GROUND_DRAG;

                entity.velocity.x += drag.x * delta_time;
                entity.velocity.z += drag.z * delta_time;
            }

            entity.velocity.y -= GRAVITY * delta_time;
        }

        v3 starting_position = entity.position;
        entity.position += entity.velocity * delta_time;

        { // detect and resolve collisions
            if (BitSet(entity.flags, EF_SOLID_HITBOX)) {
                for (Entity &other : state->entities) {
                    if (&entity == &other) {
                        continue;
                    }
       
                    // TODO: players don't collide then..
                    if (!BitSet(other.flags, EF_STATIC_HITBOX)) {
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
    
            if (BitSet(entity.flags, EF_TRIGGER_HITBOX)) {
                for (Entity &other : state->entities) {
                    if (&entity == &other) {
                        continue;
                    }

                    // collision for each other entiity is checked twice for trigger
                    // hitboxes, trigger collision events are only when a new collision
                    // starts so if there was a collision last frame then dont do anything
                    //
                    // FIXME: this means only triggers that move will actually detect a collision
                    // if it is static in the scene it sees that it would of collided with the other
                    // entity before the physics sim. This may not be actually true as it could
                    // of been the other entity that moved and caused the overlap not just this 
                    // - 13:23
                    CubeCollision c_last = cube_collision(starting_position, entity.size, other.position, other.size);
                    if (c_last.collision) {
                        continue;
                    }

                    CubeCollision c_now = cube_collision(entity.position, entity.size, other.position, other.size);
                    if (c_now.collision) {
                        game_server_on_trigger_collision(state, &entity, &other);
                    }
                }
            }
        }
    }
}

void game_server_on_trigger_collision(State *state, Entity *trigger, Entity *other) {
    Assertf(BitSet(trigger->flags, EF_MISSLE), "Just assuming missles for now");

    { // explode missles
        Entity *missle = trigger;

        for (Entity &other : state->entities) {
            if (other.id == missle->id) {
                continue;
            }

            if (!BitSet(other.flags, EF_PLAYER)) {
                continue; 
            }
      
            // vector from the missle to the other
            v3 direction = other.position - missle->position;
            f32 distance = length(direction);

            if (distance > EXPLOSION_RADIUS) {
                continue;
            }

            f32 distance_factor = 1 - (distance / EXPLOSION_RADIUS);
            f32 vfloor = 0.3f; // what do you call this thing?

            // add a little upward direction and clamp distance factor
            other.velocity += norm(direction + v3{0, 1, 0}) * EXPLOSION_FORCE * (distance_factor < vfloor ? vfloor : distance_factor);
            other.health -= EXPLOSION_DAMAGE * distance_factor;
        }

        SetBit(missle->flags, EF_DELETE);
    }

}

void game_client_update(State *state, f32 delta_time) {
    state->player_firing_cooldown -= delta_time;
    if (state->player_firing_cooldown <= 0) {
        state->player_firing_cooldown = 0;
    }

    { // weapon reaload and switching to default
        Assert(state->player_ammo >= 0);

        if (CHEAT_INFINITE_AMMO) {
            state->player_ammo = g_weapons[state->player_weapon].ammo_count;
        }

        if (state->player_ammo == 0) {
            set_player_weapon(state, WH_DEAGLE, WEAPON_SWITCH_COOLDOWN);
        }
    }

    Entity *player = get_client_player(state, state->instance_id);
    if (player != NULL) {
        GC()->camera.position = player->position + v3{0, PLAYER_EYES_OFFSET, 0};
    }

    // check player input
    if (GC()->viewport.focused) {
        if (WIN()->mouse_captured) {
            f32 sensitivity = 0.09;
            v2 mouse_input = MOUSE.delta;
   
            // camera control
            if (length(mouse_input) > 0) {
    
                GC()->camera.rotation += v3{mouse_input.y, mouse_input.x, 0} * sensitivity;
                GC()->camera.rotation.x = clamp(-90, GC()->camera.rotation.x, 90);
            }

            // shooting
            Weapon *player_weapon = get_player_weapon(state);
            InputState state_needed = player_weapon->automatic ? InputState::PRESSED : InputState::DOWN;

            if (MOUSE.buttons[GLFW_MOUSE_BUTTON_1] == state_needed) {
                if (state->player_ammo > 0 && state->player_firing_cooldown <= 0) {
                    state->player_ammo -= 1; 
                    state->player_firing_cooldown = player_weapon->firing_cooldown;

                    g_dual_wield_recoil_switch = !g_dual_wield_recoil_switch;

                    play_weapon_fire_sound(player_weapon->firing_sound);

                    switch (state->player_weapon) {
                        case WH_DEAGLE:
                        case WH_M4:
                            fire_raycast_weapon(state, state->player_weapon);
                        break;
                        case WH_TAP:
                            fire_tap(state);
                        break;
                    }
                }
            }

            // cheats to give weapons 
            if (CHEAT_WEAPON_BINDS && GC()->viewport.focused) {
                if (KEYS[GLFW_KEY_LEFT_CONTROL] == InputState::PRESSED && 
                    KEYS[GLFW_KEY_1] == InputState::DOWN) {
                    set_player_weapon(state, WH_DEAGLE, 0);
                }

                if (KEYS[GLFW_KEY_LEFT_CONTROL] == InputState::PRESSED && 
                    KEYS[GLFW_KEY_2] == InputState::DOWN) {
                    set_player_weapon(state, WH_M4, 0);
                }

                if (KEYS[GLFW_KEY_LEFT_CONTROL] == InputState::PRESSED && 
                    KEYS[GLFW_KEY_3] == InputState::DOWN) {
                    set_player_weapon(state, WH_TAP, 0);
                }

                if (KEYS[GLFW_KEY_LEFT_CONTROL] == InputState::PRESSED && 
                    KEYS[GLFW_KEY_4] == InputState::DOWN) {
                    set_player_weapon(state, WH_PAL, 0);
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
            v3 right = get_right_direction(&GC()->camera);
   
            // remove y component and normalise to just get h input
            v2 h_forward = norm(v2{forward.x, forward.z});
            v2 h_right = norm(v2{right.x, right.z});
        
            v2 horizontal = v2{};
            horizontal += h_right * keyboard_input.x;
            horizontal += h_forward * keyboard_input.z;

            // normalise again to stop diagonal movement being faster
            if (length(horizontal) > 0) {
                horizontal = norm(horizontal);
            }
                 
            NetworkMessage message = NetworkMessage{.client_id = state->instance_id, .type = NM_MOVE_PLAYER, .move_player = {
                .speed_factor = get_player_weapon(state)->speed_factor,
                .jump = keyboard_input.y,
                .input_direction = horizontal
            }};

            client_send_to_server(NET(), bytes_from_ptr(&message));
        }
    }
}

void game_client_draw(State *state) {
    Assert(is_client(state));

    for (Entity &entity : state->entities) {
        v4 draw_colour = entity.colour;

        // client's player
        if (BitSet(entity.flags, EF_PLAYER) && entity.owner == state->instance_id) {
            Weapon *player_weapon = get_player_weapon(state);

            { // draw weapon
                if (player_weapon->handle == WH_PAL) {
                    static bool side = false;

                    draw_player_weapon(state, player_weapon, WEAPON_DISPLAY_OFFSET, g_dual_wield_recoil_switch);
                    draw_player_weapon(state, player_weapon, WEAPON_DISPLAY_OFFSET * v3{-1, 1, 1}, !g_dual_wield_recoil_switch);
                }
                else {
                    draw_player_weapon(state, player_weapon, WEAPON_DISPLAY_OFFSET, true);
                }
            }

            // @hud

            // draw fire cooldown when using non auto gun
            if (!player_weapon->automatic && state->player_firing_cooldown > 0) { 
                f32 max_width = 40;
                f32 height = 3;

                v3 centre = relative_to_screen_position(GC()->viewport, {0.5, 0.5});
                v3 centre_offset = v3{0, -50, 0};

                f32 cooldown_scale = state->player_firing_cooldown / player_weapon->firing_cooldown;

                draw_rectangle_ui(REN(), centre + centre_offset, {max_width * cooldown_scale, height}, {}, alpha(BLACK, 0.3));
            }
       
            { // draw crosshair
                v3 centre = relative_to_screen_position(GC()->viewport, {0.5, 0.5});

                // horizontal
                draw_rectangle_ui(REN(), centre - v3{CROSSHAIR_GAP, 0, 0}, {CROSSHAIR_LENGTH, CROSSHAIR_THICKNESS}, {}, CROSSHAIR_COLOUR);
                draw_rectangle_ui(REN(), centre + v3{CROSSHAIR_GAP, 0, 0}, {CROSSHAIR_LENGTH, CROSSHAIR_THICKNESS}, {}, CROSSHAIR_COLOUR);

                // vertical
                draw_rectangle_ui(REN(), centre - v3{0, CROSSHAIR_GAP, 0}, {CROSSHAIR_THICKNESS, CROSSHAIR_LENGTH}, {}, CROSSHAIR_COLOUR);
                draw_rectangle_ui(REN(), centre + v3{0, CROSSHAIR_GAP, 0}, {CROSSHAIR_THICKNESS, CROSSHAIR_LENGTH}, {}, CROSSHAIR_COLOUR);
            }

            { // draw health
                f32 max_width = 600;
                f32 height = 30;
                v3 centre = relative_to_screen_position(GC()->viewport, {0.5, 0.98});

                f32 health_scale = entity.health / entity.max_health;

                draw_rectangle_ui(REN(), centre, {max_width * health_scale, height}, {}, brightness(RED, 0.8));
                draw_rectangle_ui(REN(), centre, {max_width, height}, {}, brightness(RED, 0.4));
            }

            // blood overlay when dead 
            if (BitSet(entity.flags, EF_DEAD)) {
                v3 top_right = relative_to_screen_position(GC()->viewport, {1, 1});
                v3 centre = top_right * 0.5;
                v2 size = top_right.xy;

                draw_rectangle_ui(REN(), centre, size, {}, alpha(RED, 0.3));
            }
       
            { // draw ammo
                draw_text_ui(REN(), fmt(&state->arena, "{}:  {}", player_weapon->display_name, state->player_ammo), {7, 10, 0}, 30, alpha(BLACK, 0.4));
            }
        }

        // every player
        if (BitSet(entity.flags, EF_PLAYER)) {
            v4 head_colour = BEIGE;

            // shade red when dead
            if (BitSet(entity.flags, EF_DEAD)) {
                draw_colour = mix(draw_colour, RED, 0.65);
                head_colour = mix(draw_colour, RED, 0.65);
            }

            { // draw head
                // want to draw a second cube where the head will be, centre of this cube is eye position
                f32 eyes_to_top = (PLAYER_HEIGHT * 0.5)  - PLAYER_EYES_OFFSET;
                v3 head_size = v3{PLAYER_WIDTH, eyes_to_top * 2, PLAYER_WIDTH};
    
                // add a little extra to stop z fighting
                head_size += v3{0.01, 0.01, 0.01};
    
                draw_cube(REN(), entity.position + v3{0, PLAYER_EYES_OFFSET, 0}, head_size, entity.rotation, head_colour);
            }
        }

        // pickups 
        if (BitSet(entity.flags, EF_PICKUP)) {
            f32 t = sin(state->time * 0.5f);
            v3 pickup_position = entity.position + v3{0, 1.5f + t, 0};
            v3 pickup_rotation = v3{0, t * 360, 0};

            Mesh *mesh = NULL;
            v4 pickup_colour = {};
            v3 pickup_size = {};

            switch (entity.pickup_type) {
                case PT_M4: { 
                    mesh = g_meshes[g_weapons[WH_M4].mesh];
                    pickup_colour = entity.pickup_cooldown > 0 ? brightness(RED, 0.5) : g_weapons[WH_M4].colour;
                    pickup_size = v3{0.8, 0.8, 0.8};
                } break;
                case PT_TAP: { 
                    mesh = g_meshes[g_weapons[WH_TAP].mesh];
                    pickup_colour = entity.pickup_cooldown > 0 ? brightness(RED, 0.5) : g_weapons[WH_TAP].colour;
                    pickup_size = v3{1, 1, 1};
                } break;
                case PT_PAL: { 
                    mesh = g_meshes[g_weapons[WH_PAL].mesh];
                    pickup_colour = entity.pickup_cooldown > 0 ? brightness(RED, 0.5) : g_weapons[WH_PAL].colour;
                    pickup_size = v3{1, 1, 1};
                } break;
                case PT_HEALTH: {
                    mesh = g_meshes[MH_CROSS];
                    pickup_colour = entity.pickup_cooldown > 0 ? brightness(RED, 0.5) : brightness({0.2, 1, 0.2, 1}, 0.8);
                    pickup_size = v3{0.3, 0.3, 0.3};
                } break;
                case PT_NONE: { 
                    Warnf("Entity with id {} is marked as a pickup but has PT_NONE assigned", entity.id);
                    continue;
                } break;
                default: Assertf(false, "Did you add a new pickup type?");
            }

            draw_mesh(REN(), mesh, pickup_position, pickup_size, pickup_rotation, pickup_colour);
        }

        if (BitSet(entity.flags, EF_MISSLE)) {
            i64 trail_count = 6;
            f32 trail_gap = 7;
            f32 trail_radius = 1.3;

            for (i64 i = 0; i < trail_count; i++) {
                f32 t = f32(i) / f32(trail_count);
                v3 trail_direction = -norm(entity.velocity);
                v3 trail_offset = trail_direction * trail_gap * t;
                v4 trail_colour = mix(RED, SUN_YELLOW, t);

                draw_sphere(REN(), entity.position + trail_offset, trail_radius * (1.0f - t), trail_colour);
            }
        }

        if (ED()->selected_entity && ED()->selected_entity->id == entity.id) {
            draw_colour = RED;
        }

        if (DEBUG_DRAW_OWNER) {
            if (entity.owner == LEVEL_INSTANCE_ID) {
                draw_colour = GREEN;
            }
            else if (entity.owner == SERVER_INSTANCE_ID) {
                draw_colour = ORANGE;
            }
            else {
                draw_colour = RED;
            }
        }

        draw_cube(REN(), entity.position, entity.size, entity.rotation, draw_colour);
    }
}

void editor_update(State *state) {
    Assert(is_client(state));

    Camera *camera = &ED()->camera;

    { // editor mouse interaction
        v2 mouse = ED()->viewport.mouse;
        v2 view_size = to_floats(ED()->viewport.size);

        bool mouse_in_viewport = true;
        if (mouse.x < 0 || mouse.x >= view_size.x) {
            mouse_in_viewport = false;
        }
        else if (mouse.y < 0 || mouse.y >= view_size.y) {
            mouse_in_viewport = false;
        }

        // mouse picking
        if (mouse_in_viewport && MOUSE.buttons[GLFW_MOUSE_BUTTON_1] == InputState::DOWN) {
            Ray ray = ray_from_screen_position(camera, ED()->viewport, {mouse.x, mouse.y, -1});
            RaycastIterator it = raycast_iterator_create(ray, camera->far_plane - camera->near_plane);
    
            auto [entity, _] = next(&it, state);
            if (entity) {
                ED()->selected_entity = entity; 
            }
        }
   
        // panning 
        if (MOUSE.buttons[GLFW_MOUSE_BUTTON_2] == InputState::PRESSED) {
            f32 sensitivity = 0.15;
            v2 mouse_input = MOUSE.delta;
         
            if (length(mouse_input) > 0) {
                camera->rotation += v3{mouse_input.y, mouse_input.x, 0} * sensitivity;
                camera->rotation.x = clamp(-90, camera->rotation.x, 90);
            }
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

    // ctrl-D: duplicate selected entity 
    if (KEYS[GLFW_KEY_LEFT_CONTROL] == InputState::PRESSED && 
        KEYS[GLFW_KEY_D] == InputState::DOWN) {

        if (ED()->selected_entity) {
            ED()->selected_entity = local_duplicate_entity(state, ED()->selected_entity);
        }
    }

    // delete: delete selected entity 
    if (KEYS[GLFW_KEY_DELETE] == InputState::DOWN) {
        if (ED()->selected_entity) {
            local_delete_entity(state, ED()->selected_entity->id);
            ED()->selected_entity = NULL;
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
    
    { // render output
        f32 image_downscale = 4;
        ImVec2 size = ImVec2(GC()->viewport.size.x / image_downscale, GC()->viewport.size.y / image_downscale);

        ImGui::Begin("Render output");

        if (ImGui::CollapsingHeader("G buffer")) {
            ImGui::Text("Position  //  Normals");
            ImGui::Image(REN()->g_buffer.position_attachment, size, ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
            ImGui::Image(REN()->g_buffer.normals_attachment, size, ImVec2(0, 1), ImVec2(1, 0));

            ImGui::Text("Albedo  //  Depth");
            ImGui::Image(REN()->g_buffer.albedo_attachment, size, ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
            ImGui::Image(REN()->g_buffer.depth_attachment, size, ImVec2(0, 1), ImVec2(1, 0));
        }

        if (ImGui::CollapsingHeader("Lighting buffer")) {
            ImGui::Text("Position  //  Normals");
            ImGui::Image(REN()->lighting_buffer.position_attachment, size, ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
            ImGui::Image(REN()->lighting_buffer.normals_attachment, size, ImVec2(0, 1), ImVec2(1, 0));

            ImGui::Text("Albedo  //  Depth");
            ImGui::Image(REN()->lighting_buffer.albedo_attachment, size, ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
            ImGui::Image(REN()->lighting_buffer.depth_attachment, size, ImVec2(0, 1), ImVec2(1, 0));
        }

        ImGui::End();
    }
    
    { // debug info
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

            v3 position = GC()->camera.position;
            v3 rotation = GC()->camera.rotation;
            v3 forward = get_forward_direction(&GC()->camera);
            v3 right = get_right_direction(&GC()->camera);
            v3 up = get_up_direction(&GC()->camera);

            ImGui::Text("Position: [%.3f, %.3f, %.3f]", position.x, position.y, position.z);
            ImGui::Text("Rotation: [%.3f, %.3f, %.3f]", rotation.x, rotation.y, rotation.z);
            ImGui::Text("Forward: [%.3f, %.3f, %.3f]", forward.x, forward.y, forward.z);
            ImGui::Text("Right: [%.3f, %.3f, %.3f]", right.x, right.y, right.z);
            ImGui::Text("Up: [%.3f, %.3f, %.3f]", up.x, up.y, up.z);
        }

        { // editor camera
            ImGui::SeparatorText("Editor camera");

            v3 position = ED()->camera.position;
            v3 rotation = ED()->camera.rotation;
            v3 forward = get_forward_direction(&ED()->camera);
            v3 right = get_right_direction(&ED()->camera);
            v3 up = get_up_direction(&ED()->camera);

            ImGui::Text("Position: [%.3f, %.3f, %.3f]", position.x, position.y, position.z);
            ImGui::Text("Rotation: [%.3f, %.3f, %.3f]", rotation.x, rotation.y, rotation.z);
            ImGui::Text("Forward: [%.3f, %.3f, %.3f]", forward.x, forward.y, forward.z);
            ImGui::Text("Right: [%.3f, %.3f, %.3f]", right.x, right.y, right.z);
            ImGui::Text("Up: [%.3f, %.3f, %.3f]", up.x, up.y, up.z);
        }

        ImGui::SeparatorText("Network messages");

        f32 message_in_MB = f32(sizeof(NetworkMessage)) / (8.0f * 1024.0f);
     
        { // client events sampler info
            f32 average = sampler_average(&state->network_in_sampler);
            f32 samples_per_second = sampler_samples_per_second(&state->network_in_sampler);
            f32 messages_per_second = average * samples_per_second;
            f32 MB_per_second = messages_per_second * message_in_MB;
     
            ImGui::Text("Avg: %f", average);
            ImGui::Text("Samples/s: %f", samples_per_second);
            ImGui::Text("Messages/s: %f", messages_per_second);
            ImGui::Text("MB/s: %f", MB_per_second);
            ImGui::PlotLines("Client", state->network_in_sampler.samples, SAMPLER_SIZE, 0, NULL, FLT_MAX, FLT_MAX, ImVec2(0, 60));
        }
     
        if (g_game_server != NULL) { // server events sampler info
            Sampler *sampler = atomic_snapshot_read(&server_messages_snapshot);

            f32 average = sampler_average(sampler);
            f32 samples_per_second = sampler_samples_per_second(sampler);
            f32 messages_per_second = average * samples_per_second;
            f32 MB_per_second = messages_per_second * message_in_MB;
     
            ImGui::Text("Avg: %f", average);
            ImGui::Text("Samples/s: %f", samples_per_second);
            ImGui::Text("Messages/s: %f", messages_per_second);
            ImGui::Text("MB/s: %f", MB_per_second);
            ImGui::PlotLines("Server", sampler->samples, SAMPLER_SIZE, 0, NULL, FLT_MAX, FLT_MAX, ImVec2(0, 60));
        }

        ImGui::End();
    }

    { // settings
        ImGui::Begin("Settings");

        if (ImGui::CollapsingHeader("Cheats")) {
            ImGui::Checkbox("Weapon binds", &CHEAT_WEAPON_BINDS);
            ImGui::Checkbox("Infinite ammo", &CHEAT_INFINITE_AMMO);
            ImGui::Checkbox("No damage", &CHEAT_NO_DAMAGE);

            { // give weapon buttons
                ImGui::SeparatorText("Give weapon");

                EnumValue<WeaponHandle> *weapons = meta_values<WeaponHandle>();
                for (i32 i = 0; i < meta_count<WeaponHandle>() - 1; i++) {
                    if (i != 0) {
                        ImGui::SameLine();
                    }

                    if (ImGui::Button(weapons[i].name.c())) {
                        set_player_weapon(state, weapons[i].value, 0);
                    }
                }
            }
        }

        if (ImGui::CollapsingHeader("Player")) {
            Entity *player = get_client_player(state, state->instance_id);
            if (player) {
                imgui_entity(player);
            }

            ImGui::SeparatorText("Movement");
            ImGui::InputFloat("Ground acceleration", &PLAYER_GROUND_ACCELERATION);
            ImGui::InputFloat("Jump acceleration", &PLAYER_JUMP_ACCELERATION);
            ImGui::InputFloat("Ground drag", &PLAYER_GROUND_DRAG);
            ImGui::InputFloat("Air control", &PLAYER_AIR_CONTROL);
            ImGui::InputFloat("Gravity", &GRAVITY);

            ImGui::SeparatorText("Character");
            ImGui::SliderFloat("Eyes offset", &PLAYER_EYES_OFFSET, 0, PLAYER_HEIGHT * 0.5);

            ImGui::SeparatorText("Weapon");
            ImGui::SliderFloat("Fire Cooldown", &state->player_firing_cooldown, 0, g_weapons[state->player_weapon].firing_cooldown);
            ImGui::InputInt("Ammo", (i32 *) &state->player_ammo);
            ImGui::SliderFloat3("Weapon offset", &WEAPON_DISPLAY_OFFSET.x, -2, 2);
        }

        if (ImGui::CollapsingHeader("Renderer")) {
            ImGui::Checkbox("Draw network owner", &DEBUG_DRAW_OWNER);
            ImGui::ColorEdit4("Clear colour",   &REN()->clear_colour[0]);
            ImGui::ColorEdit3("Ambient light",  &REN()->ambient_light[0]);
            ImGui::ColorEdit3("Sun colour",     &REN()->sun_colour[0]);
            ImGui::ColorEdit3("Shadow colour",  &REN()->shadow_colour[0]);
            imgui_v3_control("Sun position", &REN()->sun_position);
        }

        if (ImGui::CollapsingHeader("Crosshair")) {
            ImGui::SliderFloat("Gap", &CROSSHAIR_GAP, 0, 20);
            ImGui::SliderFloat("Length", &CROSSHAIR_LENGTH, 0, 20);
            ImGui::SliderFloat("Thickness", &CROSSHAIR_THICKNESS, 0, 20);
            ImGui::ColorEdit4("Colour", &CROSSHAIR_COLOUR[0]);
        }

        ImGui::End();
    }

    { // level
        ImGui::Begin("Level & Entities");

        ImGui::SeparatorText("Level");

        if (GC()->mode == GC_EDITOR) {
            if (ImGui::Button("Host")) {
                ED()->selected_entity = NULL;
                clear_level(state);
                game_client_host();
            }
    
            ImGui::SameLine();
        
            if (ImGui::Button("Connect")) {
                ED()->selected_entity = NULL;
                clear_level(state);
                game_client_connect();
            }
    
            ImGui::SameLine();
        
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
        }
        else {
            if (ImGui::Button("Stop game")) {
                game_client_stop_game();
                clear_level(state);
                deserialise_level(state);
            }
        }
 
        ImGui::SeparatorText("Spawn Entities");

        if (ImGui::Button("Empty")) {
            ED()->selected_entity = local_spawn_empty(state);
        }

        ImGui::SameLine();

        if (ImGui::Button("Spawn point")) {
            ED()->selected_entity = local_spawn_spawn_point(state);
        }

        ImGui::SameLine();

        if (ImGui::Button("Static box")) {
            ED()->selected_entity = local_spawn_static_box(state);
        }

        ImGui::SameLine();

        if (ImGui::Button("M4 pickup")) {
            ED()->selected_entity = local_spawn_pickup(state, PT_M4);
        }

        if (ImGui::Button("T&P pickup")) {
            ED()->selected_entity = local_spawn_pickup(state, PT_TAP);
        }

        ImGui::SameLine();

        if (ImGui::Button("P&L pickup")) {
            ED()->selected_entity = local_spawn_pickup(state, PT_PAL);
        }

        ImGui::SameLine();

        if (ImGui::Button("Health pickup")) {
            ED()->selected_entity = local_spawn_pickup(state, PT_HEALTH);
        }

        if (ImGui::Button("Jump pad")) {
            ED()->selected_entity = local_spawn_jump_pad(state);
        }

        ImGui::SameLine();

        if (ImGui::Button("Dummy")) {
            ED()->selected_entity = local_spawn_dummy(state);
        }

        ImGui::SeparatorText("Entities in level");
 
        for (i64 i = 0; i < state->entities.len; i++) {
            Entity *entity = &state->entities[i];
    
            ImGui::PushID(i);

            static const int buffer_size = 96;
            static char label_buffer[buffer_size] = {};
   
            MemZero(label_buffer, buffer_size);
            snprintf(label_buffer, buffer_size, "id: %u", entity->id);

            if (ImGui::Button(label_buffer, ImVec2(200, 20))) {
                ED()->selected_entity = entity;
            }

            ImGui::SameLine();

            if (ImGui::Button("Goto")) {
                ED()->camera.position = entity->position;
            }
    
            ImGui::PopID();
        }
        
        ImGui::End();
    }

    { // inspector
        ImGui::Begin("Inspector");
        if (ED()->selected_entity) {
            if (ImGui::Button("Deselect")) {
                ED()->selected_entity = NULL;
            }
    
            ImGui::SameLine();
    
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7, 0.1, 0.1, 1));
            if (ImGui::Button("Delete")) {
                Assert(local_delete_entity(state, ED()->selected_entity->id));
                ED()->selected_entity = NULL;
            }
            ImGui::PopStyleColor();
    
            if (ED()->selected_entity) {
                imgui_entity(ED()->selected_entity);
            }
        }

        ImGui::End();
    }

    GC()->viewport = imgui_viewport("Game", GC()->game_view.albedo_attachment, WIN()->mouse_captured);
    ED()->viewport = imgui_viewport("Editor", ED()->editor_view.albedo_attachment, false);

    draw_imgui_frame();
}

void on_server_receive(State *state, NetworkMessage *message, f32 delta_time) {
    switch (message->type) {
        case NM_CLIENT_CONNECTED: {
            // when client connects, the server generates this message and a few things are required to happen
            // 1. The client is assigned an id from the server
            // 2. Any existing entities are sent to the new client to spawn
            // 3. The player entity is spawn on all clients and is owned by the new client
            // - 09/08/25
            ConnectionId connection_id = message->client_connected;
            Infof("Processing new client connection: connection_id={}", connection_id);
            
            { // assign client id
                Logf("Assigning new client: id={}", connection_id);

                NetworkMessage message = NetworkMessage{.type = NM_ASSIGN_CLIENT_ID, .assign_client_id = connection_id};
                server_send_to_client(NET(), bytes_from_ptr(&message), connection_id);
            }

            { // spawn any entities on new client
                Logf("Spawning {} existing entities on new client", state->entities.len);

                for (Entity &entity : state->entities) {
                    NetworkMessage message = NetworkMessage{.type = NM_SPAWN_ENTITY, .spawn_entity = entity};
                    server_send_to_client(NET(), bytes_from_ptr(&message), connection_id);
                }
            }

            { // spawn new player entity on all clients
                Entity *new_player = local_spawn_player(state);
                new_player->position = {};
                new_player->owner = connection_id;

                move_to_random_spawn_point(state, new_player);

                Infof("Spawning new player entity: entity_id={}, owner={} position={}", new_player->id, new_player->owner, new_player->position);

                NetworkMessage message = NetworkMessage{.type = NM_SPAWN_ENTITY, .spawn_entity = *new_player};
                server_send_to_all_clients(NET(), bytes_from_ptr(&message));
            }
        } break;
        case NM_SPAWN_ENTITY: {
            Entity entity = message->spawn_entity;
            entity.id = new_entity_id();

            Logf("Server spawning entity: id={}, owner={}", entity.id, entity.owner);
            local_spawn_entity(state, entity);
            NetworkMessage message = NetworkMessage{.type = NM_SPAWN_ENTITY, .spawn_entity = entity};
            server_send_to_all_clients(NET(), bytes_from_ptr(&message));
        } break;
        case NM_MOVE_PLAYER: {
            Entity *player = get_client_player(state, message->client_id);
            if (!player) {
                return;
            }

            if (player->death_cooldown > 0) {
                return;
            }

            bool grounded = player_is_grounded(state, player);

            if (grounded) {
                player->velocity.x += message->move_player.input_direction.x * message->move_player.speed_factor * PLAYER_GROUND_ACCELERATION;
                player->velocity.y += message->move_player.jump                                                  * PLAYER_JUMP_ACCELERATION;
                player->velocity.z += message->move_player.input_direction.y * message->move_player.speed_factor * PLAYER_GROUND_ACCELERATION;
            }
            else {
                v2 wish_direction = message->move_player.input_direction;

                v2 h_velocity = v2{player->velocity.x, player->velocity.z}; 
                f32 h_speed = length(h_velocity); 

                if (length(wish_direction) > 0 && h_speed > 0) {
                    v2 h_direction = norm(h_velocity); 

                    v2 new_h_direction = norm(HMM_LerpV2(h_direction, PLAYER_AIR_CONTROL * delta_time, wish_direction));

                    player->velocity.x = new_h_direction.x * h_speed;
                    player->velocity.z = new_h_direction.y * h_speed;
                }

                f32 allignment = HMM_DotV2(h_velocity, wish_direction);
            }
        } break;
        case NM_PLAYER_HIT: {
            Entity *entity = get_entity_with_id(state, message->player_hit.target_id);
            if (entity == NULL || !BitSet(entity->flags, EF_PLAYER)) {
                return;
            }

            if (entity->death_cooldown > 0) {
                return;
            }

            entity->health -= message->player_hit.damage;
        } break;
        case NM_SPAWN_MISSLE: {
            Entity *missle = local_spawn_missle(state);
            missle->owner = SERVER_INSTANCE_ID;
            missle->position = message->spawn_missle.origin;
            missle->velocity = message->spawn_missle.direction * MISSLE_SPEED;

            NetworkMessage message = NetworkMessage{.type = NM_SPAWN_ENTITY, .spawn_entity = *missle};
            server_send_to_all_clients(NET(), bytes_from_ptr(&message));
        } break;
        default: {
            Log("WARNING unknown message sent");
        } break;
    }
}

void on_client_receive(State *state, NetworkMessage *message) {
    switch (message->type) {
        case NM_ASSIGN_CLIENT_ID: {
            state->instance_id = message->assign_client_id;
            Logf("Client assigned id={}", state->instance_id);
        } break;
        case NM_SPAWN_ENTITY: {
            Logf("Client spawning entity: id={}, owner={}", message->spawn_entity.id, message->spawn_entity.owner);
            local_spawn_entity(state, message->spawn_entity);
        } break;
        case NM_SYNC_ENTITY: {
            Entity *entity = get_entity_with_id(state, message->sync_entity.id);
            if (entity != NULL) {
                *entity = message->sync_entity;
            }
        } break;
        case NM_DELETE_ENTITY: {
            Logf("Client deleting entity: id={}", message->delete_entity);

            for (i64 i = 0; i < state->entities.len; i++) {
                Entity *entity = &state->entities[i];
        
                if (entity->id == message->delete_entity) {
                    swap_remove(&state->entities, i);
                    return;
                }
            }
        } break;
        case NM_SET_WEAPON: {
            Logf("Client was told to use a new weapon: {}", (u32) message->set_weapon);
            set_player_weapon(state, message->set_weapon, 0);
        } break;
        default: {
            Log("WARNING unknown message sent");
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

bool local_delete_entity(State *state, u32 id) {
    for (i64 i = 0; i < state->entities.len; i++) {
        Entity *entity = &state->entities[i];
 
        if (entity->id == id) {
            swap_remove(&state->entities, i);
            return true;
        }
    }

    return false;
}

Entity *get_client_player(State *state, u32 client_id) {
    for (Entity &entity : state->entities) {
        if (BitSet(entity.flags, EF_DUMMY)) {
            continue;
        }

        if (BitSet(entity.flags, EF_PLAYER) && entity.owner == client_id) {
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
        if (BitSet(entity.flags, flag)) {
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

void move_to_random_spawn_point(State *state, Entity *entity) {
    Assertf(state->spawn_point_count > 0, "No spawn points in the scene?");

    // get a random number from 0 -> spawn point count
    // skip that number of spawn points in the list and
    // pick the next in the list
    i64 spawn_point_number = rand_i64(0, state->spawn_point_count);
    i64 current_spawn_point_number = 0;

    for (Entity &other : state->entities) {
        if (BitSet(other.flags, EF_SPAWN_POINT)) {
            if (spawn_point_number == current_spawn_point_number) {
                entity->position = other.position + v3{0, PLAYER_HEIGHT + 1, 0};
                break;
            }

            current_spawn_point_number++;
        }
    }
}

bool player_is_grounded(State *state, Entity *entity) {
    Assert(BitSet(entity->flags, EF_PLAYER));

    v3 collider_size        = v3{PLAYER_WIDTH, 0.2, PLAYER_WIDTH};
    f32 collider_y          = entity->position.y - (entity->size.y * 0.5) - (collider_size.y * 0.5);
    v3 collider_position    = v3{entity->position.x, collider_y, entity->position.z};

    for (Entity &other : state->entities) {
        if (!BitSet(other.flags, EF_STATIC_HITBOX)) {
            continue;
        }

        auto [collided, overlap, distance] = cube_collision(collider_position, collider_size, other.position, other.size);
        if (collided) {
            return true;
        }
    }

    return false;
}

void log_entity_flags(Entity *entity) {
    Logf("List of entity flags set for entity id={}", entity->id);

    EnumValue<EntityFlag> *flags = meta_values<EntityFlag>();
    for (i64 i = 0; i < meta_count<EntityFlag>(); i++) {
        if (BitSet(entity->flags, flags[i].value)) {
            Log(flags[i].name);
        }
    }
}

Entity *local_duplicate_entity(State *state, Entity *entity) {
    Entity new_entity = *entity;

    new_entity.id = new_entity_id();
    new_entity.position.x += new_entity.size.x * 0.5;
    new_entity.position.z += new_entity.size.z * 0.5;

    return local_spawn_entity(state, new_entity);
}

Entity *local_spawn_empty(State *state) {
    Entity entity = Entity {
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = v3{1, 1, 1},
        .colour = v4{1, 1, 1, 1},
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_player(State *state) {
    Entity entity = Entity {
        .flags = EF_PLAYER | EF_SOLID_HITBOX,
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = v3{PLAYER_WIDTH, PLAYER_HEIGHT, PLAYER_WIDTH},
        .colour = TURQUOISE,
        .max_health = 100,
        .health = 100,
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_dummy(State *state) {
    Entity entity = Entity {
        .flags = EF_PLAYER | EF_DUMMY | EF_SOLID_HITBOX,
        .id = new_entity_id(),
        .owner = SERVER_INSTANCE_ID,
        .size = v3{PLAYER_WIDTH, PLAYER_HEIGHT, PLAYER_WIDTH},
        .colour = BLUE,
        .max_health = 100,
        .health = 100,
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_spawn_point(State *state) {
    Entity entity = Entity {
        .flags = EF_SPAWN_POINT,
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = v3{3, 0.2, 3},
        .colour = GREEN,
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
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_pickup(State *state, PickupType type) {
    Entity entity = Entity {
        .flags = EF_PICKUP,
        .id = new_entity_id(),
        .owner = SERVER_INSTANCE_ID,
        .size = v3{2, 0.1, 2},
        .colour = brightness(WHITE, 0.5),
        .pickup_type = type
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_missle(State *state) {
    Entity entity = Entity {
        .flags = EF_MISSLE | EF_TRIGGER_HITBOX,
        .id = new_entity_id(),
        .owner = SERVER_INSTANCE_ID,
        .size = v3{0.5, 0.5, 0.5},
        .colour = brightness(WHITE, 0.2),
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_jump_pad(State *state) {
    Entity entity = Entity {
        .flags = EF_JUMP_PAD,
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = v3{3, 0.2, 3},
        .colour = PURPLE,
    };

    return local_spawn_entity(state, entity);
}

void game_client_host() {
    Info("starting hosted game");

    GC()->mode = GC_HOSTED;

    game_server_start();
    network_layer_start_server(NET());
    network_layer_start_client(NET(), "::1");
}

void game_client_connect() {
    Info("starting and connecting to local-hosted game");

    GC()->mode = GC_CLIENT;

    network_layer_start_client(NET(), "::1");
}

void game_client_stop_game() {
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
    Logf("New connection received, sending server new connection message [thread={}]", get_current_thread_id());

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

RaycastIteratorResult next(RaycastIterator *it, State *state) {
    // how much to step along the ray
    // I have no idea what is good here
    const f32 STEP = 0.05f;
    v3 v_step = it->ray.direction * STEP;

    while (length(it->check_position - it->ray.origin) <= it->distance) {
        it->check_position += v_step;

        for (Entity &entity : state->entities) {
            bool hit = point_collision(it->check_position, entity.position, entity.size);
            if (hit) {
                return RaycastIteratorResult {.entity = &entity, .hit_position = it->check_position};
            }
        }
    }

    return RaycastIteratorResult {.entity = NULL, .hit_position = {}};
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

void imgui_entity(Entity *entity) {
    // flags check box list
    if (ImGui::CollapsingHeader("flags")) {
        EnumValue<EntityFlag> *values = meta_values<EntityFlag>();
        int members_count = meta_count<EntityFlag>();

        for (i32 i = 0; i < members_count; i++) {
            bool has_flag = BitSet(entity->flags, values[i].value);
            ImGui::Checkbox(values[i].name.c(), &has_flag);

            if (has_flag) {
                SetBit(entity->flags, values[i].value);
            }
            else {
                UnsetBit(entity->flags, values[i].value);
            }
        }
    }

    ImGui::Text("id: %u", entity->id);
    ImGui::Text("owner: %u", entity->owner);
    imgui_v3_control("position", &entity->position);
    imgui_v3_control("size", &entity->size);
    imgui_v3_control("rotation", &entity->rotation);
    imgui_v3_control("velocity", &entity->velocity);
    ImGui::Text("colour: "); ImGui::SameLine(); ImGui::ColorEdit4("##colour", &entity->colour[0], ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    ImGui::InputFloat("max health", &entity->max_health);
    ImGui::InputFloat("health", &entity->health);
    ImGui::InputFloat("death cooldown", &entity->death_cooldown);

    { // pickup type enum combo box
        EnumValue<PickupType> *values = meta_values<PickupType>();
        int members_count = meta_count<PickupType>();
        i32 selected_index = meta_index(entity->pickup_type);
        string selected_name = values[selected_index].name;
    
        if (ImGui::BeginCombo("pickup type", selected_name.c())) {
            for (i32 i = 0; i < members_count; i++) {
                bool is_selected = selected_index == i;
    
                if (ImGui::Selectable(values[i].name.c(), is_selected))  {
                    entity->pickup_type = values[i].value;
                }
    
                // set the initial focus when opening the combo
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
    
            ImGui::EndCombo();
        }
    }

    ImGui::InputFloat("pickup cooldown", &entity->pickup_cooldown);
    ImGui::InputFloat("jump pad cooldown", &entity->jump_pad_cooldown);
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

void imgui_v3_control(const char *label, v3 *vector, f32 step) {
    ImVec4 x_button_colour = ImVec4(0.7, 0.1, 0.1, 1);
    ImVec4 y_button_colour = ImVec4(0.1, 0.7, 0.1, 1);
    ImVec4 z_button_colour = ImVec4(0.1, 0.1, 0.7, 1);

    ImGui::PushID(label);
    ImGui::Columns(2);

    { // label column
        ImGui::SetColumnWidth(0, 80);
        ImGui::Text(label);
        ImGui::NextColumn();
    }

    { // controls column
        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
   
        { // X
            ImGui::PushStyleColor(ImGuiCol_Button, x_button_colour);
            if (ImGui::Button("X")) {
                vector->x = 0;        
            }
        
            ImGui::SameLine();
            ImGui::DragFloat("##X", &(*vector)[0], step);
            ImGui::PopItemWidth();
        }

        { // Y
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, y_button_colour);
            if (ImGui::Button("Y")) {
                vector->y = 0;        
            }
        
            ImGui::SameLine();
            ImGui::DragFloat("##Y", &(*vector)[1], step);
            ImGui::PopItemWidth();
        }

        { // Z
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, z_button_colour);
            if (ImGui::Button("Z")) {
                vector->z = 0;        
            }
        
            ImGui::SameLine();
            ImGui::DragFloat("##Z", &(*vector)[2], step);
            ImGui::PopItemWidth();
        }
    }
    
    ImGui::PopStyleColor(3);
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
        Log("Failed to create file for saving level");
        return;
    }

    slice<u8> bytes = slice_create((u8 *) out.c_str(), out.size());

    ok = write_file(&file, bytes);
    if (!ok) {
        Log("Failed to write data to file when saving level");
        return;
    }

    close_file(&file);

    Log("Level was saved");
}

void serialise_entity(YAML::Emitter &out, Entity *entity) {
    out << YAML::BeginMap;

    out << YAML::Key << "flags"         << YAML::BeginSeq;
    { // entity flag sequence
        EnumValue<EntityFlag> *values = meta_values<EntityFlag>();
        int members_count = meta_count<EntityFlag>();

        for (i32 i = 0; i < members_count; i++) {
            if (BitSet(entity->flags, values[i].value)) {
                out << values[i].name;
            }
        }
    }
    out << YAML::EndSeq;

    out << YAML::Key << "id"            << YAML::Value << entity->id;
    out << YAML::Key << "owner"         << YAML::Value << entity->owner;
    out << YAML::Key << "position"      << YAML::Value << entity->position;
    out << YAML::Key << "size"          << YAML::Value << entity->size;
    out << YAML::Key << "rotation"      << YAML::Value << entity->rotation;
    out << YAML::Key << "velocity"      << YAML::Value << entity->velocity;
    out << YAML::Key << "colour"        << YAML::Value << entity->colour;
    out << YAML::Key << "max_health"    << YAML::Value << entity->max_health;
    out << YAML::Key << "health"        << YAML::Value << entity->health;
    out << YAML::Key << "pickup_type"   << YAML::Value << meta_name(entity->pickup_type);
    out << YAML::EndMap;
}

void deserialise_level(State *state) {
    YAML::Node root = YAML::LoadFile("resources/levels/main.yaml");

    YAML::Node entities = root["entities"];
    if (!entities) {
        Err("No entities field in level file");
        return;
    }

    state->spawn_point_count = 0;
    reset(&state->entities);

    for (auto node : entities) {
        Entity entity = Entity {};

        { // decode flags from string array
            for (auto flag_node : node["flags"]) {
                // convert value in yaml to string
                std::string s = flag_node.as<std::string>();
                string flag_name = slice_create((u8 *) s.c_str(), s.size());

                // check if saved name is valid
                EnumValue<EntityFlag> *flag = meta_value<EntityFlag>(flag_name);
                if (!flag) {
                    Warnf("No Entity flag was found with name \"{}\", okay if deleted but could be a bug!!", flag_name);
                    Breakpoint;
                }

                SetBit(entity.flags, (u32) flag->value);
            }
        }

        entity.id =                          node["id"].as<u32>();
        entity.owner =                       node["owner"].as<u32>();
        entity.position =                    node["position"].as<v3>();
        entity.size =                        node["size"].as<v3>();
        entity.rotation =                    node["rotation"].as<v3>();
        entity.velocity =                    node["velocity"].as<v3>();
        entity.colour =                      node["colour"].as<v4>();
        entity.max_health =                  node["max_health"].as<f32>();
        entity.health =                      node["health"].as<f32>();

        { // decode pickup type from string
            std::string s = node["pickup_type"].as<std::string>();
            string flag_name = slice_create((u8 *) s.c_str(), s.size());

            // check if saved name is valid
            EnumValue<PickupType> *pickup_type = meta_value<PickupType>(flag_name);
            if (!pickup_type) {
                Warnf("No pickup type was found with name \"{}\", okay if deleted but could be a bug!!", flag_name);
                Breakpoint;
            }

            entity.pickup_type = pickup_type->value;
        }

        if (BitSet(entity.flags, EF_SPAWN_POINT)) {
            state->spawn_point_count += 1;
        }

        append(&state->entities, entity);
    }

    set_player_weapon(state, WH_DEAGLE, 0);

    Infof("Level was loaded with {} entities and {} spawn points", state->entities.len, state->spawn_point_count);
}

YAML::Emitter &operator<<(YAML::Emitter &out, string value) {
    // this may look like it is just writing a c string but
    // actually you forgot something... this is c++. So the
    // creator of this library does something behind your back.
    // this creates a std::string and then does the write, YAY
    out << value.c();
    return out;
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

void draw_player_weapon(State *state, Weapon *weapon, v3 display_offset, bool show_recoil) {
    v3 forward = get_forward_direction(&GC()->camera);
    v3 up = get_up_direction(&GC()->camera);
    v3 right = get_right_direction(&GC()->camera);

    v3 weapon_position = v3{};
    weapon_position += display_offset.x * right;
    weapon_position += display_offset.y * up;
    weapon_position += display_offset.z * forward;

    // apply recoil if there is cooldown
    if (show_recoil && state->player_firing_cooldown > 0) { 
        f32 cooldown_scale = state->player_firing_cooldown / weapon->firing_cooldown;
        v3 wro = weapon->recoil_offset;
        v3 recoil_offset = v3{};

        recoil_offset += wro.x * right;
        recoil_offset += wro.y * up;
        recoil_offset += wro.z * forward;
        recoil_offset *= cooldown_scale;

        weapon_position += recoil_offset;
    }

    weapon_position += GC()->camera.position;
    v3 weapon_rotation = v3{GC()->camera.rotation.x, -GC()->camera.rotation.y, 0};

    draw_mesh(REN(), g_meshes[weapon->mesh], weapon_position, {1, 1, 1}, weapon_rotation, weapon->colour);
}

Weapon *get_player_weapon(State *state) {
    Assert(state->player_weapon >= 0 && state->player_weapon < _WH_COUNT);

    return &g_weapons[state->player_weapon];
}

void set_player_weapon(State *state, WeaponHandle weapon, f32 cooldown) {
    Weapon *w = &g_weapons[weapon];

    state->player_weapon = weapon;
    state->player_ammo = w->ammo_count;
    state->player_firing_cooldown = cooldown;
}

void play_weapon_fire_sound(SoundHandle sound) {
    if (sound == SH_FIRE_DEAGLE) {
        sound_engine_play(g_sounds[sound]);
    } 
    else if (sound == SH_FIRE_SILENCED_GUN_HIGH) {
        static i64 last_played = 0;

        SoundHandle actual_sound = (SoundHandle) (SH_FIRE_SILENCED_GUN_HIGH + last_played);
        sound_engine_play(g_sounds[actual_sound]);

        i64 sound_variations = SH_FIRE_SILENCED_GUN_LOW - SH_FIRE_SILENCED_GUN_HIGH;
        last_played += 1;

        if (last_played > sound_variations) {
            last_played = 0;
        }
    }
}

void fire_raycast_weapon(State *state, WeaponHandle weapon) {
    Ray ray = ray_create(GC()->camera.position, get_forward_direction(&GC()->camera));
    RaycastIterator it = raycast_iterator_create(ray, GC()->camera.far_plane - GC()->camera.near_plane);

    Weapon *player_weapon = &g_weapons[weapon];
    
    Entity *hit_entity = NULL;
    v3 hit_position = v3{};

    while (true) {
        RaycastIteratorResult result = next(&it, state);
        // ray cast failed if:
        // 1. didn't hit anything, stop
        // 2. didn't hit the player, stop
        // 3. hit the clients player, try again
        // 4. hit another player but they are danother player but they are dead 
        if (result.entity == NULL) {
            break;
        }

        if (!BitSet(result.entity->flags, EF_PLAYER)) {
            break;
        }

        if (result.entity->owner == state->instance_id) {
            continue;
        }

        if (BitSet(result.entity->flags, EF_DEAD)) {
            break;
        }

        hit_entity = result.entity;
        hit_position = result.hit_position;
        break;
    } 

    if (hit_entity) {
        f32 damage = player_weapon->damage;
        SoundHandle hit_sound = SH_TARGET_HIT;
        f32 hit_height_offset = hit_position.y - hit_entity->position.y;
        f32 half_head_size = (PLAYER_HEIGHT * 0.5)  - PLAYER_EYES_OFFSET;

        if (hit_height_offset >= PLAYER_EYES_OFFSET - half_head_size) {
            damage = player_weapon->headshot_damage;
            hit_sound = SH_HEADSHOT_HIT;
        }

        sound_engine_play(g_sounds[hit_sound]);

        NetworkMessage message = NetworkMessage {
            .client_id = state->instance_id, 
            .type = NM_PLAYER_HIT, 
            .player_hit = {
                .target_id = hit_entity->id,
                .damage = damage 
            } 
        };

        client_send_to_server(NET(), bytes_from_ptr(&message));
    }
}

void fire_tap(State *state) {
    v3 forward = get_forward_direction(&GC()->camera);
    // spawn it at little how so we dont hit the player 
    Ray ray = ray_create(GC()->camera.position + forward, forward);

    NetworkMessage message = NetworkMessage {
        .client_id = state->instance_id, 
        .type = NM_SPAWN_MISSLE,
        .spawn_missle = ray
    };

    client_send_to_server(NET(), bytes_from_ptr(&message));
}

void run_tests() {
#if 0
    Assert(1 >= 1000);
    Assertf(1 >= 1000, "this shouldnt of happened");
    Unreachable("Shouldn't have got here lil bro");
#endif

    log_set_thread_name("client");

    Log("Hello logger");
    Logf("Hello logger {}", "How are you doing?");

    Info("Hello info");
    Infof("Hello info {}", "How are you doing?");

    Warn("Hello warning");
    Warnf("Hello warning {}", "How are you doing?");

    Err("Hello error");
    Errf("Hello error {}", "How are you doing?");

    Fatal("Hello fatal");
    Fatalf("Hello fatal {}", "How are you doing?");

    std::this_thread::sleep_for(std::chrono::seconds(20));
}

template<>
void fmt_value(DynamicArray<u8> *bytes, v2i value) {
    append_many(bytes, slice<u8>("v2i {"));
    fmt_value(bytes, value.x);
    append_many(bytes, slice<u8>(", "));
    fmt_value(bytes, value.y);
    append_many(bytes, slice<u8>("}"));
}

template<>
void fmt_value(DynamicArray<u8> *bytes, v2 value) {
    append_many(bytes, slice<u8>("v2 {"));
    fmt_value(bytes, value.x);
    append_many(bytes, slice<u8>(", "));
    fmt_value(bytes, value.y);
    append_many(bytes, slice<u8>("}"));
}

template<>
void fmt_value(DynamicArray<u8> *bytes, v3i value) {
    append_many(bytes, slice<u8>("v3i {"));
    fmt_value(bytes, value.x);
    append_many(bytes, slice<u8>(", "));
    fmt_value(bytes, value.y);
    append_many(bytes, slice<u8>(", "));
    fmt_value(bytes, value.z);
    append_many(bytes, slice<u8>("}"));
}

template<>
void fmt_value(DynamicArray<u8> *bytes, v3 value) {
    append_many(bytes, slice<u8>("v3 {"));
    fmt_value(bytes, value.x);
    append_many(bytes, slice<u8>(", "));
    fmt_value(bytes, value.y);
    append_many(bytes, slice<u8>(", "));
    fmt_value(bytes, value.z);
    append_many(bytes, slice<u8>("}"));
}
