#include "libs/libs.h"
#include "ack.cpp"
#include "math.cpp"
#include "net.cpp"
#include "engine.cpp"
#include "platform.h"
#include "meta.h"

#include <cmath>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <time.h>
#include <stdlib.h>
#include <chrono>
#include <atomic>

#define MAX_ENTITIES 500
#define LEVEL_INSTANCE_ID 0
#define SERVER_INSTANCE_ID 1
#define GAME_MS_PER_TICK 10
#define GAME_MS_PER_FRAME 8
#define MAX_TEAMS 2

#define RUN_TESTS 0

string g_level_save_file          = "resources/levels/main.yaml";

f32 g_player_height               = 2;
f32 g_player_width                = 0.65f;
f32 g_player_eyes_offset          = 0.8f;

f32 g_player_recoil_scale           = 0.5f;
f32 g_player_recoil_gain_per_shot   = 0.2f;
i32 g_player_recoil_min_shots       = 2;

f32 g_player_recoil_shake_frequency = 10;
f32 g_player_recoil_shake_scale     = 0.5f;

f32 g_player_death_cooldown       = 3;
f32 g_player_ground_acceleration  = 240;
f32 g_player_jump_acceleration    = 20;
f32 g_player_ground_drag          = 10;
f32 g_player_air_control          = 4;
f32 g_gravity                     = 70;

v3  g_weapon_display_offset       = v3{0.85, -0.6, 0.9};
f32 g_weapon_switch_cooldown      = 1.5;

f32 g_pickup_weapon_cooldown  = 9;
f32 g_pickup_health_cooldown  = 6;

v4 UI_TIME_BACKGROUND_COLOUR    = v4 {0.053588, 0.082173, 0.147679, 0.67};
f32 UI_TIME_FONT_SIZE           = 35;
f32 UI_TIME_Y_PADDING           = 20;
f32 UI_TIME_BG_WIDTH            = 130;

v4 UI_SCORE_BACKGROUND_COLOUR  = v4 {0.911392, 0.911392, 0.911392, 0.50};
f32 UI_SCORE_FONT_SIZE         = 30;
f32 UI_SCORE_Y_PADDING = 25;
f32 UI_SCORE_START_X_OFFSET = 105;
f32 UI_SCORE_START_Y_OFFSET = 10;
f32 UI_SCORE_GAP = 5;
f32 UI_SCORE_BG_WIDTH = 80;

f32 g_crosshair_gap           = 10;
f32 g_crosshair_length        = 12;
f32 g_crosshair_thickness     = 3;
v4  g_crosshair_colour        = GREEN;

f32 MISSLE_SPEED            = 35;
f32 EXPLOSION_RADIUS        = 12;
f32 EXPLOSION_FORCE         = 60;
f32 EXPLOSION_DAMAGE        = 200;

f32 g_jump_pad_cooldown     = 1;

bool g_debug_draw_owner                 = false;
bool g_debug_draw_no_mesh               = false;
bool g_debug_always_draw_muzzle_flash   = false;

bool g_cheat_weapon_binds     = true;
bool g_cheat_infinite_ammo    = true;
bool g_cheat_no_damage        = false;

f32 g_game_length = Minute(5);
// f32 g_game_length = Second(10);

f32 g_landing_camera_shake_duration = 0.15f;
f32 g_landing_camera_shake_intensity = 0.2f;

v3 g_health_bar_offset                          = v3{0, 1.5, 0};
v4 g_health_bar_health_colour                   = v4 {0.940928, 0.055582, 0.055582, 1.000000};
v4 g_health_bar_decay_colour                    = WHITE;
v4 g_health_bar_empty_colour                    = v4 {0.202532, 0.047001, 0.047001, 1.000000};
f32 g_health_bar_max_magnify_factor             = 3;
f32 g_health_bar_max_magnify_distance           = 70;
i32 g_health_bar_notch_count                    = 25;
f32 g_health_bar_notch_width                    = 0.10f;
f32 g_health_bar_notch_height                   = 0.3f;
f32 g_health_bar_notch_gap                      = 0.0f;
f32 g_health_bar_notch_decay_max_height_factor  = 1.6f;
f32 g_health_bar_notch_decay_max_width_factor   = 1.6f;
f32 g_health_bar_notch_empty_height_factor      = 0.8f;

v4 g_muzzle_flash_colour     = ORANGE;
f32 g_muzzle_flash_intensity = 5;

v4 g_clear_colour           = v4 {0.398013, 0.481982, 0.582278, 1.000000};
v3 g_ambient_light_colour   = v3 {0.189873, 0.189873, 0.189873};
v3 g_sun_colour             = v3 {1, 1, 1};
v3 g_sun_position           = v3 {10, 50, -10};
f32 g_sun_intensity         = 1;

f32 g_particle_lifetime                 = 2.0f;
f32 g_particle_size                     = 0.06f;
f32 g_particle_effect_lifetime          = 0.3f;
f32 g_particle_effect_size              = 0.06f;
i32 g_particle_horizontal_segments      = 6;
i32 g_particle_vertical_segments        = 3;
f32 g_particle_radial_distance          = 1;
f32 g_particle_vertical_distance        = 0.8;
v4 g_particle_player_hit_colour         = RED;
v4 g_particle_enviroment_hit_colour     = ORANGE;
f32 g_particle_max_magnify_factor       = 4;
f32 g_particle_max_magnify_distance     = 50;

meta enum MaterialHandle : u32 {
    MAT_DEFAULT,
    MAT_MUZZLE_FLASH,
    MAT_PARTICLE,
    MAT_METAL_PLATE,
    MAT_BROKEN_BRICK_WALL,
    MAT_METAL_05C,
    MAT_TILES_037,
    MAT_GRID,
    _MAT_COUNT
};

Material *g_materials[_MAT_COUNT] = {};

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
    SH_JUMP,
    SH_STEP_1,
    SH_STEP_2,
    SH_STEP_3,
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
    i32 ammo_count;
    bool automatic;
    f32 firing_cooldown;
    MeshHandle mesh; 
    SoundHandle firing_sound;
    v3 recoil_offset;
    f32 muzzle_flash_size;
    v3 muzzle_flash_offset;
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
        .muzzle_flash_size = 0.4f,
        .muzzle_flash_offset = v3{0, 0.17, 0.46},
        .speed_factor = 0.8,
    },
    Weapon {
        .handle = WH_M4,
        .display_name = "M4",
        .colour = v4 {0.1, 0.1, 0.1, 1},
        .damage = 5,
        .headshot_damage = 15,
        .ammo_count = 35,
        .automatic = true,
        .firing_cooldown = 0.1,
        .mesh = MH_M4,
        .firing_sound = SH_FIRE_SILENCED_GUN_HIGH,
        .recoil_offset = v3{0, -0.01, -0.15},
        .muzzle_flash_size = 0.2f,
        .muzzle_flash_offset = v3{-0.01f, 0.21f, 1.4f},
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
        .muzzle_flash_size = 0.3f,
        .muzzle_flash_offset = v3{0, 0, 0},
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
        .muzzle_flash_size = 0.3f,
        .muzzle_flash_offset = v3{0, 0, 0},
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
    EF_DAMAGEABLE       = 1 << 6,
    EF_DEAD             = 1 << 7,
    EF_PICKUP           = 1 << 8,
    EF_MISSLE           = 1 << 9,
    EF_JUMP_PAD         = 1 << 10,
    EF_COMPLEX_PHYSICS  = 1 << 11,
    EF_POINT_LIGHT      = 1 << 12,
    EF_PARTICLE         = 1 << 13,
    EF_BLOOD_PARTICLE   = 1 << 14,
    EF_SURFACE_PARTICLE = 1 << 15,
    EF_DRAW_MESH        = 1 << 16,
    EF_IGNORE_RAYCAST   = 1 << 17,
    EF_DELETE           = 1 << 24,
};

struct Entity {
    // meta
    u32 flags;
    u32 id;
    f32 time_created;

    // networking
    u32 owner;

    // base
    v3 position;
    v3 size;
    v3 rotation;
    v3 velocity;

    // rendering
    v4 colour;
    MaterialHandle material;

    // flag: damageable
    f32 max_health;
    f32 health;
    f32 death_cooldown; // not saved

    // flag: pickup
    PickupType pickup_type;
    f32 pickup_cooldown; // not saved

    // flag: jump pad
    f32 jump_pad_force;
    f32 jump_pad_cooldown; // not saved

    // flag: point light 
    v4 light_colour;
    f32 light_intensity;
};

struct Team {
    u32 client_id;
    v4 colour;
    i64 score;
};

enum NetworkMessageType {
    NM_ASSIGN_CLIENT_ID,
    NM_CLIENT_CONNECTED,
    NM_SPAWN_ENTITY,
    NM_SYNC_ENTITY,
    NM_DELETE_ENTITY,
    NM_MOVE_PLAYER,
    NM_SHOT_ENTITY,
    NM_CLIENT_DEALT_DAMAGE,
    NM_SET_WEAPON,
    NM_SPAWN_MISSLE,
    NM_NEW_TEAM,
    NM_SYNC_TEAM,
    NM_GAME_COMPLETE,
};

struct NetworkMessage {
    ConnectionId client_id;
    NetworkMessageType type;
    
    union {
        u32             assign_client_id;
        ConnectionId    client_connected;
        Entity          spawn_entity;
        Entity          sync_entity;
        u32             delete_entity;
        struct {
            f32 speed_factor;
            f32 jump;
            v2 input_direction;
        }               move_player;
        struct {
            u32 target_id;
            f32 damage;
        }               shot_entity;
        f32             client_dealt_damage;
        v3              spawn_dummy;
        WeaponHandle    set_weapon;
        Ray             spawn_missle;
        struct {
            i32 red;
            i32 blue;
        }               update_score;
        Team            new_team;
        Team            sync_team;
        //              game_complete
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

// want to be able to apply game effects in relation to time
// 1. over a given T: over T seconds apply this effect
// 2. once a given T: apply this effect immedietly but wait T seconds before applying it again
// .. probably more
struct TimedEffect {
    f32 start_duration;
    f32 remaining_duration;
    f32 intensity;
};

struct TimedEffectState {
    f32 remaining;
    f32 intensity;
    bool active;
};

enum InstanceType {
    IT_CLIENT,
    IT_SERVER
};

// @state
struct State {
    Arena *arena;
    Arena *frame_arena;

    InstanceType instance_type;
    u32 instance_id;
    
    f32 time;
    f32 tick_delta_time;
    f32 frame_delta_time;

    Sampler network_in_sampler;
    Sampler mspt_sampler;

    // actual game state
    bool game_complete;
    StackArray<Team, MAX_TEAMS> teams;
    i64 spawn_point_count;

    WeaponHandle player_weapon;
    i64 player_ammo;
    f32 player_firing_cooldown;
    i64 player_consecutive_shots;
    bool player_duel_wield_switch;

    StackArray<Entity, MAX_ENTITIES> entities;
};

// @server @gameserver
struct GameServer {
    std::thread thread;
    std::atomic<bool> shutdown_signal;

    AtomicSnapshot<Sampler> network_in_sampler_snapshot;
    AtomicSnapshot<Sampler> mspt_sampler_snapshot;

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

    Camera camera;
    Viewport viewport;
    FrameBuffer game_view;

    TimedEffect camera_shake;
    TimedEffect health_bar_decay;
    TimedEffect muzzle_flash;

    State state;
};

// @editor
struct Editor {
    Camera camera;
    Viewport viewport;
    FrameBuffer editor_view;

    Entity *selected_entity;
};

#include "type_info.h"

GameServer *g_game_server = NULL;
GameClient *g_game_client = NULL;
Editor     *g_editor      = NULL;

GameServer *GS();
GameClient *GC();
Editor *ED();

void game_server_start();
void game_server_entry();
void game_client_entry();

void game_server_stop();

void poll_user_input(State *state);
void process_network(State *state);
void sync_clients(State *state);

void game_server_update(State *state);
void game_server_physics(State *state);
void game_server_on_trigger_collision(State *state, Entity *trigger, Entity *other);

void game_client_update(GameClient *client, State *state);
void game_client_draw(GameClient *client, State *state);

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
bool entity_is_grounded(State *state, Entity *entity);
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
Entity *local_spawn_point_light(State *state);
Entity *local_spawn_blood_particle(State *state);

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
void imgui_v2_control(const char *label, v2 *vector, f32 step = 1);
void imgui_v3_control(const char *label, v3 *vector, f32 step = 1);
void imgui_colour_control(const char *label, v4 *colour);
void imgui_colour_control(const char *label, v3 *colour);

template <typename T>
void imgui_enum_dropdown(const char *label, T *value);

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
void play_weapon_fire_sound(Weapon *weapon);

void timed_effect_start(TimedEffect *timed_effect, f32 duration, f32 intensity);
void timed_effect_start_or_accumulate(TimedEffect *timed_effect, f32 duration, f32 intensity);
void timed_effect_tick(TimedEffect *timed_effect, f32 delta_time);
TimedEffectState timed_effect_state(TimedEffect *timed_effect);

// SOURCE: https://easings.net/#
f32 ease_in_sin(f32 x);
f32 ease_out_sin(f32 x);
f32 ease_in_out_sin(f32 x);

f32 ease_in_quad(f32 x);

f32 ease_in_cubic(f32 x);
f32 ease_out_cubic(f32 x);

v2 rotate_point(v2 position, v2 centre, f32 degrees);

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

    g_game_server = new GameServer {};

    // NOTE: doing this stuff before new thread is started
    atomic_snapshot_init(&g_game_server->network_in_sampler_snapshot);
    atomic_snapshot_init(&g_game_server->mspt_sampler_snapshot);
    g_game_server->shutdown_signal = false;

    g_game_server->thread = std::thread(game_server_entry); 
}

// @entrygs @gs
void game_server_entry() {
    log_set_thread_name("server");

    Arena arena = arena_create(MB(5));
    Arena frame_arena = arena_create(MB(5));

    GS()->state = State {
        .arena              = &arena,
        .frame_arena        = &frame_arena,
        .instance_type      = IT_SERVER,
        .instance_id        = SERVER_INSTANCE_ID,
        .entities           = stack_array_create<Entity, MAX_ENTITIES>(),
    };

    Timer tick_timer = timer_create_ms(GAME_MS_PER_TICK);

    Infof("Started game server @ {}tps [thread={}]", i64(1000.0f / f32(GAME_MS_PER_TICK)), get_current_thread_id());

    deserialise_level(&GS()->state);

    while (!GS()->shutdown_signal) {
        f32 tick_delta_time = 0;
        if (!timer_is_complete(&tick_timer, &tick_delta_time)) {
            continue;
        }

        GS()->state.tick_delta_time = tick_delta_time;
        GS()->state.time += GS()->state.tick_delta_time;

        sampler_append(&GS()->state.mspt_sampler, GS()->state.tick_delta_time * 1000.0f);

        if (GS()->state.time >= g_game_length && !GS()->state.game_complete) {
            GS()->state.game_complete = true;

            Infof("Game time limit reached ({}s) game is ending", GS()->state.time);
            NetworkMessage message = NetworkMessage{.type = NM_GAME_COMPLETE};
            server_send_to_all_clients(NET(), bytes_from_ptr(&message));
        }

        process_network(&GS()->state);
        game_server_update(&GS()->state);
        game_server_physics(&GS()->state);
        sync_clients(&GS()->state);

        atomic_snapshot_copy_and_swap(&GS()->network_in_sampler_snapshot, &GS()->state.network_in_sampler);
        atomic_snapshot_copy_and_swap(&GS()->mspt_sampler_snapshot, &GS()->state.mspt_sampler);

        arena_reset(GS()->state.frame_arena);

        std::this_thread::sleep_for(std::chrono::milliseconds(GAME_MS_PER_TICK - 1));
    }

    Log("Game server was given shutdown signal.. stopping");
}

// @entrygc @gc
void game_client_entry() {
    Arena arena = arena_create(MB(5));
    Arena frame_arena = arena_create(MB(5));

    { // init all the global stuff
        bool ok = false;

        ok = window_init("Game12", 1280, 720);
        Assert(ok);

        if (!ok) {
            Log("Failed when trying to init the window");
            return;
        }

        ok = renderer_init(&arena, &frame_arena, WIN(), g_clear_colour, g_ambient_light_colour, g_sun_colour, v3{50, 100, -100}, g_sun_intensity);
        if (!ok) {
            Log("Failed when trying to init the renderer");
            return;
        }

        { // load meshes
            g_meshes[MH_DEAGLE] = mesh_create_from_file(REN(), "resources/models/deagle/deagle.obj");
            Assert(g_meshes[MH_DEAGLE]);
    
            g_meshes[MH_M4] = mesh_create_from_file(REN(), "resources/models/m4/m4.obj");
            Assert(g_meshes[MH_M4]);
    
            g_meshes[MH_CROSS] = mesh_create_from_file(REN(), "resources/models/cross/cross.obj");
            Assert(g_meshes[MH_CROSS]);
        }

        { // load materials
            g_materials[MAT_DEFAULT] = REN()->default_material;
            Assert(g_materials[MAT_DEFAULT]);

            g_materials[MAT_MUZZLE_FLASH] = material_create_unlit(REN(), 
                {1.0, 1.0},
                render_texture_create_from_file(REN(), "resources/textures/muzzle_flash/muzzle_flash.png", TD_RGBA_8, TD_RGBA_8)
            );

            Assert(g_materials[MAT_MUZZLE_FLASH]);

            g_materials[MAT_PARTICLE] = material_create_unlit(REN(), 
                {1.0, 1.0},
                REN()->default_material_albedo
            );

            Assert(g_materials[MAT_PARTICLE]);

            g_materials[MAT_METAL_PLATE] = material_create_pbr(REN(),
                {1, 1},
                render_texture_create_from_file(REN(), "resources/textures/blue_metal_plate/blue_metal_plate_diff_1k.png",      TD_sRGBA_8, TD_sRGBA_8),
                render_texture_create_from_file(REN(), "resources/textures/blue_metal_plate/blue_metal_plate_nor_gl_1k.png",    TD_RGBA_8, TD_RGBA_8),
                render_texture_create_from_file(REN(), "resources/textures/blue_metal_plate/blue_metal_plate_ao_1k.png",        TD_RGBA_8, TD_RGBA_8),
                render_texture_create_from_file(REN(), "resources/textures/blue_metal_plate/blue_metal_plate_rough_1k.png",     TD_RGBA_8, TD_RGBA_8),
                REN()->default_material_metalness
            );

            Assert(g_materials[MAT_METAL_PLATE]);

            g_materials[MAT_BROKEN_BRICK_WALL] = material_create_pbr(REN(), 
                {1, 1},
                render_texture_create_from_file(REN(), "resources/textures/broken_brick_wall/broken_brick_wall_diff_1k.png",    TD_sRGBA_8, TD_sRGBA_8),
                render_texture_create_from_file(REN(), "resources/textures/broken_brick_wall/broken_brick_wall_nor_gl_1k.png",  TD_RGBA_8, TD_RGBA_8),
                render_texture_create_from_file(REN(), "resources/textures/broken_brick_wall/broken_brick_wall_ao_1k.png",      TD_RGBA_8, TD_RGBA_8),
                render_texture_create_from_file(REN(), "resources/textures/broken_brick_wall/broken_brick_wall_rough_1k.png",   TD_RGBA_8, TD_RGBA_8),
                REN()->default_material_metalness
            );

            Assert(g_materials[MAT_BROKEN_BRICK_WALL]);

            g_materials[MAT_METAL_05C] = material_create_pbr(REN(), 
                {1, 1},
                render_texture_create_from_file(REN(), "resources/textures/metal05C/Metal050C_1K-PNG_Color.png",        TD_sRGBA_8, TD_sRGBA_8),
                render_texture_create_from_file(REN(), "resources/textures/metal05C/Metal050C_1K-PNG_NormalGL.png",     TD_RGBA_8, TD_RGBA_8),
                REN()->default_material_ambient_occlusion,
                render_texture_create_from_file(REN(), "resources/textures/metal05C/Metal050C_1K-PNG_Roughness.png",    TD_RGBA_8, TD_RGBA_8),
                render_texture_create_from_file(REN(), "resources/textures/metal05C/Metal050C_1K-PNG_Metalness.png",    TD_RGBA_8, TD_RGBA_8)
            );

            Assert(g_materials[MAT_METAL_05C]);

            g_materials[MAT_TILES_037] = material_create_pbr(REN(), 
                {1, 1},
                render_texture_create_from_file(REN(), "resources/textures/Tiles037/Tiles037_2K-PNG_Color.png",         TD_sRGBA_8, TD_sRGBA_8),
                render_texture_create_from_file(REN(), "resources/textures/Tiles037/Tiles037_2K-PNG_NormalGL.png",      TD_RGBA_8, TD_RGBA_8),
                REN()->default_material_ambient_occlusion,
                render_texture_create_from_file(REN(), "resources/textures/Tiles037/Tiles037_2K-PNG_Roughness.png",     TD_RGBA_8, TD_RGBA_8),
                render_texture_create_from_file(REN(), "resources/textures/defaults/default_metalness.png",             TD_RGBA_8, TD_RGBA_8)
            );

            Assert(g_materials[MAT_TILES_037]);

            g_materials[MAT_GRID] = material_create_pbr(REN(), 
                {1, 1},
                render_texture_create_from_file(REN(), "resources/textures/grid/albedo.png", TD_sRGBA_8, TD_sRGBA_8),
                REN()->default_material_normal,
                REN()->default_material_ambient_occlusion,
                REN()->default_material_roughness,
                REN()->default_material_metalness
            );

            Assert(g_materials[MAT_GRID]);
        }

        ok = sound_engine_init();
        Assert(ok);

        if (!ok) {
            Log("Failed when trying to init the sound engine");
            return;
        }

        { // load sounds
            g_sounds[SH_FIRE_DEAGLE] = sound_engine_load(SE(), "resources/sounds/deagle_fire.wav");
            Assert(g_sounds[SH_FIRE_DEAGLE]);
    
            g_sounds[SH_FIRE_SILENCED_GUN_HIGH] = sound_engine_load(SE(), "resources/sounds/silenced_gun_high.wav");
            Assert(g_sounds[SH_FIRE_SILENCED_GUN_HIGH]);
    
            g_sounds[SH_FIRE_SILENCED_GUN_MID] = sound_engine_load(SE(), "resources/sounds/silenced_gun_mid.wav");
            Assert(g_sounds[SH_FIRE_SILENCED_GUN_MID]);
    
            g_sounds[SH_FIRE_SILENCED_GUN_LOW] = sound_engine_load(SE(), "resources/sounds/silenced_gun_low.wav");
            Assert(g_sounds[SH_FIRE_SILENCED_GUN_LOW]);
    
            g_sounds[SH_TARGET_HIT] = sound_engine_load(SE(), "resources/sounds/target_hit.wav");
            Assert(g_sounds[SH_TARGET_HIT]);
    
            g_sounds[SH_HEADSHOT_HIT] = sound_engine_load(SE(), "resources/sounds/headshot_hit.wav");
            Assert(g_sounds[SH_HEADSHOT_HIT]);
    
            g_sounds[SH_JUMP] = sound_engine_load(SE(), "resources/sounds/jump.wav");
            Assert(g_sounds[SH_JUMP]);
    
            g_sounds[SH_STEP_1] = sound_engine_load(SE(), "resources/sounds/step_1.wav");
            Assert(g_sounds[SH_STEP_1]);
    
            g_sounds[SH_STEP_2] = sound_engine_load(SE(), "resources/sounds/step_2.wav", 5);
            Assert(g_sounds[SH_STEP_2]);
    
            g_sounds[SH_STEP_3] = sound_engine_load(SE(), "resources/sounds/step_3.wav");
            Assert(g_sounds[SH_STEP_3]);
        }
    }

    g_game_client = new GameClient {
        .mode = GC_EDITOR,
        .camera = camera_create(CameraMode::FIRST_PERSON, 90, v3{0, 10, 0}, 0.1, 300),
        .viewport =  Viewport {
            .focused = false,
            .size = WIN()->frame_buffer_size
        },
        .game_view = FrameBuffer {.size = WIN()->frame_buffer_size},
        .state = State {
            .arena = &arena,
            .frame_arena = &frame_arena,
            .instance_type = IT_CLIENT,
            .instance_id = 0,
            .entities = stack_array_create<Entity, MAX_ENTITIES>(),
        }
    };

    g_editor = new Editor {
        .camera = camera_create(CameraMode::FIRST_PERSON, 90, v3{0, 15, -20}, 0.1, 300),
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
            Fatal("failed to init game view frame buffer");
            return;
        }

        ok = frame_buffer_init(&g_editor->editor_view);
        if (!ok) {
            Fatal("failed to init editor view frame buffer");
            return;
        }
    }

    Timer tick_timer = timer_create_ms(GAME_MS_PER_TICK);
    Timer frame_timer = timer_create_ms(GAME_MS_PER_FRAME);
    Stopwatch frame_stopwatch = stopwatch_create();

    Infof("Started game client @ {}tps [thread={}]", i64(1000.0f / f32(GAME_MS_PER_TICK)), get_current_thread_id());

    deserialise_level(&GC()->state);
    ED()->camera.position = {0, 2.4, -5};

    while (!glfwWindowShouldClose(WIN()->glfw_window)) {
        GC()->state.frame_delta_time = stopwatch_get_time_and_reset(&frame_stopwatch);

        f32 tick_delta_time = 0;

        if (timer_is_complete(&tick_timer, &tick_delta_time)) {
            GC()->state.tick_delta_time = tick_delta_time;

            sampler_append(&GC()->state.mspt_sampler, GC()->state.tick_delta_time * 1000.0f);

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
                GC()->state.time += GC()->state.tick_delta_time;
                process_network(&GC()->state);
                game_client_update(GC(), &GC()->state);
            }
        }

        f32 frame_delta_time = 0;

        if (timer_is_complete(&frame_timer, &frame_delta_time)) {
            GC()->state.frame_delta_time = frame_delta_time;

            renderer_start_frame(REN());
    
            game_client_draw(GC(), &GC()->state);
    
            renderer_draw_frame(REN(), &ED()->camera, ED()->viewport, &ED()->editor_view, false);
            renderer_draw_frame(REN(), &GC()->camera, GC()->viewport, &GC()->game_view, true);
    
            renderer_end_frame(REN());
    
            editor_draw_ui(&GC()->state);
            swap_buffers(WIN());
        }

        arena_reset(GC()->state.frame_arena);
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

void process_network(State *state) {
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
            on_server_receive(state, message, state->tick_delta_time);
            slice_free(bytes);
        }
    }
}

void sync_clients(State *state) {
    Assert(is_server(state));

    for (Team &team : state->teams) {
        NetworkMessage message = NetworkMessage{.type = NM_SYNC_TEAM, .sync_team = team};
        server_send_to_all_clients(NET(), bytes_from_ptr(&message));
    }

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

void game_server_update(State *state) {
    Assert(is_server(state));

    for (Entity &entity : state->entities) {
        if (BitSet(entity.flags, EF_PICKUP)) {
            entity.pickup_cooldown -= state->tick_delta_time;
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
                            entity.pickup_cooldown = g_pickup_weapon_cooldown;
                            NetworkMessage message = NetworkMessage{.type = NM_SET_WEAPON, .set_weapon = WH_M4};
                            server_send_to_client(NET(), bytes_from_ptr(&message), other.owner);
                        } break;
                        case PT_TAP: {
                            entity.pickup_cooldown = g_pickup_weapon_cooldown;
                            NetworkMessage message = NetworkMessage{.type = NM_SET_WEAPON, .set_weapon = WH_TAP};
                            server_send_to_client(NET(), bytes_from_ptr(&message), other.owner);
                        } break;
                        case PT_PAL: {
                            entity.pickup_cooldown = g_pickup_weapon_cooldown;
                            NetworkMessage message = NetworkMessage{.type = NM_SET_WEAPON, .set_weapon = WH_PAL};
                            server_send_to_client(NET(), bytes_from_ptr(&message), other.owner);
                        } break;
                        case PT_HEALTH: {
                            entity.pickup_cooldown = g_pickup_health_cooldown;
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
            entity.jump_pad_cooldown -= state->tick_delta_time;
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
                
                other.velocity.y = entity.jump_pad_force;
                entity.jump_pad_cooldown += g_jump_pad_cooldown;

                // means only one player per cooldown can be effected?
                break;
            }
        }

        if (BitSet(entity.flags, EF_DEAD)) {
            Assert(BitSet(entity.flags, EF_DAMAGEABLE));

            entity.death_cooldown -= state->tick_delta_time;

            // cool down is over so now reset entity back to "non-dead" state
            if (entity.death_cooldown < 0) {
                UnsetBit(entity.flags, EF_DEAD);
                entity.death_cooldown = 0;

                entity.health = entity.max_health;
                entity.velocity = {};

                // probably best to move this respawn logic to where the player handles it but
                // for now this is the only entity that needs it so its fine
                if (BitSet(entity.flags, EF_PLAYER)) {
                    NetworkMessage message = NetworkMessage{.type = NM_SET_WEAPON, .set_weapon = WH_DEAGLE};
                    server_send_to_client(NET(), bytes_from_ptr(&message), entity.owner);

                    move_to_random_spawn_point(state, &entity);
                }
            }
        }

        if (BitSet(entity.flags, EF_DAMAGEABLE)) {
            if (g_cheat_no_damage) {
                entity.health = entity.max_health;
            }

            if (entity.health <= 0 && entity.death_cooldown == 0) {
                entity.death_cooldown = g_player_death_cooldown;
                entity.health = 0;
                SetBit(entity.flags, EF_DEAD);
            }
        }
    }
}

void game_server_physics(State *state) {
    Assert(is_server(state));

    for (Entity &entity : state->entities) {
        if (BitSet(entity.flags, EF_STATIC_HITBOX)) {
            continue;
        }

        if (!BitSet(entity.flags, EF_SOLID_HITBOX) && !BitSet(entity.flags, EF_TRIGGER_HITBOX)) {
            continue;
        }

        if (BitSet(entity.flags, EF_COMPLEX_PHYSICS)) {
            if (entity_is_grounded(state, &entity)) {
                v3 h_velocity = v3{entity.velocity.x, 0, entity.velocity.z};
                v3 drag = -h_velocity * g_player_ground_drag;

                entity.velocity.x += drag.x * state->tick_delta_time;
                entity.velocity.z += drag.z * state->tick_delta_time;
            }

            entity.velocity.y -= g_gravity * state->tick_delta_time;
        }

        v3 starting_position = entity.position;
        entity.position += entity.velocity * state->tick_delta_time;

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

            if (!BitSet(other.flags, EF_DAMAGEABLE)) {
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

void game_client_update(GameClient *client, State *state) {
    timed_effect_tick(&client->camera_shake, state->tick_delta_time);
    timed_effect_tick(&client->health_bar_decay, state->tick_delta_time);
    timed_effect_tick(&client->muzzle_flash, state->tick_delta_time);

    { // player state cooldowns
        state->player_firing_cooldown -= state->tick_delta_time;
        if (state->player_firing_cooldown <= 0) {
            state->player_firing_cooldown = 0;
        }
    }

    { // weapon reaload and switching to default
        Assert(state->player_ammo >= 0);

        if (g_cheat_infinite_ammo) {
            state->player_ammo = g_weapons[state->player_weapon].ammo_count;
        }

        if (state->player_ammo == 0) {
            set_player_weapon(state, WH_DEAGLE, g_weapon_switch_cooldown);
        }
    }

    Entity *player = get_client_player(state, state->instance_id);
    if (player != NULL) {
        client->camera.position = player->position + v3{0, g_player_eyes_offset, 0};
    }

    { // camera shake
        // camera shake works by offesting the camera on the y axis over time,
        // the extent of the offset is the intensity in world units.
        // The shake is split into two phases, for the first half of the duration
        // the position is offset more and more until it has reached the peak offset
        // then for the second half of the duration it is decreased over time and eventually
        // is the same as the orignial camera positon therefore making the transition from
        // "active" camera shake to normal, smooth
        // - 03/09/25
        auto [remaining, intensity, active] = timed_effect_state(&GC()->camera_shake);

        if (active) {
            f32 y_offset = 0;
       
            if (remaining >= 0.5f) {
                f32 time = (remaining - 0.5f) * 2;  // 1->0.5 to 1->0
                time = 1 - time;                    // 1->0 to 0->1
                time = ease_in_out_sin(time); // 0->1 to 0~>1
                    
                y_offset = intensity * time;
            }
            else {
                f32 time = remaining * 2; // 0.5->0 to 1->0

                y_offset = intensity * time;
            }

            client->camera.position.y -= y_offset;
        }
    }

    // camera vertical recoil
    if (state->player_consecutive_shots > 0 && state->player_consecutive_shots > g_player_recoil_min_shots) {
        Weapon *player_weapon = get_player_weapon(state);

        f32 firing_t = f32(state->player_consecutive_shots) / f32(player_weapon->ammo_count);
        firing_t = clamp(0, firing_t, 1);
        firing_t = max(0.3f, firing_t);

        f32 new_recoil = (g_player_recoil_gain_per_shot * g_player_recoil_scale) * ease_in_sin(firing_t);
        client->camera.rotation.x += new_recoil;
    }

    // camera recoil shake
    if (state->player_consecutive_shots > 0) {
        Weapon *player_weapon = get_player_weapon(state);

        f32 firing_t = f32(state->player_consecutive_shots) / f32(player_weapon->ammo_count);

        FastNoiseLite noise = {};
        noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        noise.SetFrequency(g_player_recoil_shake_frequency);

        f32 sample = noise.GetNoise(firing_t, 0.0f);
        sample = ease_out_cubic(sample);
        sample *= g_player_recoil_shake_scale;

        client->camera.rotation.z = sample;

    } else {
        client->camera.rotation.z = 0;
    }

    // update client owned player 
    if (client->viewport.focused && player != NULL && !BitSet(player->flags, EF_DEAD)) {
        if (WIN()->mouse_captured) {
            f32 sensitivity = 3;
            v2 mouse_input = MOUSE.delta;
    
            // camera control
            if (length(mouse_input) > 0) {
                client->camera.rotation += v3{mouse_input.y, -mouse_input.x, 0} * sensitivity * state->tick_delta_time;
                client->camera.rotation.x = clamp(-90, client->camera.rotation.x, 90);
            }

            // shooting
            Weapon *player_weapon = get_player_weapon(state);
            InputState state_needed = player_weapon->automatic ? InputState::PRESSED : InputState::DOWN;

            bool player_can_shoot = false;
            bool player_did_shoot = false;

            if (state->player_ammo > 0 && state->player_firing_cooldown <= 0) {
                player_can_shoot = true;
            }

            if (MOUSE.buttons[GLFW_MOUSE_BUTTON_1] == state_needed) {
                player_did_shoot = true;
            }

            if (player_can_shoot) {
                if (player_did_shoot) {
                    state->player_consecutive_shots += 1;
                }
                else {
                    state->player_consecutive_shots = 0;
                }
            }

            if (player_can_shoot && player_did_shoot) {
                state->player_ammo -= 1; 
                state->player_firing_cooldown = player_weapon->firing_cooldown;
                state->player_duel_wield_switch = !state->player_duel_wield_switch;

                play_weapon_fire_sound(player_weapon);
                timed_effect_start_or_accumulate(&client->muzzle_flash, 0.05, 1);

                if (state->player_weapon != WH_TAP) {
                    v3 ray_direction = get_forward_direction(&GC()->camera);
                
                    Ray ray = ray_create(GC()->camera.position, ray_direction);
                    RaycastIterator it = raycast_iterator_create(ray, GC()->camera.far_plane - GC()->camera.near_plane);
                
                    RaycastIteratorResult result = {};
                
                    while (true) {
                        result = next(&it, state);
                
                        if (result.entity == NULL) {
                            break;
                        }
                
                        if (result.entity->owner == state->instance_id) {
                            continue;
                        }
                
                        break;
                    }
                
                    if (result.entity) {
                        { // spawn bullet particle effect
                            Entity *particle = local_spawn_blood_particle(state);
                            particle->position = result.hit_position;
                
                            if (BitSet(result.entity->flags, EF_DAMAGEABLE)) {
                                SetBit(particle->flags, EF_BLOOD_PARTICLE);
                            }
                            else {
                                SetBit(particle->flags, EF_SURFACE_PARTICLE);
                            }
                        }
                
                        if (BitSet(result.entity->flags, EF_DAMAGEABLE)) {
                            f32 hit_height_offset = result.hit_position.y - result.entity->position.y;
                            f32 half_head_size = (g_player_height * 0.5)  - g_player_eyes_offset;
                    
                            f32 damage              = {};
                            SoundHandle hit_sound   = {};
                    
                            // headshot
                            if (hit_height_offset >= g_player_eyes_offset - half_head_size) {
                                damage = player_weapon->headshot_damage;
                                hit_sound = SH_HEADSHOT_HIT;
                            }
                            // body shot
                            else {
                                damage = player_weapon->damage;
                                hit_sound = SH_TARGET_HIT;
                            }
                    
                            sound_engine_play(g_sounds[hit_sound]);
                    
                            NetworkMessage message = NetworkMessage {
                                .client_id = state->instance_id, 
                                .type = NM_SHOT_ENTITY, 
                                .shot_entity = {
                                    .target_id = result.entity->id,
                                    .damage = damage 
                                } 
                            };
                    
                            client_send_to_server(NET(), bytes_from_ptr(&message));
                        }
                    }
                }
                else {
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
            }

            // cheats to give weapons 
            if (g_cheat_weapon_binds && client->viewport.focused) {
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
            v3 forward = get_forward_direction(&client->camera);
            v3 right = get_right_direction(&client->camera);
   
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

        static bool s_grounded_last_tick = true;
        bool grounded_this_tick = entity_is_grounded(state, player);

        if (grounded_this_tick) {
            // jumping sounds
            if (keyboard_input.y == 1) {
                sound_engine_play(g_sounds[SH_JUMP]);
            }

            // landing sounds
            if (!s_grounded_last_tick) {
                sound_engine_play(g_sounds[SH_STEP_2]);
                timed_effect_start(&client->camera_shake, g_landing_camera_shake_duration, g_landing_camera_shake_intensity);
            }

            // footstep sounds
            if (keyboard_input.x != 0 || keyboard_input.z != 0) {
                constexpr f32 FOOTSTEP_SOUND_DELAY = 0.3;
        
                static array<SoundHandle, 2> s_step_sounds = {SH_STEP_1, SH_STEP_3};
                static f32 s_footstep_sound_cooldown = 0;
                static i64 s_next_sound = 0;
        
                s_footstep_sound_cooldown -= state->tick_delta_time;
                if (s_footstep_sound_cooldown < 0) {
                    s_footstep_sound_cooldown = 0; 
                }
        
                if (s_footstep_sound_cooldown == 0) {
                    SoundHandle sound = s_step_sounds[s_next_sound];
                    sound_engine_play(g_sounds[sound]);
     
                    s_footstep_sound_cooldown += FOOTSTEP_SOUND_DELAY;
     
                    s_next_sound += 1;
                    if (s_next_sound >= s_step_sounds.len) {
                        s_next_sound = 0;
                    }
                }
            }
        }

        s_grounded_last_tick = grounded_this_tick;
    }

    // remove any temperaory entities made for effects
    i64 index = 0;
    while (index < state->entities.len) {
        Entity &entity = state->entities[index];

        if (BitSet(entity.flags, EF_PARTICLE)) {
            if (state->time - entity.time_created > g_particle_lifetime) {
                local_delete_entity(state, entity.id);
                continue;
            }
        }

        index++;
    }
}

void game_client_draw(GameClient *client, State *state) {
    Assert(is_client(state));

    { // top bar ui
        v3 time_bg_size = v3{UI_TIME_BG_WIDTH, UI_TIME_FONT_SIZE + UI_TIME_Y_PADDING, 0};
        v3 time_centre = v3{time_bg_size.x * 0.5f, client->viewport.size.y - (time_bg_size.y * 0.5f)};
        
        { // time
            i32 target_seconds = g_game_length;
            i32 total_seconds = state->time;
            i32 remaining_seconds = target_seconds - total_seconds;

            i32 minutes = remaining_seconds / 60;
            i32 seconds = remaining_seconds % 60;

            if (minutes < 0) minutes = 0;
            if (seconds < 0) seconds = 0;

            string time_string = fmt(state->frame_arena, "{}:{}", minutes, seconds);

            draw_rectangle_ui(REN(), v3{time_centre.x, time_centre.y, UI_LAYER_1}, time_bg_size.xy, {}, UI_TIME_BACKGROUND_COLOUR);
            draw_text_ui(REN(), time_string, v3{time_centre.x, time_centre.y, UI_LAYER_0}, UI_TIME_FONT_SIZE, WHITE, true);
        }

        v3 score_bg_size = v3{UI_SCORE_BG_WIDTH, UI_SCORE_FONT_SIZE + UI_SCORE_Y_PADDING, 1};
        v3 score_centre_begin = time_centre + v3{UI_SCORE_START_X_OFFSET, UI_SCORE_START_Y_OFFSET, 0};

        for (i64 i = 0; i < state->teams.len; i++) {
            Team *team = &state->teams[i];

            v3 score_centre = score_centre_begin;
            score_centre.x += (score_bg_size.x + UI_SCORE_GAP) * f32(i);

            string score_text = fmt(state->frame_arena, "{}", team->score);

            draw_rectangle_ui(REN(), v3{score_centre.x, score_centre.y, UI_LAYER_1}, score_bg_size.xy, {}, UI_SCORE_BACKGROUND_COLOUR);
            draw_text_ui(REN(), score_text, v3{score_centre.x, score_centre.y, UI_LAYER_0}, UI_SCORE_FONT_SIZE, team->colour, true);

            if (team->client_id == state->instance_id) {
                v3 indicator_centre = v3{score_centre.x, score_centre.y - ((score_bg_size.y * 0.5f) + 12), score_centre.z};
                draw_circle_ui(REN(), indicator_centre, {8, 8}, {}, team->colour);
            }
        }
    }

    // game complete screen
    if (state->game_complete) { 
        v3 top_right = relative_to_screen_position(client->viewport, {1, 1});
        v3 centre = top_right * 0.5;
        v2 size = top_right.xy;

        draw_rectangle_ui(REN(), centre, size, {}, alpha(BLUE, 0.6));
    }

    for (Entity &entity : state->entities) {
        v4 draw_colour = entity.colour;

        if (!BitSet(entity.flags, EF_DRAW_MESH)) {
            draw_colour = HOT_PINK;
        }

        // client's player
        if (BitSet(entity.flags, EF_PLAYER) && entity.owner == state->instance_id) {
            Weapon *player_weapon = get_player_weapon(state);

            { // draw weapon
                if (player_weapon->handle == WH_PAL) {
                    draw_player_weapon(state, player_weapon, g_weapon_display_offset, state->player_duel_wield_switch);
                    draw_player_weapon(state, player_weapon, g_weapon_display_offset * v3{-1, 1, 1}, !state->player_duel_wield_switch);
                }
                else {
                    draw_player_weapon(state, player_weapon, g_weapon_display_offset, true);
                }
            }

            // @hud

            // draw fire cooldown when using non auto gun
            if (!player_weapon->automatic && state->player_firing_cooldown > 0) { 
                f32 max_width = 40;
                f32 height = 3;

                v3 centre = relative_to_screen_position(client->viewport, {0.5, 0.5});
                v3 centre_offset = v3{0, -50, 0};

                f32 cooldown_scale = state->player_firing_cooldown / player_weapon->firing_cooldown;

                draw_rectangle_ui(REN(), centre + centre_offset, {max_width * cooldown_scale, height}, {}, alpha(BLACK, 0.3));
            }
       
            { // draw crosshair
                v3 centre = relative_to_screen_position(client->viewport, {0.5, 0.5});

                // horizontal
                draw_rectangle_ui(REN(), centre - v3{g_crosshair_gap, 0, 0}, {g_crosshair_length, g_crosshair_thickness}, {}, g_crosshair_colour);
                draw_rectangle_ui(REN(), centre + v3{g_crosshair_gap, 0, 0}, {g_crosshair_length, g_crosshair_thickness}, {}, g_crosshair_colour);

                // vertical
                draw_rectangle_ui(REN(), centre - v3{0, g_crosshair_gap, 0}, {g_crosshair_thickness, g_crosshair_length}, {}, g_crosshair_colour);
                draw_rectangle_ui(REN(), centre + v3{0, g_crosshair_gap, 0}, {g_crosshair_thickness, g_crosshair_length}, {}, g_crosshair_colour);
            }

            { // draw health
                f32 max_width = 600;
                f32 height = 30;
                v3 centre = relative_to_screen_position(client->viewport, {0.5, 0.01});

                f32 health_scale = entity.health / entity.max_health;

                draw_rectangle_ui(REN(), centre, {max_width * health_scale, height}, {}, brightness(RED, 0.8));
                draw_rectangle_ui(REN(), centre, {max_width, height}, {}, brightness(RED, 0.4));
            }

            // blood overlay when dead 
            if (BitSet(entity.flags, EF_DEAD)) {
                v3 top_right = relative_to_screen_position(client->viewport, {1, 1});
                v3 centre = top_right * 0.5;
                v2 size = top_right.xy;

                draw_rectangle_ui(REN(), centre, size, {}, alpha(RED, 0.3));
            }
       
            { // draw ammo
                draw_text_ui(REN(), fmt(state->frame_arena, "{}:  {}", player_weapon->display_name, state->player_ammo), {7, 10, 0}, 30, alpha(BLACK, 0.4), false);
            }

            continue;
        }

        // every other player
        if (BitSet(entity.flags, EF_PLAYER)) {
            { // draw head
                // want to draw a second cube where the head will be, centre of this cube is eye position
                f32 eyes_to_top = (g_player_height * 0.5)  - g_player_eyes_offset;
                v3 head_size = v3{g_player_width, eyes_to_top * 2, g_player_width};
    
                // add a little extra to stop z fighting
                head_size += v3{0.01, 0.01, 0.01};
    
                draw_cube(REN(), entity.position + v3{0, g_player_eyes_offset, 0}, head_size, entity.rotation, BEIGE, REN()->default_material);
            }
        }

        if (BitSet(entity.flags, EF_DAMAGEABLE)) {
            if (BitSet(entity.flags, EF_DEAD)) {
                draw_colour = mix(draw_colour, RED, 0.65);
            }

            { // neo health bar
                v3 health_bar_centre = entity.position + g_health_bar_offset;
                v3 camera_direction = client->camera.position - health_bar_centre;
                v3 camera_direction_n = norm(camera_direction);

                f32 pitch = camera_direction_n.y;
                f32 yaw = camera_direction_n.x;
                v3 notch_rotation = v3{pitch * -90.0f, 0, 0};

                if (camera_direction_n.z <= 0) {
                    notch_rotation.y = yaw * 90.0f;
                }
                else {
                    notch_rotation.y = (sign(yaw) * 180) - (yaw * 90);
                }

                f32 camera_distance = length(camera_direction);
                camera_distance = clamp(0.1, camera_distance, g_health_bar_max_magnify_distance);
                f32 magnification_factor = max(1, (camera_distance / g_health_bar_max_magnify_distance) * g_health_bar_max_magnify_factor);

                f32 notch_width     = g_health_bar_notch_width * magnification_factor;
                f32 notch_height    = g_health_bar_notch_height * magnification_factor;
                f32 notch_gap       = g_health_bar_notch_gap * magnification_factor;

                f32 total_health_bar_width = 0;
                total_health_bar_width += f32(g_health_bar_notch_count) * notch_width;
                total_health_bar_width += f32(g_health_bar_notch_count - 1) * notch_gap;

                // health bar is split into three sections
                // [HHHHHH-DDDDD-EEEEEE]
                // H - "Health" notch, the part of the health bar which signifies health remaining
                // D - "Decay"  notch, the part of the health bar which signifies health just removed
                // E - "Empty"  notch, the part of the health bar which signifies no health 

                f32 health = entity.health;
                f32 max_health = entity.max_health;
                f32 health_per_notch = max_health / f32(g_health_bar_notch_count);

                i32 last_health_notch = 0;

                {
                    f32 filled_notches = health / health_per_notch;
                    last_health_notch  = i32(ceilf(filled_notches) - 1);
                }

                TimedEffectState decay = {};
                i32 last_decay_notch = 0;

                { 
                    decay = timed_effect_state(&client->health_bar_decay);
                    if (!decay.active) {
                        last_decay_notch = -1;
                    }
    
                    f32 filled_and_decayed_notches = (health + decay.intensity) / health_per_notch;
                    last_decay_notch = i32(ceilf(filled_and_decayed_notches) - 1);
                }

                for (i32 i = 0; i < g_health_bar_notch_count; i++) {
                    v3 notch_offset = {};                                   // offset from health bar centre to notch centre
                    notch_offset.x += f32(i) * notch_width;                 // shift by its width
                    notch_offset.x += f32(i) * notch_gap;                   // shift by the gap
                    notch_offset.x += notch_width * 0.5;                    // because drawing is from centre, shoft over by half width so it is totally in the bar width
                    notch_offset.x -= total_health_bar_width * 0.5;         // shift total bar width so all notches are centred on the bar offset
                  
                    v4 notch_colour = {};
                    f32 notch_height_factor = 0;
                    f32 notch_width_factor = 0;

                    if (i <= last_health_notch) {
                        notch_colour = g_health_bar_health_colour;
                        notch_height_factor = 1;
                        notch_width_factor = 1;
                    }
                    else if (i <= last_decay_notch) {
                        f32 i_remaining = 1 - decay.remaining;

                        notch_colour = mix(g_health_bar_health_colour, g_health_bar_decay_colour, i_remaining);
                        notch_height_factor = 1 + (i_remaining * (g_health_bar_notch_decay_max_height_factor - 1));
                        notch_width_factor = 1 + (i_remaining * (g_health_bar_notch_decay_max_width_factor - 1));
                    }
                    else {
                        notch_colour = g_health_bar_empty_colour;
                        notch_height_factor = g_health_bar_notch_empty_height_factor;
                        notch_width_factor = 1;
                    }

                    v3 notch_position = health_bar_centre + notch_offset;

                    v2 rotated = rotate_point(v2{notch_position.x, notch_position.z}, v2{health_bar_centre.x, health_bar_centre.z}, notch_rotation.y);
                    notch_position.x = rotated.x;
                    notch_position.z = rotated.y;

                    draw_quad(REN(), notch_position, {notch_width * notch_width_factor, notch_height * notch_height_factor}, notch_rotation, notch_colour, REN()->default_material);
                }
            }
        }

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

            draw_mesh(REN(), mesh, pickup_position, pickup_size, pickup_rotation, pickup_colour, REN()->default_material);
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

                draw_sphere(REN(), entity.position + trail_offset, trail_radius * (1.0f - t), trail_colour, REN()->default_material);
            }
        }

        if (BitSet(entity.flags, EF_POINT_LIGHT)) {
            draw_point_light(REN(), entity.position, entity.light_colour, entity.light_intensity);
        }

        if (BitSet(entity.flags, EF_PARTICLE)) {
            Material *particle_material = g_materials[MAT_PARTICLE];

            f32 alivetime = state->time - entity.time_created;
            f32 effect_t = alivetime / g_particle_effect_lifetime;

            f32 h_degrees_per_particle = 360.0f / f32(g_particle_horizontal_segments);
            f32 v_degrees_per_particle = 180.0f / f32(g_particle_vertical_segments);

            v3 camera_direction = GC()->camera.position - entity.position;
            f32 camera_distance = length(camera_direction);
            camera_distance = clamp(0.1, camera_distance, g_particle_max_magnify_distance);
            f32 magnification_factor = max(1, (camera_distance / g_particle_max_magnify_distance) * g_particle_max_magnify_factor);

            bool is_blood_particle = BitSet(entity.flags, EF_BLOOD_PARTICLE);

            v4 particle_effect_colour = is_blood_particle ? g_particle_player_hit_colour : g_particle_enviroment_hit_colour;

            if (alivetime <= g_particle_effect_lifetime) {
                for (i64 h = 0; h < g_particle_horizontal_segments; h++) {
                    v2 h_direction = rotate_point({0, 1}, {0, 0}, h_degrees_per_particle * f32(h));
    
                    for (i64 v = 0; v < g_particle_vertical_segments; v++) {
                        v2 v_direction = rotate_point({0, 1}, {0, 0}, v_degrees_per_particle * f32(v));
    
                        v3 particle_direction = norm(v3{h_direction.x, v_direction.y, h_direction.y});
    
                        v3 particle_position = entity.position;
                        particle_position += (particle_direction * g_particle_radial_distance) * ease_out_cubic(effect_t) * min(1.5f, magnification_factor);
    
                        f32 particle_size = g_particle_effect_size;
                        particle_size *= 1.0f - ease_out_cubic(effect_t);
    
                        draw_sphere(REN(), particle_position, particle_size * magnification_factor, particle_effect_colour, particle_material);
                    }
                }
            }

            if (!is_blood_particle) {
                if (alivetime <= 0.04) {
                    v3 light_position = entity.position;
                    light_position += norm(camera_direction) * 0.5f;
    
                    draw_point_light(REN(), light_position, ORANGE, 0.5 * magnification_factor);
                }
    
                draw_sphere(REN(), entity.position, g_particle_size * min(1.5f, magnification_factor), BLACK, particle_material);
            }
        }

        if (ED()->selected_entity && ED()->selected_entity->id == entity.id) {
            draw_colour = RED;
        }

        if (g_debug_draw_owner) {
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

        if (BitSet(entity.flags, EF_DRAW_MESH)) {
            draw_cube(REN(), entity.position, entity.size, entity.rotation, draw_colour, g_materials[entity.material]);
        }
        else if (g_debug_draw_no_mesh) {
            draw_sphere(REN(), entity.position, entity.size.x * 0.5, HOT_PINK, g_materials[MAT_DEFAULT]);
        }
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
                camera->rotation += v3{mouse_input.y, -mouse_input.x, 0} * sensitivity;
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

        f32 speed = 0.5f;
        
        camera->position += movement * speed;
    }
}

void editor_draw_ui(State *state) {
    new_imgui_frame();

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), 0);

    // https://github.com/ocornut/imgui/blob/master/imgui_demo.cpp
    // ImGui::ShowDemoWindow();

    // https://github.com/epezent/implot/blob/master/implot_demo.cpp
    // ImPlot::ShowDemoWindow();

    if (true) {
        ImGui::Begin("Style");

        { // timer card
            ImGui::PushID("timer_style");
    
            ImGui::SeparatorText("Timer");
            imgui_colour_control("Background", &UI_TIME_BACKGROUND_COLOUR);
            ImGui::SliderFloat("Font size", &UI_TIME_FONT_SIZE, 1, 60);
            ImGui::SliderFloat("Y padding", &UI_TIME_Y_PADDING, 1, 60);
            ImGui::SliderFloat("Width", &UI_TIME_BG_WIDTH, 1, 200);
    
            ImGui::PopID();
        }

        { // score card
            ImGui::PushID("score_style");
    
            ImGui::SeparatorText("Score");
            imgui_colour_control("Background", &UI_SCORE_BACKGROUND_COLOUR);
            ImGui::SliderFloat("Font size", &UI_SCORE_FONT_SIZE, 1, 60);
            ImGui::SliderFloat("Y padding", &UI_SCORE_Y_PADDING, 1, 60);
            ImGui::SliderFloat("X offset", &UI_SCORE_START_X_OFFSET, 1, 150);
            ImGui::SliderFloat("Y offset", &UI_SCORE_START_Y_OFFSET, -50, 50);
            ImGui::SliderFloat("Gap", &UI_SCORE_GAP, 1, 100);
            ImGui::SliderFloat("Width", &UI_SCORE_BG_WIDTH, 1, 200);
    
            ImGui::PopID();
        }

        { // health bars
            ImGui::PushID("health_bar_style");
    
            ImGui::SeparatorText("Health bars");

            imgui_v3_control("Offset", &g_health_bar_offset);
            imgui_colour_control("Health colour", &g_health_bar_health_colour);
            imgui_colour_control("Decay colour", &g_health_bar_decay_colour);
            imgui_colour_control("Empty colour", &g_health_bar_empty_colour);
            ImGui::SliderFloat("Max magnification", &g_health_bar_max_magnify_factor, 1, 10);
            ImGui::SliderFloat("Max magnification distance", &g_health_bar_max_magnify_distance, 1, 100);

            ImGui::SliderInt("Notch count", &g_health_bar_notch_count, 1, 100);
            ImGui::SliderFloat("Notch width", &g_health_bar_notch_width, 0, 2);
            ImGui::SliderFloat("Notch height", &g_health_bar_notch_height, 0, 2);
            ImGui::SliderFloat("Notch gap", &g_health_bar_notch_gap, 0, 2);
            ImGui::SliderFloat("Notch decay max height factor", &g_health_bar_notch_decay_max_height_factor, 1, 2);
            ImGui::SliderFloat("Notch decay max width factor", &g_health_bar_notch_decay_max_width_factor, 1, 2);
            ImGui::SliderFloat("Notch empty height factor", &g_health_bar_notch_empty_height_factor, 0, 1);
    
            ImGui::PopID();
        }

        { // particles
            ImGui::PushID("particle_style");

            ImGui::SeparatorText("Particles");

            ImGui::SliderFloat("Particle lifetime", &g_particle_lifetime, 0, 10);
            ImGui::SliderFloat("Particle size", &g_particle_size, 0, 0.1f);
            ImGui::SliderFloat("Particle effect lifetime", &g_particle_effect_lifetime, 0, 5);
            ImGui::SliderFloat("Particle effect size", &g_particle_effect_size, 0, 0.1f);
            ImGui::SliderInt("Particle horizontal segments", &g_particle_horizontal_segments, 0, 30);
            ImGui::SliderInt("Particle vertical segments", &g_particle_vertical_segments, 0, 30);
            ImGui::SliderFloat("Particle radial distance", &g_particle_radial_distance, 0, 5);
            ImGui::SliderFloat("Particle vertical distance", &g_particle_vertical_distance, 0, -2);
            imgui_colour_control("Particle enviroment hit colour", &g_particle_enviroment_hit_colour);
            imgui_colour_control("Particle player hit colour", &g_particle_player_hit_colour);
            ImGui::SliderFloat("Max magnification", &g_health_bar_max_magnify_factor, 1, 10);
            ImGui::SliderFloat("Max magnification distance", &g_health_bar_max_magnify_distance, 1, 100);

            ImGui::PopID();
        }

        ImGui::End();
    }
  
    { // debug info
        ImGui::Begin("Debug info");

        ImVec2 plot_size = ImVec2(450, 300);

        if (ImGui::CollapsingHeader("Performance")) {

            if (ImPlot::BeginPlot("Time to start new tick", plot_size, ImPlotFlags_NoInputs)) {
                f32 min_y = GAME_MS_PER_TICK - 2;
                f32 target_y = GAME_MS_PER_TICK;
                f32 max_y = GAME_MS_PER_TICK + 10;

                ImPlot::SetupAxes("time","ms", ImPlotAxisFlags_NoGridLines, 0);
                ImPlot::SetupAxesLimits(0, SAMPLER_SIZE, min_y, max_y);

                ImPlot::PlotInfLines("Target", &target_y, 1, ImPlotInfLinesFlags_Horizontal);
                ImPlot::PlotLine("Client", state->mspt_sampler.samples, SAMPLER_SIZE);

                if (GC()->mode == GC_HOSTED) {
                    Sampler *sampler = atomic_snapshot_read(&GS()->mspt_sampler_snapshot);
                    ImPlot::PlotLine("Server", sampler->samples, SAMPLER_SIZE);
                }

                ImPlot::EndPlot();
            }

            if (GC()->mode != GC_EDITOR) {
                if (ImPlot::BeginPlot("Time to start new tick (Net)", plot_size, ImPlotFlags_NoInputs)) {
                    f32 min_y = NETWORK_MS_PER_TICK - 2;
                    f32 target_y = NETWORK_MS_PER_TICK;
                    f32 max_y = NETWORK_MS_PER_TICK + 10;
    
                    ImPlot::SetupAxes("time","ms", ImPlotAxisFlags_NoGridLines, 0);
                    ImPlot::SetupAxesLimits(0, SAMPLER_SIZE, min_y, max_y);
    
                    ImPlot::PlotInfLines("Target", &target_y, 1, ImPlotInfLinesFlags_Horizontal);
    
                    Sampler *sampler = atomic_snapshot_read(&NET()->mspt_sampler_snapshot);
                    ImPlot::PlotLine("Net", sampler->samples, SAMPLER_SIZE);
    
                    ImPlot::EndPlot();
                }

                if (ImPlot::BeginPlot("Incoming messages", plot_size, ImPlotFlags_NoInputs)) {
                    const f32 max_messages = 20; 

                    ImPlot::SetupAxes("time","Messages", ImPlotAxisFlags_NoGridLines, 0);
                    ImPlot::SetupAxesLimits(0, SAMPLER_SIZE, 0, max_messages);

                    { // MB axis
                        f32 total_size_bytes = max_messages * f32(sizeof(NetworkMessage));

                        ImPlot::SetupAxis(ImAxis_Y2, "KB", ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_Opposite);
                        ImPlot::SetupAxisLimits(ImAxis_Y2, 0, total_size_bytes / 1024);
                    }
    
                    Sampler *client_sampler = atomic_snapshot_read(&NET()->client_in_messages_sampler_snapshot);
                    ImPlot::PlotLine("Client", client_sampler->samples, SAMPLER_SIZE);

                    Sampler *server_sampler = atomic_snapshot_read(&NET()->server_in_messages_sampler_snaphot);
                    ImPlot::PlotLine("Server", server_sampler->samples, SAMPLER_SIZE);
    
                    ImPlot::EndPlot();
                }
            }
        }

        { // basic timings
            ImGui::SeparatorText("Client frame timings");
            ImGui::Text("FPS: %.1f", 1.0f / state->frame_delta_time);
            ImGui::Text("Delta time (s): %f", state->frame_delta_time);
            ImGui::Text("Delta time (ms): %.1f", state->frame_delta_time * 1000);

            ImGui::SeparatorText("Client tick timings");
            ImGui::Text("TPS: %.1f", 1.0f / state->tick_delta_time);
            ImGui::Text("Delta time (s): %f", state->tick_delta_time);
            ImGui::Text("Delta time (ms): %.1f", state->tick_delta_time * 1000);
        }

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

            v3 forward = get_forward_direction(&GC()->camera);
            v3 right = get_right_direction(&GC()->camera);
            v3 up = get_up_direction(&GC()->camera);

            imgui_v3_control("Position", &GC()->camera.position);
            imgui_v3_control("Rotation", &GC()->camera.rotation);

            imgui_v3_control("Forward", &forward);
            imgui_v3_control("Right", &right);
            imgui_v3_control("Up", &up);
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

        ImGui::End();
    }

    { // settings
        ImGui::Begin("Settings");

        if (ImGui::Button("Reload shaders")) {
            delete_shaders(REN());
            load_shaders(REN());
        }

        if (ImGui::CollapsingHeader("Cheats")) {
            ImGui::Checkbox("Weapon binds", &g_cheat_weapon_binds);
            ImGui::Checkbox("Infinite ammo", &g_cheat_infinite_ammo);
            ImGui::Checkbox("No damage", &g_cheat_no_damage);

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
            ImGui::SeparatorText("Movement");
            ImGui::InputFloat("Ground acceleration", &g_player_ground_acceleration);
            ImGui::InputFloat("Jump acceleration", &g_player_jump_acceleration);
            ImGui::InputFloat("Ground drag", &g_player_ground_drag);
            ImGui::InputFloat("Air control", &g_player_air_control);
            ImGui::InputFloat("Gravity", &g_gravity);

            ImGui::SeparatorText("Character");
            ImGui::SliderFloat("Eyes offset", &g_player_eyes_offset, 0, g_player_height * 0.5);

            ImGui::SeparatorText("Camera shake");
            ImGui::SliderFloat("Landing shake duration", &g_landing_camera_shake_duration, 0, 2);
            ImGui::SliderFloat("Landing shake intensity", &g_landing_camera_shake_intensity, 0, 2);

            ImGui::SeparatorText("Weapon");
            ImGui::SliderFloat("Fire Cooldown", &state->player_firing_cooldown, 0, g_weapons[state->player_weapon].firing_cooldown);
            ImGui::InputInt("Ammo", (i32 *) &state->player_ammo);
            imgui_v3_control("Display offset", &g_weapon_display_offset);

            ImGui::SeparatorText("Weapon Recoil");
            ImGui::SliderFloat("Recoil scale", &g_player_recoil_scale, 0, 1);
            ImGui::SliderFloat("Recoil gain per shot", &g_player_recoil_gain_per_shot, 0, 10);
            ImGui::SliderInt("Recoil min shots", &g_player_recoil_min_shots, 0, 30);

            ImGui::SeparatorText("Camera recoil shake");
            ImGui::SliderFloat("Recoil shake frequency", &g_player_recoil_shake_frequency, 0, 30);
            ImGui::SliderFloat("Recoil shake scale", &g_player_recoil_shake_scale, 0, 20);

            ImGui::SeparatorText("Muzzle flash");
            imgui_colour_control("Flash colour", &g_muzzle_flash_colour);
            ImGui::SliderFloat("Intensity", &g_muzzle_flash_intensity, 0, 50);

            if (ImGui::CollapsingHeader("Entity")) {
                Entity *player = get_client_player(state, state->instance_id);
                if (player) {
                    imgui_entity(player);
                }
            }
        }

        if (ImGui::CollapsingHeader("Renderer")) {
            imgui_colour_control("Clear colour", &REN()->clear_colour);
            imgui_colour_control("Ambient light", &REN()->ambient_light);
            imgui_colour_control("Sun colour", &REN()->sun_colour);
            imgui_v3_control("Sun position", &REN()->sun_position);
            ImGui::InputFloat("Sun intensity", &REN()->sun_intensity);

            if (ImGui::CollapsingHeader("Frame buffers")) {
                f32 image_downscale = 4;
                ImVec2 size = ImVec2(GC()->viewport.size.x / image_downscale, GC()->viewport.size.y / image_downscale);

                ImGui::Image(REN()->main_buffer.colour_attachment, size, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image(REN()->main_buffer.depth_attachment, size, ImVec2(0, 1), ImVec2(1, 0));
            }
        }

        if (ImGui::CollapsingHeader("Crosshair")) {
            ImGui::SliderFloat("Gap", &g_crosshair_gap, 0, 20);
            ImGui::SliderFloat("Length", &g_crosshair_length, 0, 20);
            ImGui::SliderFloat("Thickness", &g_crosshair_thickness, 0, 20);
            ImGui::ColorEdit4("Colour", &g_crosshair_colour[0]);
        }

        if (ImGui::CollapsingHeader("Weapons")) {
            for (i32 i = 0; i < _WH_COUNT; i++) {
                Weapon *weapon = &g_weapons[(WeaponHandle) i];
                if (ImGui::CollapsingHeader(weapon->display_name.c())) {
                    ImGui::Text("Name: %s", weapon->display_name.c());
                    imgui_colour_control("Colour", &weapon->colour);
                    ImGui::InputFloat("Damage", &weapon->damage);
                    ImGui::InputFloat("Headshot Damage", &weapon->headshot_damage);
                    ImGui::InputInt("Ammo", &weapon->ammo_count);
                    ImGui::Checkbox("Automatic", &weapon->automatic);
                    ImGui::InputFloat("Firing cooldown", &weapon->firing_cooldown);
                    ImGui::Text("TODO: mesh");
                    ImGui::Text("TODO: sound");
                    imgui_v3_control("Recoil offset", &weapon->recoil_offset);
                    ImGui::DragFloat("Muzzle flash size", &weapon->muzzle_flash_size, 0, 1, 0.01);
                    imgui_v3_control("Muzzle flash offset", &weapon->muzzle_flash_offset, 0.01);
                    ImGui::InputFloat("Speed factor", &weapon->speed_factor);
                }
            }
        }

        if (ImGui::CollapsingHeader("Materials")) {
            EnumValue<MaterialHandle> *material_handles = meta_values<MaterialHandle>();
            for (i64 i = 0; i < meta_count<MaterialHandle>() - 1; i++) {
                EnumValue<MaterialHandle> material_handle = material_handles[i];

                if (ImGui::CollapsingHeader(material_handle.name.c())) {
                    Material *material = g_materials[material_handles[i].value];

                    imgui_v2_control("Tiling factor", &material->tiling_factor, 0.01);

                    ImGui::Text("Albedo: %dx%d", material->albedo->width, material->albedo->height);
                    ImGui::Image(material->albedo->id, ImVec2(200, 200));

                    ImGui::Text("Normal: %dx%d", material->normal->width, material->normal->height);
                    ImGui::Image(material->normal->id, ImVec2(200, 200));

                    ImGui::Text("Ambient occlusion: %dx%d", material->ambient_occlusion->width, material->ambient_occlusion->height);
                    ImGui::Image(material->ambient_occlusion->id, ImVec2(200, 200));

                    ImGui::Text("Roughness: %dx%d", material->roughness->width, material->roughness->height);
                    ImGui::Image(material->roughness->id, ImVec2(200, 200));

                    ImGui::Text("Metalness: %dx%d", material->metalness->width, material->metalness->height);
                    ImGui::Image(material->metalness->id, ImVec2(200, 200));
                }
            }
        }

        if (ImGui::CollapsingHeader("Debug")) {
            ImGui::Checkbox("Draw network owner", &g_debug_draw_owner);
            ImGui::Checkbox("Draw entities with no mesh", &g_debug_draw_no_mesh);
            ImGui::Checkbox("Always draw muzzle flash", &g_debug_always_draw_muzzle_flash);
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

        ImGui::SameLine();

        if (ImGui::Button("Point light")) {
            ED()->selected_entity = local_spawn_point_light(state);
        }

        ImGui::SameLine();

        if (ImGui::Button("Blood particle")) {
            ED()->selected_entity = local_spawn_blood_particle(state);
        }

        ImGui::SeparatorText("Teams");

        for (i64 i = 0; i < state->teams.len; i++) {
            Team *team = &state->teams[i];

            ImGui::PushID(team->client_id);

            if (ImGui::CollapsingHeader("##team")) {
                ImGui::Text("client id: %u", team->client_id);
                ImGui::Text("name: <UNAMED>");
                imgui_colour_control("colour", &team->colour);
                ImGui::Text("score: %lld", team->score);
            }

            ImGui::PopID();
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

    GC()->viewport = imgui_viewport("Game", GC()->game_view.colour_attachment, WIN()->mouse_captured);
    ED()->viewport = imgui_viewport("Editor", ED()->editor_view.colour_attachment, false);

    draw_imgui_frame();
}

void on_server_receive(State *state, NetworkMessage *message, f32 delta_time) {
    switch (message->type) {
        case NM_CLIENT_CONNECTED: {
            // when client connects, the server generates this message and a few things happen
            // 1. The client is assigned an id from the server
            // 2. Any existing state is sent only to new client:
            //      - All existing teams
            //      - All existing entities
            // 3. Any new state as a consequence from the new client is sent to all clients
            //      - New team
            //      - New player entity
            // - 02/09/25
            ConnectionId connection_id = message->client_connected;
            Infof("Processing new client connection: connection_id={}", connection_id);

            { // assign client id
                Logf("Assigning new client: id={}", connection_id);

                NetworkMessage message = NetworkMessage{.type = NM_ASSIGN_CLIENT_ID, .assign_client_id = connection_id};
                server_send_to_client(NET(), bytes_from_ptr(&message), connection_id);
            }

            { // send new client all teams 
                Log("Sending existing team info to new client");

                for (Team &team : state->teams) {
                    Logf("Sent team info to new client: client_id={}, colour={}", team.client_id, team.colour);
                    NetworkMessage message = NetworkMessage{.type = NM_NEW_TEAM, .new_team = team};
                    server_send_to_client(NET(), bytes_from_ptr(&message), connection_id);
                }
            }

            { // send new client all entities 
                Logf("Spawning {} existing entities on new client", state->entities.len);

                for (Entity &entity : state->entities) {
                    NetworkMessage message = NetworkMessage{.type = NM_SPAWN_ENTITY, .spawn_entity = entity};
                    server_send_to_client(NET(), bytes_from_ptr(&message), connection_id);
                }
            }

            { // create new team on all clients
                Team new_team = Team {
                    .client_id = connection_id,
                    .colour = random_colour(),
                    .score = 0,
                };

                append(&state->teams, new_team);

                Logf("Creating new team and updating clients: client_id={}, colour={}", new_team.client_id, new_team.colour);

                NetworkMessage message = NetworkMessage{.type = NM_NEW_TEAM, .new_team = new_team};
                server_send_to_all_clients(NET(), bytes_from_ptr(&message));
            }

            { // spawn new player entity on all cliens
                Entity *new_player = local_spawn_player(state);
                new_player->owner = connection_id;
                move_to_random_spawn_point(state, new_player);

                Logf("Spawned new player entity: entity_id={}, owner={} position={}", new_player->id, new_player->owner, new_player->position);

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
            if (state->game_complete) {
                return;
            }

            Entity *player = get_client_player(state, message->client_id);
            if (!player) {
                return;
            }

            if (player->death_cooldown > 0) {
                return;
            }

            bool grounded = entity_is_grounded(state, player);

            if (grounded) {
                player->velocity.x += message->move_player.input_direction.x * message->move_player.speed_factor * g_player_ground_acceleration * delta_time;
                player->velocity.y += message->move_player.jump                                                  * g_player_jump_acceleration;
                player->velocity.z += message->move_player.input_direction.y * message->move_player.speed_factor * g_player_ground_acceleration * delta_time;
            }
            else {
                v2 wish_direction = message->move_player.input_direction;

                v2 h_velocity = v2{player->velocity.x, player->velocity.z}; 
                f32 h_speed = length(h_velocity); 

                if (length(wish_direction) > 0 && h_speed > 0) {
                    v2 h_direction = norm(h_velocity); 

                    v2 new_h_direction = norm(HMM_LerpV2(h_direction, g_player_air_control * delta_time, wish_direction));

                    player->velocity.x = new_h_direction.x * h_speed;
                    player->velocity.z = new_h_direction.y * h_speed;
                }

                f32 allignment = HMM_DotV2(h_velocity, wish_direction);
            }
        } break;
        case NM_SHOT_ENTITY: {
            Entity *entity = get_entity_with_id(state, message->shot_entity.target_id);

            Assertf(entity, "Client sent \"shot entity\" message but the entity did not exist");
            Assertf(BitSet(entity->flags, EF_DAMAGEABLE), "Client sent \"shot entity\" message but the entity was not damagable");

            { // apply damage
                if (entity->death_cooldown > 0) {
                    return;
                }
               
                f32 start_health = entity->health;
                entity->health -= message->shot_entity.damage;

                if (entity->health < 0) {
                    entity->health = 0;
                }

                // because we clamp health this means even damage that is hgher then the remaining health
                // is still reported as the the damage needed to take the health to 0
                f32 actual_damage = start_health - entity->health;
    
                NetworkMessage response = NetworkMessage{.type = NM_CLIENT_DEALT_DAMAGE, .client_dealt_damage = actual_damage};
                server_send_to_client(NET(), bytes_from_ptr(&response), message->client_id);
            }

            // if the target was a player and is now dead, add score to clients team
            if (BitSet(entity->flags, EF_PLAYER) && entity->health <= 0) {
                u32 damage_source = message->client_id;

                for (Team &team : state->teams) {
                    if (team.client_id == damage_source) {
                        team.score += 1;
                        return;
                    }
                }

                Unreachable("Should of found team associated with message client id");
            }
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
        case NM_CLIENT_DEALT_DAMAGE: {
            timed_effect_start_or_accumulate(&GC()->health_bar_decay, 0.2, message->client_dealt_damage);
        } break;
        case NM_SET_WEAPON: {
            Logf("Client was told to use a new weapon: {}", (u32) message->set_weapon);
            set_player_weapon(state, message->set_weapon, 0);
        } break;
        case NM_NEW_TEAM: {
            Logf("Client received new team: client_id={}, colour={}", message->new_team.client_id, message->new_team.colour);
            append(&state->teams, message->new_team);
        } break;
        case NM_SYNC_TEAM: {
            for (Team &team : state->teams) {
                if (team.client_id == message->sync_team.client_id) {
                    team = message->sync_team;
                    return;
                }
            }

            Warnf("CLient was sent team sync message but team with client_id={} was not found", message->sync_team.client_id);
        } break;
        case NM_GAME_COMPLETE: {
            Info("The game has been completed");
            state->game_complete = true;
        } break;
        default: {
            Warn("Client received unknown message type");
        } break;
    }
}

u32 new_entity_id() {
    return u32(rand_i64());
}

Entity *local_spawn_entity(State *state, Entity entity) {
    Entity *spawned = push(&state->entities);
    *spawned = entity;

    spawned->time_created = state->time;

    return spawned;
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
    if (state->spawn_point_count == 0) {
        entity->position = {};
        Warn("Tried to move entity to a spawn point but there were none in the scene");
        return;
    }

    // get a random number from 0 -> spawn point count
    // skip that number of spawn points in the list and
    // pick the next in the list
    i64 spawn_point_number = rand_i64(0, state->spawn_point_count);
    i64 current_spawn_point_number = 0;

    for (Entity &other : state->entities) {
        if (BitSet(other.flags, EF_SPAWN_POINT)) {
            if (spawn_point_number == current_spawn_point_number) {
                entity->position = other.position + v3{0, g_player_height + 1, 0};
                break;
            }

            current_spawn_point_number++;
        }
    }
}

bool entity_is_grounded(State *state, Entity *entity) {
    v3 collider_size        = v3{g_player_width, 0.2, g_player_width};
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
        .flags = EF_DRAW_MESH,
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = v3{1, 1, 1},
        .colour = v4{1, 1, 1, 1},
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_player(State *state) {
    Entity entity = Entity {
        .flags = EF_PLAYER | EF_DAMAGEABLE | EF_SOLID_HITBOX | EF_COMPLEX_PHYSICS | EF_DRAW_MESH,
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = v3{g_player_width, g_player_height, g_player_width},
        .colour = TURQUOISE,
        .max_health = 100,
        .health = 100,
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_dummy(State *state) {
    Entity entity = Entity {
        .flags = EF_DUMMY | EF_DAMAGEABLE | EF_SOLID_HITBOX | EF_COMPLEX_PHYSICS | EF_DRAW_MESH,
        .id = new_entity_id(),
        .owner = SERVER_INSTANCE_ID,
        .size = v3{g_player_width, g_player_height, g_player_width},
        .colour = BLUE,
        .max_health = 100,
        .health = 100,
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_spawn_point(State *state) {
    Entity entity = Entity {
        .flags = EF_SPAWN_POINT | EF_DRAW_MESH,
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = v3{3, 0.2, 3},
        .colour = GREEN,
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_static_box(State *state) {
    Entity entity = Entity {
        .flags = EF_STATIC_HITBOX | EF_DRAW_MESH,
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = v3{1, 1, 1},
        .colour = v4{1, 1, 1, 1},
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_pickup(State *state, PickupType type) {
    Entity entity = Entity {
        .flags = EF_PICKUP | EF_DRAW_MESH,
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
        .flags = EF_JUMP_PAD | EF_DRAW_MESH,
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = v3{3, 0.2, 3},
        .colour = ORANGE,
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_point_light(State *state) {
    Entity entity = Entity {
        .flags = EF_POINT_LIGHT | EF_IGNORE_RAYCAST,
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = {0.2, 0.2, 0.2},
        .colour = WHITE,
        .light_colour = WHITE,
        .light_intensity = 10,
    };

    return local_spawn_entity(state, entity);
}

Entity *local_spawn_blood_particle(State *state) {
    Entity entity = Entity {
        .flags = EF_PARTICLE | EF_IGNORE_RAYCAST,
        .id = new_entity_id(),
        .owner = LEVEL_INSTANCE_ID,
        .size = {0.2, 0.2, 0.2},
    };

    return local_spawn_entity(state, entity);
}

void game_client_host() {
    Info("starting hosted game");

    GC()->mode = GC_HOSTED;

    game_server_start();
    network_layer_start_server(NET());
    network_layer_start_client(NET(), DEFAULT_IP);
}

void game_client_connect() {
    Info("starting and connecting to local-hosted game");

    GC()->mode = GC_CLIENT;

    network_layer_start_client(NET(), DEFAULT_IP);
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
            if (BitSet(entity.flags, EF_IGNORE_RAYCAST)) {
                continue;
            }

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
    imgui_v3_control("position", &entity->position, 0.1f);
    imgui_v3_control("size", &entity->size);
    imgui_v3_control("rotation", &entity->rotation);
    imgui_v3_control("velocity", &entity->velocity);
    imgui_colour_control("colour", &entity->colour);
    imgui_enum_dropdown("material", &entity->material);
    ImGui::InputFloat("max health", &entity->max_health);
    ImGui::InputFloat("health", &entity->health);
    ImGui::InputFloat("death cooldown", &entity->death_cooldown);
    imgui_enum_dropdown("Pickup type", &entity->pickup_type);
    ImGui::InputFloat("pickup cooldown", &entity->pickup_cooldown);
    ImGui::InputFloat("jump pad force", &entity->jump_pad_force);
    ImGui::InputFloat("jump pad cooldown", &entity->jump_pad_cooldown);
    imgui_colour_control("light colour", &entity->light_colour);
    ImGui::InputFloat("light intensity", &entity->light_intensity);
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

void imgui_v2_control(const char *label, v2 *vector, f32 step) {
    ImVec4 x_button_colour = ImVec4(0.7, 0.1, 0.1, 1);
    ImVec4 y_button_colour = ImVec4(0.1, 0.7, 0.1, 1);

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
    }
    
    ImGui::PopStyleColor(2);
    ImGui::Columns(1);
    ImGui::PopID();
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

void imgui_colour_control(const char *label, v4 *colour) {
    ImGui::PushID(label);

    ImGui::Text(label);

    ImGui::SameLine();
    ImGui::ColorEdit4("##colour", &colour->r, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    ImGui::SameLine();

    if (ImGui::Button("print")) {
        Infof("{}", *colour);
    }

    ImGui::PopID();
}

void imgui_colour_control(const char *label, v3 *colour) {
    ImGui::PushID(label);

    ImGui::Text(label);

    ImGui::SameLine();
    ImGui::ColorEdit3("##colour", &colour->r, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    ImGui::SameLine();

    if (ImGui::Button("print")) {
        Infof("{}", *colour);
    }

    ImGui::PopID();
}

template <typename T>
void imgui_enum_dropdown(const char *label, T *value) {
    EnumValue<T> *values = meta_values<T>();
    int members_count = meta_count<T>();

    i32 selected_index = meta_index(*value);
    string selected_name = values[selected_index].name;
 
    if (ImGui::BeginCombo(label, selected_name.c())) {
        for (i32 i = 0; i < members_count; i++) {
            bool is_selected = selected_index == i;
 
            if (ImGui::Selectable(values[i].name.c(), is_selected))  {
                *value = values[i].value;
            }
 
            // set the initial focus when opening the combo
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
 
        ImGui::EndCombo();
    }
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
    File file = new_file(g_level_save_file);

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

    out << YAML::Key << "id"                << YAML::Value << entity->id;
    out << YAML::Key << "owner"             << YAML::Value << entity->owner;
    out << YAML::Key << "position"          << YAML::Value << entity->position;
    out << YAML::Key << "size"              << YAML::Value << entity->size;
    out << YAML::Key << "rotation"          << YAML::Value << entity->rotation;
    out << YAML::Key << "velocity"          << YAML::Value << entity->velocity;
    out << YAML::Key << "colour"            << YAML::Value << entity->colour;
    out << YAML::Key << "material"          << YAML::Value << meta_name(entity->material);
    out << YAML::Key << "max_health"        << YAML::Value << entity->max_health;
    out << YAML::Key << "health"            << YAML::Value << entity->health;
    out << YAML::Key << "pickup_type"       << YAML::Value << meta_name(entity->pickup_type);
    out << YAML::Key << "jump_pad_force"    << YAML::Value << entity->jump_pad_force;
    out << YAML::Key << "light_colour"      << YAML::Value << entity->light_colour;
    out << YAML::Key << "light_intensity"   << YAML::Value << entity->light_intensity;
    out << YAML::EndMap;
}

void deserialise_level(State *state) {
    { // set default non-saved data in state
        state->time = 0;
        state->game_complete = false;
        state->spawn_point_count = 0;
        set_player_weapon(state, WH_DEAGLE, 0);
        reset(&state->teams);
        reset(&state->entities);
    }

    YAML::Node root = YAML::LoadFile(g_level_save_file.c());

    YAML::Node entities = root["entities"];
    if (!entities) {
        Err("No entities field in level file");
        return;
    }

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

        { // decode material from string
            std::string s = node["material"].as<std::string>();
            string saved_name = slice_create((u8 *) s.c_str(), s.size());

            // check if saved name is valid
            EnumValue<MaterialHandle> *material_handle = meta_value<MaterialHandle>(saved_name);
            if (!material_handle) {
                Warnf("No material was found with name \"{}\", okay if deleted but could be a bug!!", saved_name);
                Breakpoint;
            }

            entity.material = material_handle->value;
        }

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

        entity.jump_pad_force   = node["jump_pad_force"].as<f32>();
        entity.light_colour     = node["light_colour"].as<v4>();
        entity.light_intensity  = node["light_intensity"].as<f32>();

        if (BitSet(entity.flags, EF_SPAWN_POINT)) {
            state->spawn_point_count += 1;
        }

        append(&state->entities, entity);
    }

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
        v3 recoil_offset = v3{};

        recoil_offset += weapon->recoil_offset.x * right;
        recoil_offset += weapon->recoil_offset.y * up;
        recoil_offset += weapon->recoil_offset.z * forward;
        recoil_offset *= cooldown_scale;

        weapon_position += recoil_offset;
    }

    weapon_position += GC()->camera.position;
    v3 weapon_rotation = v3{GC()->camera.rotation.x, GC()->camera.rotation.y, 0};

    draw_mesh(REN(), g_meshes[weapon->mesh], weapon_position, {1, 1, 1}, weapon_rotation, weapon->colour, REN()->default_material);


    { // draw muzzle flash
        v3 light_position = GC()->camera.position + forward;

        TimedEffectState muzzle_flash = timed_effect_state(&GC()->muzzle_flash);

        if (muzzle_flash.active || g_debug_always_draw_muzzle_flash) {
            v3 muzzle_flash_position = v3{};
            muzzle_flash_position += weapon_position;
            muzzle_flash_position += weapon->muzzle_flash_offset.x * right;
            muzzle_flash_position += weapon->muzzle_flash_offset.y * up;
            muzzle_flash_position += weapon->muzzle_flash_offset .z * forward;

            draw_quad(REN(), muzzle_flash_position, {weapon->muzzle_flash_size, weapon->muzzle_flash_size}, weapon_rotation, WHITE, g_materials[MAT_MUZZLE_FLASH]);
            draw_point_light(REN(), muzzle_flash_position, g_muzzle_flash_colour, g_muzzle_flash_intensity);
        }
    }
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
    state->player_consecutive_shots = 0;
}

void play_weapon_fire_sound(Weapon *weapon) {
    switch (weapon->handle) {
        case WH_TAP:
        case WH_PAL:
        case WH_DEAGLE: {
            sound_engine_play(g_sounds[weapon->firing_sound]);
        } break;
        case WH_M4: {
            static i64 last_played = 0;
    
            SoundHandle actual_sound = (SoundHandle) (SH_FIRE_SILENCED_GUN_HIGH + last_played);
            sound_engine_play(g_sounds[actual_sound]);
    
            i64 sound_variations = SH_FIRE_SILENCED_GUN_LOW - SH_FIRE_SILENCED_GUN_HIGH;
            last_played += 1;
    
            if (last_played > sound_variations) {
                last_played = 0;
            }
        } break;
        default: Unreachable("New weapon handle added?");
    }
}

void timed_effect_start(TimedEffect *timed_effect, f32 duration, f32 intensity) {
    Assert(duration != 0);

    timed_effect->start_duration = duration;
    timed_effect->remaining_duration = duration;
    timed_effect->intensity = intensity;
}

void timed_effect_start_or_accumulate(TimedEffect *timed_effect, f32 duration, f32 intensity) {
    if (timed_effect->remaining_duration == 0) {
        timed_effect_start(timed_effect, duration, intensity);
        return;
    }

    timed_effect->intensity += intensity;
}

void timed_effect_tick(TimedEffect *timed_effect, f32 delta_time) {
    timed_effect->remaining_duration -= delta_time;

    if (timed_effect->remaining_duration < 0) {
        timed_effect->remaining_duration = 0;
    }
}

TimedEffectState timed_effect_state(TimedEffect *timed_effect) {
    if (timed_effect->remaining_duration == 0) {
        return TimedEffectState {.active = false};
    }

    // remaining is how much left of the original duration is left from 1->0
    // 1:   being the full duration is left
    // 0.5: halfway through the duration 
    // 0:   ended
    return TimedEffectState {
        .remaining = timed_effect->remaining_duration / timed_effect->start_duration,
        .intensity = timed_effect->intensity,
        .active = true
    };
}

f32 ease_in_sin(f32 x) {
    return 1 - cosf((x * HMM_PI32) / 2);
}

f32 ease_out_sin(f32 x) {
    return sinf((x * HMM_PI32) * 0.5);
}

f32 ease_in_out_sin(f32 x) {
    return -(cosf(HMM_PI32 * x) - 1) * 0.5f;
}

f32 ease_in_quad(f32 x) {
    return x * x;
}

f32 ease_in_cubic(f32 x) {
    return x * x * x;
}

f32 ease_out_cubic(f32 x) {
    return 1 - powf(1.0f - x, 3.0f);
}

v2 rotate_point(v2 position, v2 centre, f32 degrees) {
    f32 radians = HMM_DegToRad * degrees;
    v2 local_position = position - centre;
     
    v2 local_rotated_position = v2 {
        (local_position.x * cosf(radians)) - (local_position.y * sinf(radians)),
        (local_position.x * sinf(radians)) + (local_position.y * cosf(radians)),
    }; 

    v2 rotated_position = local_rotated_position + centre;

    return rotated_position;
};

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

    array<i32, 3> a = {100, 200, 300};
    Logf("{} {} {}", a[0], a[1], a[2]);

    for (i32 n : a) {
        Logf("{}", n);
    }

    Logf("{} {} {} {}", 
         ease_out_sin(0.0f),
         ease_out_sin(0.25f),
         ease_out_sin(0.5f),
         ease_out_sin(1.0f)
    );
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

template<>
void fmt_value(DynamicArray<u8> *bytes, v4 value) {
    append_many(bytes, slice<u8>("v4 {"));
    fmt_value(bytes, value.x);
    append_many(bytes, slice<u8>(", "));
    fmt_value(bytes, value.y);
    append_many(bytes, slice<u8>(", "));
    fmt_value(bytes, value.z);
    append_many(bytes, slice<u8>(", "));
    fmt_value(bytes, value.w);
    append_many(bytes, slice<u8>("}"));
}
