package src

import "core:fmt"
import "base:runtime"
import "core:encoding/ansi"
import "core:log"
import "core:path/filepath"
import "core:strings"
import "core:math/linalg"
import "core:os"
import "core:slice"
import "core:c"
import "core:mem"
import "core:math"
import "core:math/rand"
import "base:intrinsics"
import "core:container/small_array"

import "vendor:glfw"
import gl "vendor:OpenGL"
import stbi "vendor:stb/image"
import stbtt "vendor:stb/truetype"
import stbrp "vendor:stb/rect_pack"

import "imgui"
import "imgui/imgui_impl_glfw"
import "imgui/imgui_impl_opengl3"

import json "json"

// TODO:
// crates
// finish layout of first level
// better spawning of enemies in nests
// switch to different levels
// real path finding for enemies  

// TODO, abilities:
// level 1: going outside
//  - armour I
//  - speed
// level 2: freinds
//  - power dash (tank enemy that dashes at player, player can dash at emeies)
// level 3: the girl
//  - multi spawn (on death enemy spawn 4 smaller versions of itself, player can spawn smaller versions to attack enemies)
// level 4: family, the girl, freinds
//  - armour 2?? same as armour just more
//  - lava pit (enemies shoots projectiles to cover floor in lava, player does the same)

// record:
// start: 21/01/2025
// total time: 41 hrs
// started: 23

// controls
// developer:
// - f1: toggle live editor
// - f2: toggle input mode
// - f4: toggle editor
// - t:  reload textures
//   - editor:
//      - r:            rotate CW
//      - shift + r:    rotate CCW
//      - space:        duplicate
//      - scroll:       camera zoom

// player:
// - gamepad
//      - left stick: move
//      - right stick: aim
//      - right trigger: shoot
//      - x button: interact
//      - rb: dash
// - mouse and keyboard
//      - WASD: move
//      - mouse: aim
//      - left click: shoot
//      - e key: interact
//      - f key: dash

// indev settings
LOG_COLOURS                 :: false
OPENGL_MESSAGES             :: false
WRITE_DEBUG_IMAGES          :: true
V_SYNC                      :: true
DRAW_ARMOUR_BUBBLE          :: true
NO_ENEMY_SPAWN              :: false
CAN_RELOAD_TEXTURES         :: true
ALLOW_EDITOR                :: true
LEVEL_SAVE_NAME             :: "start"
ALL_ABILITIES_ACTIVE        :: true
GOD_MODE                    :: true

// internal settings
MAX_ENTITIES    :: 5_000
MAX_QUADS       :: 10_000
    
// gameplay settings
MAX_PLAYER_HEALTH       :: 100
PLAYER_SPEED            :: 275
PLAYER_REACH_SIZE       :: 100
PLAYER_DASH_COOLDOWN    :: 2
PLAYER_DASH_DURATION    :: 0.25
PLAYER_DASH_ATTACK_SIZE :: 100
PLAYER_DASH_DPS         :: 2000
PLAYER_DASH_SPEED_MULTIPLIER :: 5
GEM_ATTRACT_RADIUS      :: 200
GEM_ATTRACT_SPEED       :: 800
START_WEAPON_LEVEL      :: 2
MAX_WEAPON_LEVEL        :: 4

AI_ATTACK_DISTANCE          :: 10
ORC_TARGET_ATTACK_DISTANCE  :: 300
WIZARD_TARGET_ATTACK_DISTANCE  :: 500

PROJECTILE_SPEED        :: 750
PROJECTILE_LIFETIME     :: 0.8

POTION_SPEED            :: 500
SLUDGE_LIFETIME         :: 3
SPLUDGE_DPS             :: 30

MAX_ARMOUR              :: 150
ARMOUR_REGEN_RATE       :: 50
ARMOUR_REGEN_COOLDOWN   :: 1.5

// player settings
AIM_DEADZONE        :: 0.15
MOVEMENT_DEADZONE   :: 0.15
SHOOTING_DEADZONE   :: 0.3

// -------------------------- @global ---------------------------
state: State

State :: struct {
    width: f32,
    height: f32,
    window: glfw.WindowHandle,
    input_mode: InputMode,
    keys: [348]InputState,
    mouse: [8]InputState,
    mouse_position: v2,
    mouse_scroll: f32,
    gamepad: glfw.GamepadState,
    time: f64,
    player_state: PlayerState,
    level_state: LevelState,
    mode: Mode,
    editor: Editor,
    camera: struct {
        position: v2,
        // length in world units from camera centre to top edge of camera view
        // length of camera centre to side edge is this * aspect ratio
        orthographic_size: f32,
        near_plane: f32,
        far_plane: f32
    },
    renderer: Renderer,
    entities: []Entity,
    entity_count: int,
    id_counter: int,
}

PlayerState :: struct {
    dropped_abilities: bit_set[Ability; u64],
    weapon_level: int,
    collected_gems_this_level: int,
    kills_per_ai: [AiType]int,
}

LevelState :: struct {
    in_boss_battle: bool,
    boss_battle_over: bool
}

Mode :: enum {
    game,
    editor
}

InputMode :: enum {
    mouse_and_keyboard,
    gamepad,
}

InputState :: enum {
    up,
    down,
    pressed,
}

main :: proc() {
    context = custom_context()

    state = {
        width = 1920,
        height = 1080,
        camera = {
            position = {0, 0},
            // length in world units from camera centre to top edge of camera view
            // length of camera centre to side edge is this * aspect ratio
            orthographic_size = 600,
            near_plane = 0.01,
            far_plane = 100
        },
        player_state = {
            weapon_level = START_WEAPON_LEVEL
        },
        mode = .game,
        editor = {
            use_grid = true,
            grid_size = {50, 50},
            use_highlight = true,
            camera_move_speed = 6,
            entity_move_speed = 5
        },
        renderer = {
            quads = make([]Quad, MAX_QUADS)
        },
        entities = make([]Entity, MAX_ENTITIES)
    }

    { // initialise everything
        ok: bool

        ok = load_textures(&state.renderer)
        if !ok {
            log.fatal("error when loading textures")
            return
        }
    
        ok = build_texture_atlas(&state.renderer)
        if !ok {
            log.fatal("error when building texture atlas")
            return
        }

        ok = load_font(&state.renderer, .baskerville, 2000, 2000, 320, .linear)
        if !ok {
            log.fatal("error when loading fonts")
            return
        }

        // ok = load_font(.alagard, 128, 128, 15, .nearest)
        // if !ok {
            // log.fatal("error when loading fonts")
            // return
        // }

        state.window, ok = create_window(state.width, state.height, "game5")
        if !ok {
            log.fatal("error trying to init window")
            return
        }
     
        ok = init_renderer(&state.renderer)
        if !ok {
            log.fatal("error when initialising the renderer")
            return
        }

        ok = init_imgui(&state.renderer)
        if !ok {
            log.fatal("error when initialising imgui")
            return
        }
    }

    start()

    last_frame_start_time: f64

    for !glfw.WindowShouldClose(state.window) {
        free_all(context.temp_allocator)

        if state.keys[glfw.KEY_ESCAPE] == .down {
            glfw.SetWindowShouldClose(state.window, true)
        }

        when ALLOW_EDITOR {
            if state.keys[glfw.KEY_F1] == .down {
                state.editor.live_level = true
                state.mode = next_enum_value(state.mode)
            }

            if state.keys[glfw.KEY_F4] == .down {
                state.editor.live_level = false
                assert(load_level(LEVEL_SAVE_NAME))
                state.mode = next_enum_value(state.mode)
            }
        }

        when CAN_RELOAD_TEXTURES {
            if state.keys[glfw.KEY_T] == .down {
                ok := reload_textures(&state.renderer)
                if !ok {
                    log.error("tried to reload textures but failed...")
                }
            }
        }

        if state.keys[glfw.KEY_F2] == .down {
            state.input_mode = next_enum_value(state.input_mode)
        }

        now := glfw.GetTime()
        delta_time := f32(now - last_frame_start_time)
        last_frame_start_time = now

        if state.mode == .game {
            state.time = now 
        }

        when ALLOW_EDITOR {
            imgui_impl_opengl3.NewFrame()
	    imgui_impl_glfw.NewFrame()
	    imgui.NewFrame()
        }

        switch state.mode {
            case .game:     tick_game(delta_time)
            case .editor:   tick_editor(delta_time)
        } 

        in_screen_space = false

        gl.Clear(gl.COLOR_BUFFER_BIT)

        // update vertex buffer with current quad data
        gl.BindBuffer(gl.ARRAY_BUFFER, state.renderer.vertex_buffer_id)
        gl.BufferSubData(gl.ARRAY_BUFFER, 0, size_of(Quad) * state.renderer.quad_count, slice.as_ptr(state.renderer.quads))

        // draw quads
        if state.renderer.quad_count > 0 {
            gl.UseProgram(state.renderer.shader_program_id)

            gl.ActiveTexture(gl.TEXTURE0)
            gl.BindTexture(gl.TEXTURE_2D, state.renderer.atlas_texture_id)
            gl.ActiveTexture(gl.TEXTURE1)
            gl.BindTexture(gl.TEXTURE_2D, state.renderer.font_texture_id)

            gl.BindVertexArray(state.renderer.vertex_array_id)

            gl.DrawElements(gl.TRIANGLES, 6 * i32(state.renderer.quad_count), gl.UNSIGNED_INT, nil)
        }

        state.renderer.quad_count = 0 

        when ALLOW_EDITOR {
            imgui.EndFrame()
            imgui.Render()
	    imgui_impl_opengl3.RenderDrawData(imgui.GetDrawData()) 
	    backup_current_window := glfw.GetCurrentContext()
	    imgui.UpdatePlatformWindows()
	    imgui.RenderPlatformWindowsDefault()
	    glfw.MakeContextCurrent(backup_current_window)
        }

        glfw.SwapBuffers(state.window)
    }

    glfw.DestroyWindow(state.window)
    glfw.Terminate()
}

tick_game :: proc(delta_time: f32) {
    input()
    update(delta_time)
    physics(delta_time)
    draw(delta_time)
}

tick_editor :: proc(delta_time: f32) {
    input()
    update_editor()
    draw(delta_time)

    in_screen_space = false

    editor_ui()
}

LevelSaveData :: struct {
    entities: []Entity
}

save_level :: proc(name: string) -> bool {
    LEVEL_DIRECTORY :: "resources/levels"

    path := fmt.tprintf("%v/%v.json", LEVEL_DIRECTORY, name)
    log.infof("saving level to \"%v\"", path)

    backup_ok := make_level_backup_if_exist(path)
    if !backup_ok {
        log.error("canceling saving of level because creating backup failed")
        return false
    }

    save_data := LevelSaveData {
        entities = state.entities[0:state.entity_count]
    }

    { // marshal into json bytes and write
        bytes, error := json.marshal(
            save_data, 
            json.Marshal_Options{
                spec = .JSON5,
                pretty = true,
                use_spaces = true,
                spaces = 0,
                write_uint_as_hex = false,
            },
            context.temp_allocator
        )
        
        if error != nil {
            log.errorf("error when trying to marshal entities, marshaling returned: %v", error)
            return false
        }

        ok := os.write_entire_file(path, bytes)
        if !ok {
            log.error("error when trying to write json data to file")
            return false
        }
    }

    log.info("succesfully saved level data")

    return true
}

load_level :: proc(name: string) -> bool {
    LEVEL_DIRECTORY :: "resources/levels"

    path := fmt.tprintf("%v/%v.json", LEVEL_DIRECTORY, name)
    log.infof("loading level from \"%v\"", path)

    bytes, ok := os.read_entire_file(path, context.temp_allocator)
    if !ok {
        log.error("error when trying to read json data from file")
        return false
    }

    save_data: LevelSaveData

    error := json.unmarshal(bytes, &save_data, spec = .JSON5, allocator = context.temp_allocator)
    if error != nil {
        log.errorf("error when trying to unmarshal entities, unmarshaling returned: %v", error)
        return false
    }

    mem.copy(&state.entities[0], &save_data.entities[0], len(save_data.entities) * size_of(Entity))
    state.entity_count = len(save_data.entities)

    { // reset id counter
        max_id := -1

        for &entity in state.entities[0:state.entity_count] {
            if entity.id > max_id {
                max_id = entity.id 
            }
        }

        state.id_counter = max_id + 1
        assert(state.id_counter >= 0)
    }

    log.info("succesfully loaded level data")

    return true
}

make_level_backup_if_exist :: proc(path: string) -> bool {
    BACKUP_DIRECTORY :: "resources/levels/backups"

    exists := os.exists(path)
    if !exists {
        return true
    }

    file_data, ok := os.read_entire_file(path, context.temp_allocator)
    if !ok {
        log.error("failed to create save backup, failed to read existing file data")
        return false
    }

    file_name := filepath.base(path)
    random_prefix := rand.int_max(999_999)
    
    backup_path := fmt.tprintf("%v/%v_%v.json", BACKUP_DIRECTORY, random_prefix, file_name)

    ok = os.write_entire_file(backup_path, file_data)
    if !ok {
        log.error("failed to create save backup, failed to write file")
        return false
    }

    log.infof("created save backup to \"%v\"", backup_path)

    return true
}

// -------------------------- @game -----------------------
Entity :: struct {
    // meta
    id: int,
    flags: bit_set[EntityFlag; u64],
    abilities: bit_set[Ability; u64],
    created_time: f64,
    only_boss_battle: bool,

    // global
    position: v2,
    size: v2,
    velocity: v2,
    rotation: f32,
    mass: f32,
    texture: TextureHandle,
    attack_cooldown: f32,

    // flag: player
    aim_direction: v2,
    player_dash_cooldown: f32,

    // flag: has_health
    health: f32,
    max_health: f32,

    // ability: armour
    armour: f32,
    armour_regen_cooldown: f32,

    // flag: ai
    ai_type: AiType,
    ai_state: AiState,

    // flag: nest
    cluster_size: int,
    spawn_rate: f32,
    spawn_cooldown: f32,
    speeders_to_spawn: int,
    drones_to_spawn: int,
    orcs_to_spawn: int,
    total_spawns: int,
    spawn_radius: f32,

    // flag: ability pickup
    pickup_type: PickupType,

    // flag: potion
    potion_lifetime: f32
}

EntityFlag :: enum {
    player,
    ai,
    nest,
    projectile,
    ability_pickup,

    solid_hitbox,
    static_hitbox,
    trigger_hitbox,
    interactable,

    has_health,

    gem,
    door,
    potion,
    spludge,

    is_dashing,

    to_be_deleted
}

AiType :: enum {
    speeder,
    drone,
    orc,
    wizard,
}

AiState :: enum {
    tracking,
    attacking,
    charging_dash,
    dashing,
    dash_cooldown,
    throwing_potion
}

Ability :: enum {
    armour,
    speed,
    dash,
    potion
}

PickupType :: enum {
    armour,
    speed,
    dash,
    potion
}

Prefab :: enum {
    blank,
    player,
    nest,
    pickup,
    brick_wall,
    corner_wall,
    brick_wall_vertical,
    brick_wall_corner_left,
    brick_wall_corner_right,
    gem,
    crate,
    door,
    speeder,
    drone,
    orc,
    wizard,
    potion,
    spludge,
}

start :: proc() {
    load_level(LEVEL_SAVE_NAME)
}

input :: proc() {
    // this will set the state of things to up or down
    // to keep track of what is already down, we can go through
    // every key before this and set it to pressed, if is still
    // down we dont get and event and it stays pressed, if we get
    // an event for that key it will be to set it to up so the
    // pressed we accidentlly set is changed, this is not the best
    // - 24/01/25

    for &input in state.keys {
        if input == .down {
            input = .pressed
        }
    }

    for &input in state.mouse {
        if input == .down {
            input = .pressed
        }
    }

    state.mouse_scroll = 0

    glfw.PollEvents()

    if glfw.GetGamepadState(glfw.JOYSTICK_1, &state.gamepad) == 0 {
        if state.input_mode == .gamepad {
            log.error("in gamepad mode but not gamepad detected")
        }
    }
}

update :: proc(delta_time: f32) {
    player_this_frame := get_entity_with_flag(.player)

    for &entity in state.entities[0:state.entity_count] {
        { // reduce cooldowns
            entity.attack_cooldown -= delta_time
            if entity.attack_cooldown < 0 {
                entity.attack_cooldown = 0
            }

            entity.armour_regen_cooldown -= delta_time
            if entity.armour_regen_cooldown < 0 {
                entity.armour_regen_cooldown = 0
            }

            entity.spawn_cooldown -= delta_time
            if entity.spawn_cooldown < 0 {
                entity.spawn_cooldown = 0
            }

            entity.player_dash_cooldown -= delta_time
            if entity.player_dash_cooldown < 0 {
                entity.player_dash_cooldown = 0
            }
        }

        if entity.only_boss_battle {
            if !state.level_state.in_boss_battle {
                continue
            }
        }

        player_update: {
            if !(.player in entity.flags) {
                break player_update
            }

            state.camera.position = entity.position

            when ALL_ABILITIES_ACTIVE {
                entity.abilities += {.speed, .armour, .dash}
            }

            { // set aim
                aim_vector := get_aim_input(entity.position)

                input_length := linalg.length(aim_vector)
    
                if input_length > AIM_DEADZONE {
                    entity.aim_direction = linalg.normalize(aim_vector)
                }
            }

            { // movement
                entity.velocity = 0
    
                input_vector := get_movement_input()
                input_length := linalg.length(input_vector)
    
                if input_length > MOVEMENT_DEADZONE {
                    if input_length > 1 {
                        input_vector = linalg.normalize(input_vector)
                    }
    
                    entity.velocity = input_vector * PLAYER_SPEED
                }
            }

            shooting: { 
                input := get_shooting_input()

                if input < SHOOTING_DEADZONE {
                    break shooting
                }

                if entity.attack_cooldown != 0 {
                    break shooting
                }
                   
                entity.attack_cooldown = attack_cooldown_for_weapon_level(state.player_state.weapon_level)
                create_bullet(entity.position, entity.aim_direction * PROJECTILE_SPEED)
            }

            interact: { // interact
                if get_interact_input() != .down {
                    break interact
                }

                for &other in state.entities[0:state.entity_count] {
                    if !(.interactable in other.flags) {
                        continue 
                    }

                    if .ability_pickup in other.flags {
                        if linalg.distance(entity.position, other.position) < PLAYER_REACH_SIZE {
                            ability := ability_from_pickup_type(other.pickup_type)
     
                            entity.abilities += {ability}
                            other.flags += {.to_be_deleted}
                        }
                    }

                    if .door in other.flags {
                        if !state.level_state.in_boss_battle && !state.level_state.boss_battle_over {
                            state.level_state.in_boss_battle = true
                        }
                    }
                }
            }

            weapon_upgrade: {
                if state.player_state.weapon_level == MAX_WEAPON_LEVEL {
                    break weapon_upgrade
                }

                gems_needed_for_upgrade := gems_needed_for_level(state.player_state.weapon_level + 1)
                if state.player_state.collected_gems_this_level >= gems_needed_for_upgrade {
                    state.player_state.weapon_level += 1
                    state.player_state.collected_gems_this_level = 0
                }
            }

            dash: {
                if !(.dash in entity.abilities) {
                    break dash
                }

                if !(.is_dashing in entity.flags) {
                    if entity.player_dash_cooldown > 0 {
                        break dash
                    }
    
                    input := get_dash_input()
                    if input == .down {
                        entity.flags += {.is_dashing}
                        entity.player_dash_cooldown = PLAYER_DASH_COOLDOWN
                        entity.armour = MAX_ARMOUR
                    }
                }

                if .is_dashing in entity.flags {
                    done := wait(TimeId(entity.id), PLAYER_DASH_DURATION)
                    if done {
                        entity.flags -= {.is_dashing}
                    }
                    
                    entity.velocity = entity.aim_direction * PLAYER_SPEED * PLAYER_DASH_SPEED_MULTIPLIER

                    iter := new_box_collider_iterator(entity.position, entity.size + PLAYER_DASH_ATTACK_SIZE)
                    for {
                        other := next(&iter)
                        if other == nil {
                            break
                        }

                        if !(.ai in other.flags) {
                            continue
                        }

                        entity_take_damage(other, PLAYER_DASH_DPS * delta_time)
                    }
                }
            }
        }

        ai_update: {
            if !(.ai in entity.flags) {
                break ai_update
            }

            entity.velocity = 0

            if player_this_frame == nil {
                break ai_update
            }

            switch entity.ai_type {
                case .speeder, .drone:
                #partial switch entity.ai_state {
                    case .tracking: {
                        direction := linalg.normalize(player_this_frame.position - entity.position)
                        entity.velocity = direction * ai_speed(entity.ai_type)
    
                        if entity.attack_cooldown != 0 {
                            break
                        }
    
                        distance := distance_between_entity_edges(&entity, player_this_frame)
                        if linalg.max(distance) < AI_ATTACK_DISTANCE {
                            entity.ai_state = .attacking     
                        }
                    }
                    case .attacking: {
                        hit_player := aabb_collided(entity.position, entity.size + AI_ATTACK_DISTANCE * 2, player_this_frame.position, player_this_frame.size)
                        if hit_player {
                            entity_take_damage(player_this_frame, ai_damage(entity.ai_type))
                            entity.attack_cooldown = ai_attack_cooldown(entity.ai_type)
                        }
     
                        entity.ai_state = .tracking 
                    }
                    case: unreachable()
                }
                case .orc:
                DASH_CHARGE_TIME    :: 0.8
                DASHING_TIME        :: 0.45
                DASH_COOLDOWN_TIME  :: 1.2

                #partial switch entity.ai_state {
                    case .tracking: {
                        direction := linalg.normalize(player_this_frame.position - entity.position)
                        entity.velocity = direction * ai_speed(entity.ai_type)
    
                        distance := linalg.distance(entity.position, player_this_frame.position)
                        if distance < ORC_TARGET_ATTACK_DISTANCE {
                            entity.ai_state = .charging_dash
                        }
                    }
                    case .charging_dash: {
                        done := wait(TimeId(entity.id), DASH_CHARGE_TIME)
                        if done {
                            entity.ai_state = .dashing
                            entity.flags += {.is_dashing}
                        }
                    }
                    case .dashing: {
                        dashing_timer := TimeId(entity.id)

                        done := wait(dashing_timer, DASHING_TIME)
                        if done {
                            entity.ai_state = .dash_cooldown
                        }

                        direction := linalg.normalize(player_this_frame.position - entity.position)
                        entity.velocity = direction * ai_speed(entity.ai_type) * 5

                        hit_player := aabb_collided(entity.position, entity.size + AI_ATTACK_DISTANCE * 2, player_this_frame.position, player_this_frame.size)
                        if hit_player {
                            entity_take_damage(player_this_frame, ai_damage(entity.ai_type))
                            cancel(dashing_timer)
                            entity.ai_state = .dash_cooldown
                        }
                    }
                    case .dash_cooldown: {
                        entity.flags -= {.is_dashing}

                        done := wait(TimeId(entity.id), DASH_COOLDOWN_TIME)
                        if done {
                            entity.ai_state = .tracking
                        }
                    }
                    case: unreachable()
                }
                case .wizard:
                #partial switch entity.ai_state {
                    case .tracking: {
                        direction := linalg.normalize(player_this_frame.position - entity.position)
                        entity.velocity = direction * ai_speed(entity.ai_type)
    
                        distance := linalg.distance(entity.position, player_this_frame.position)
                        if distance < WIZARD_TARGET_ATTACK_DISTANCE {
                            entity.ai_state = .throwing_potion
                        }
                    }
                    case .throwing_potion: {
                        done := wait(TimeId(entity.id), 3)
                        if done {
                            entity.ai_state = .tracking
                        }

                        if entity.attack_cooldown != 0 {
                            break
                        }

                        target_position := player_this_frame.position + player_this_frame.velocity // target is player in 1 second
                        target_direction := target_position - entity.position

                        seconds_to_target := linalg.length(target_direction) / POTION_SPEED

                        create_potion(entity.position, linalg.normalize(target_direction) * POTION_SPEED, seconds_to_target)
                        entity.attack_cooldown = ai_attack_cooldown(.wizard) 
                    }
                    case: unreachable()
                }
            }
        }

        nest_update: {
            if !(.nest in entity.flags) {
                break nest_update
            }

            when NO_ENEMY_SPAWN {
                break nest_update
            }

            if entity.spawn_cooldown != 0 {
                break nest_update
            }

            if player_this_frame == nil {
                break nest_update
            }

            distance_to_player := linalg.distance(entity.position, player_this_frame.position)
            if distance_to_player > entity.spawn_radius {
                break nest_update
            }

            random := rand.float32()

            spawned: bool

            for _ in 0..<entity.cluster_size {
                if entity.speeders_to_spawn > 0 && random < ai_spawn_chance(.speeder) {
                    create_speeder(entity.position)
                    entity.speeders_to_spawn -= 1
                    spawned = true
                }
    
                if entity.drones_to_spawn > 0 && random < ai_spawn_chance(.drone) {
                    create_drone(entity.position)    
                    entity.drones_to_spawn -= 1
                    spawned = true
                }

                if entity.orcs_to_spawn > 0 && random < ai_spawn_chance(.orc) {
                    create_orc(entity.position)    
                    entity.orcs_to_spawn -= 1
                    spawned = true
                }
            }

            if spawned {
                entity.spawn_cooldown += entity.spawn_rate
            }
        }

        projectile_update: {
            if !(.projectile in entity.flags) {
                break projectile_update
            }

            if state.time - entity.created_time > PROJECTILE_LIFETIME {
                entity.flags += {.to_be_deleted}  
            }
        }

        potion_update: {
            if !(.potion in entity.flags) {
                break potion_update
            }

            entity.rotation += 1

            if f32(state.time - entity.created_time) > entity.potion_lifetime {
                create_spludge(entity.position)
                entity.flags += {.to_be_deleted}  
            }
        }

        health_update: {
            if !(.has_health in entity.flags) {
                break health_update
            }

            if entity.health == 0 {
                entity.flags += {.to_be_deleted}
                on_entity_killed(&entity)
            }
        }

        armour_update: {
            if !(.armour in entity.abilities) {
                break armour_update
            }
            
            if entity.armour != MAX_ARMOUR && entity.armour_regen_cooldown == 0 {
                entity.armour += ARMOUR_REGEN_RATE * delta_time

                if entity.armour > MAX_ARMOUR {
                    entity.armour = MAX_ARMOUR
                }
            }
        }

        speed_update: {
            if !(.speed in entity.abilities) {
                break speed_update
            }
            
            entity.velocity *= 1.3
        }

        gem_update: {
            if !(.gem in entity.flags) {
                break gem_update
            }

            if player_this_frame == nil {
                break gem_update
            }

            distance_to_player := linalg.distance(entity.position, player_this_frame.position)
            
            if distance_to_player  <= GEM_ATTRACT_RADIUS {
                direction := linalg.normalize(player_this_frame.position - entity.position)
                distance_percentage := distance_to_player / GEM_ATTRACT_RADIUS

                speed := ease_in_sine(1 - distance_percentage) * GEM_ATTRACT_SPEED * direction 

                entity.velocity = speed    
            }

            if distance_to_player < linalg.min(player_this_frame.size) {
                state.player_state.collected_gems_this_level += 1
                entity.flags += {.to_be_deleted}
            }
        }

       spludge_update: {
            if !(.spludge in entity.flags) {
                break spludge_update
            }

            alive_for := state.time - entity.created_time

            if alive_for > SLUDGE_LIFETIME {
                entity.flags += {.to_be_deleted}
                break spludge_update
            }

            if player_this_frame == nil {
                break spludge_update
            }

            hit_player := aabb_collided(entity.position, entity.size, player_this_frame.position, player_this_frame.size)
            if hit_player {
                entity_take_damage(player_this_frame, SPLUDGE_DPS * delta_time)
            }
        }
    }

    // deleting work by doing a spaw remove on the entity buffer
    // of any entity that has the to_be_deleted flag set 
    // we only increment index of the entity we are checking 
    // when the entity was not deleted
    // if the entity was deleted then we want to recheck the
    // new one at the current index - 25/01/25

    index := 0
    for index < state.entity_count {
        entity := &state.entities[index]
    
        if .to_be_deleted in entity.flags {
            delete_entity_from_buffer(index) 
        } else {
            index += 1
        }
    }
}

delete_entity_from_buffer :: proc(index: int) {
    // last value just decrement count
    if index == state.entity_count - 1 {
        state.entity_count -= 1
        return
    }
    
    // swap remove with last entity
    state.entities[index] = state.entities[state.entity_count - 1]
    state.entity_count -= 1
}

physics :: proc(delta_time: f32) {
    for &entity in state.entities[0:state.entity_count] {
        // used to check if a collision occurs after
        // a velocity is applied
        start_position := entity.position
    
        entity.position += entity.velocity * delta_time

        solid_hitbox: {
            // this means if we are ever doing a collision the 
            // soldin one is always the entity, and if one is 
            // static then it is the other entity
            if !(.solid_hitbox in entity.flags) {
                break solid_hitbox
            }
    
            for &other in state.entities[0:state.entity_count] {
                if entity == other {
                    continue
                }
        
                if !(.solid_hitbox in other.flags) && !(.static_hitbox in other.flags) {
                    continue
                }

                overlap_amount, distance, collision := aabb(entity.position, entity.size, other.position, other.size)
                if !collision {
                    continue;
                }
        
                other_static := .static_hitbox in other.flags
        
                // if there is an overlap then measure on which axis has less 
                // overlap and move both entities by that amount away
                // from each other, the proprtion to move each entity is
                // based on their mass to each other
                // m1 == m2 -> 0.5 of total overlap each
                // m1 = 9, m2 = 1 -> e1 moves 0.1 of overlap and e2 moves 0.9
                // - 23/01/25

                // if other is static only move entity
                // by the full overlap instead of sharing it
                if other_static {
                    if overlap_amount.x < overlap_amount.y {
                        entity.position.x -= math.sign(distance.x) * overlap_amount.x

                    } else {
                        entity.position.y -= math.sign(distance.y) * overlap_amount.y
                    }
                } else {
                    total_mass              := entity.mass + other.mass
                    entity_push_proportion  := 1 - (entity.mass / total_mass)
                    other_push_proportion   := 1 - (other.mass / total_mass)

                    if overlap_amount.x < overlap_amount.y {
                        x_push_amount       := overlap_amount.x
                        entity.position.x   -= math.sign(distance.x) * x_push_amount * entity_push_proportion
                        other.position.x    += math.sign(distance.x) * x_push_amount * other_push_proportion
                    } 
                    else {
                        y_push_amount       := overlap_amount.y
                        entity.position.y   -= math.sign(distance.y) * y_push_amount * entity_push_proportion
                        other.position.y    += math.sign(distance.y) * y_push_amount * other_push_proportion
                    }
                }
            } 
        }

        trigger_hitbox: {
            if !(.trigger_hitbox in entity.flags) {
                break trigger_hitbox
            }
    
            for &other in state.entities[0:state.entity_count] {
                if entity == other {
                    continue
                }
        
                if  !(.solid_hitbox in other.flags) && 
                    !(.static_hitbox in other.flags) &&
                    !(.trigger_hitbox in other.flags)
                {
                    continue
                }
        
                // collision for each other entiity is checked twice for trigger
                // hitboxes, trigger collision events are only when a new collision
                // starts so if there was a collision last frame then dont do anything

                collision_last_frame := aabb_collided(start_position, entity.size, other.position, other.size)
                if collision_last_frame {
                    continue;
                }

                collision_this_frame := aabb_collided(entity.position, entity.size, other.position, other.size)
                if collision_this_frame {
                    on_trigger_collision(&entity, &other)
                }
            }
        }
    }
}

draw :: proc(delta_time: f32) {
    for &entity in state.entities[0:state.entity_count] {
        base_colour         := WHITE
        highlight_colour    := WHITE

        if .player in entity.flags {
            for &other in state.entities[0:state.entity_count] {
                if !(.interactable in other.flags) {
                    continue 
                }

                if linalg.distance(entity.position, other.position) < PLAYER_REACH_SIZE {
                    ICON_SIZE :: 25

                    icon_position := other.position + {0, (other.size.y * 0.5) + ICON_SIZE}

                    draw_texture(.x_button, icon_position, {ICON_SIZE, ICON_SIZE}, colour = WHITE, highlight_colour = WHITE)
                }
            }
        }

        if .is_dashing in entity.flags {
            angle := linalg.angle_between(entity.velocity, v2{0, 1}) * linalg.DEG_PER_RAD
             
            if math.sign(entity.velocity.x) != 0 {
                angle *= math.sign(entity.velocity.x)
            }

            fireball_scale: f32

            if .player in entity.flags {
                fireball_scale = 3
            } else {
                fireball_scale = 1.5
            }

            draw_texture(.fireball, entity.position, entity.size * fireball_scale, rotation = angle)
        }

        if .nest in entity.flags {
            left_to_spawn := entity.speeders_to_spawn + entity.drones_to_spawn + entity.orcs_to_spawn

            percentage_left_to_spawn := f32(left_to_spawn) / f32(entity.total_spawns)
            highlight_colour = mix(BLACK, RED, percentage_left_to_spawn)
        }

        if .has_health in entity.flags {
            health_bar_width := entity.size.x * 2
            percentage_health_left := entity.health / entity.max_health 

            highlight_colour = mix(RED, SKY_BLUE, percentage_health_left)
        }

        when DRAW_ARMOUR_BUBBLE {
        if .armour in entity.abilities && !(.is_dashing in entity.flags) {
            armour_colour: v4

            if entity.armour == MAX_ARMOUR {
                armour_colour = YELLOW
            } else {
                armour_colour = BLUE
            }

            armour_alpha := entity.armour / MAX_ARMOUR

            size_multiplier: f32
            if .player in entity.flags {
                size_multiplier = 2 
            } else {
                size_multiplier = 1.2
            }

            draw_texture(.armour, entity.position, entity.size * size_multiplier, colour = alpha(WHITE, armour_alpha), highlight_colour = alpha(armour_colour, armour_alpha))
        }
        }

        if .ability_pickup in entity.flags {
            // TODO: think of a way to show the different types of pickups
        }

        if .spludge in entity.flags {
            alive_for := state.time - entity.created_time
            alive_percentage := f32(alive_for / SLUDGE_LIFETIME)

            base_colour = alpha(base_colour, 1 - ease_in_cubic(alive_percentage))
        }

        if entity.only_boss_battle && !state.level_state.in_boss_battle {
            if state.mode == .editor {
                base_colour = GREEN
            } else {
                continue
            }
        }

        draw_texture(entity.texture, entity.position, entity.size, rotation = entity.rotation, colour = base_colour, highlight_colour = highlight_colour)
    } 

    in_screen_space = true 

    player_info: { // player abilities info
        ICON_SIZE       :: v2{30, 30}
        SCREEN_PADDING  :: v2{10, 10}
        ICON_X_PADDING  :: 5
        ICON_COUNT      :: 10
        LAST_HEART_SIZE_MULTIPLIER :: 1.2

        player := get_entity_with_flag(.player)
        if player == nil {
            break player_info 
        }

        remaining_health := (player.health / MAX_PLAYER_HEALTH) * 10 // from 0 -> 10
        hearts_remaining := int(math.trunc_f32(remaining_health))
        hearts_remaining = clamp(hearts_remaining, 1, ICON_COUNT) // if health was less then 10% then this is 0

        last_heart_alpha: f32
        if remaining_health < 1 {
            // if you dont do this when the health is less then 10%
            // it will always be zero because there is no value on 
            // the left side of the decimal point
            last_heart_alpha = remaining_health / 10
        } else {
            last_heart_alpha = remaining_health - math.trunc_f32(remaining_health) // 9.5 - 9 == 0.5 alpha
        }

        last_heart_alpha = clamp(last_heart_alpha, 0.1, 1)

        for i in 0..<hearts_remaining {
            heart_colour := WHITE
            icon_draw_size := ICON_SIZE

            // only change size when last heart
            if i == hearts_remaining - 1 {
                if remaining_health != 10 {
                    heart_colour = alpha(heart_colour, last_heart_alpha)
                }

                icon_draw_size *= LAST_HEART_SIZE_MULTIPLIER
            }

            start_position := ICON_SIZE * 0.5 + SCREEN_PADDING
            offset_from_start := v2{ICON_SIZE.x * f32(i) + (ICON_X_PADDING * f32(i)), 0}
            draw_texture(.heart, start_position + offset_from_start, icon_draw_size, colour = heart_colour)
        }

        armour_bar: {
            if !(.armour in player.abilities) {
                break armour_bar
            }

            percentage_remaining_armour := player.armour / MAX_ARMOUR

            MAX_WIDTH :: 350
            height := MAX_WIDTH * (1 / state.renderer.textures[.armour_bar].aspect_ratio)
            size := v2{MAX_WIDTH * percentage_remaining_armour, height}

            BASE_BLUE :: v4{0.121, 0.176, 0.521, 1} // same blue from texture
            highlight_colour := brightness(BASE_BLUE, 0.75)

            if percentage_remaining_armour == 1 {
                highlight_colour = YELLOW 
            }

            start_position := SCREEN_PADDING + {0, 50}
            draw_texture(.armour_bar, start_position + (size * 0.5), size, highlight_colour = highlight_colour)  
        }

        dash: {
            DASH_ICON_SIZE :: 90

            if !(.dash in player.abilities) {
                break dash
            }

            colour := WHITE
            if player.player_dash_cooldown != 0 {
                colour = alpha(colour, 0.25)
            }

            position := v2{state.width, 0} + SCREEN_PADDING * {-1, 0}
            draw_texture(.dash_icon, position + v2{-DASH_ICON_SIZE, DASH_ICON_SIZE}, v2{DASH_ICON_SIZE, DASH_ICON_SIZE}, colour = colour)

            if .is_dashing in player.flags {
                draw_texture(.speed_lines, {state.width, state.height} * 0.5, {state.width, state.height})
            }
        }
    }

    { // game info 
        font_size : f32 = 15 

        fps := math.trunc(1 / delta_time)
        text := fmt.tprintf("%v", fps)
        draw_text(text, {10, state.height - font_size}, font_size, WHITE, .bottom_left)
    }

    { // input mode
        size : f32 = 20
        draw_text(fmt.tprint(state.input_mode), {state.width * 0.5, size}, size, WHITE, .center)
    } 

    { // weapon info
        gems_needed_for_upgrade: int
        gems := state.player_state.collected_gems_this_level

        if state.player_state.weapon_level != MAX_WEAPON_LEVEL {
            gems_needed_for_upgrade = gems_needed_for_level(state.player_state.weapon_level + 1)
        }
       
        size : f32 = 20
        text := fmt.tprintf("%v/%v", gems, gems_needed_for_upgrade)
        draw_text(text, {state.width - 130, state.height - size}, size, YELLOW, .bottom_left)
    }
}

on_entity_killed :: proc(entity: ^Entity) {
    if .ai in entity.flags {
        { // add death to player state
            state.player_state.kills_per_ai[entity.ai_type] += 1
        }

        ability_drop: { // drop ability pickup
            // set should be empty if the ability has not dropped yet
            if entity.abilities & state.player_state.dropped_abilities != {} {
                break ability_drop
            }

            if state.player_state.kills_per_ai[entity.ai_type] < ai_kills_for_ability(entity.ai_type) {
                break ability_drop
            }

            pickup := pickup_type_from_ability(ai_ability(entity.ai_type))
            create_ability_pickup(entity.position, pickup)

            // dont accidently add more then one ability
            assert(card(entity.abilities) == 1)
            state.player_state.dropped_abilities += entity.abilities
        }

        { // drop gems
            gem_count := ai_gem_drop_amount(entity.ai_type)
            for i in 0..<gem_count {
                // normalise to -1 to 1 for unit vector direction
                random_x := (rand.float32() * 2) - 1
                random_y := (rand.float32() * 2) - 1

                direction := linalg.normalize(v2{random_x, random_y})
                position := entity.position + (direction * (entity.size) * 0.5)

                create_gem(position)                
            }
        }
    }
}

create_entity :: proc(entity: Entity) -> ^Entity {
    ptr := &state.entities[state.entity_count]
    state.entity_count += 1

    ptr^ = entity

    ptr.id = state.id_counter
    ptr.created_time = state.time

    state.id_counter += 1

    return ptr
}

create_player :: proc(position: v2) -> ^Entity {
    prefab := create_entity_from_prefab(.player)
    prefab.position = position

    return create_entity(prefab)
}

create_speeder :: proc(position: v2) -> ^Entity {
    prefab := create_entity_from_prefab(.speeder)
    prefab.position = position

    return create_entity(prefab)
}

create_drone :: proc(position: v2) -> ^Entity {
    prefab := create_entity_from_prefab(.drone)
    prefab.position = position

    return create_entity(prefab)
}

create_orc :: proc(position: v2) -> ^Entity {
    prefab := create_entity_from_prefab(.orc)
    prefab.position = position

    return create_entity(prefab)
}

create_nest :: proc(position: v2, cluster_size: int, spawn_rate: f32, speeders_to_spawn: int, drones_to_spawn: int) -> ^Entity {
    total_spawns := speeders_to_spawn + drones_to_spawn

    prefab := create_entity_from_prefab(.nest)

    prefab.position = position
    prefab.cluster_size = cluster_size
    prefab.spawn_rate = spawn_rate
    prefab.speeders_to_spawn = speeders_to_spawn
    prefab.drones_to_spawn = drones_to_spawn
    prefab.total_spawns = total_spawns

    return create_entity(prefab)
}

create_ability_pickup :: proc(position: v2, type: PickupType) -> ^Entity {
    prefab := create_entity_from_prefab(.pickup)
    prefab.pickup_type = type
    prefab.position = position

    return create_entity(prefab)
}

create_bullet :: proc(position: v2, velocity: v2) -> ^Entity {
    return create_entity({
        flags = {.projectile, .trigger_hitbox},
        position = position,
        velocity = velocity,
        size = {5, 5},
        texture = .cuber
    })
}

create_potion :: proc(position: v2, velocity: v2, lifetime: f32) -> ^Entity {
    prefab := create_entity_from_prefab(.potion)
    prefab.position = position
    prefab.velocity = velocity
    prefab.potion_lifetime = lifetime

    return create_entity(prefab)
}

create_spludge :: proc(position: v2) -> ^Entity {
    prefab := create_entity_from_prefab(.spludge)
    prefab.position = position

    return create_entity(prefab)
}

create_gem :: proc(position: v2) -> ^Entity {
    prefab := create_entity_from_prefab(.gem)
    prefab.position = position

    return create_entity(prefab)
}

create_entity_from_prefab :: proc(prefab: Prefab) -> Entity {
    switch prefab {
        case .blank: {
            return Entity {
                flags = {},
                size = {100, 100},
            }
        }
        case .player: {
            return Entity {
                flags = {.player, .has_health, .solid_hitbox},
                size = {50, 50},
                mass = 200,
                texture = .player,
                aim_direction = {0, 1},
                health = MAX_PLAYER_HEALTH,
                max_health = MAX_PLAYER_HEALTH,
            }
        }
        case .nest: {
            return Entity {
                flags = {.nest},
                size = {150, 150},
                texture = .nest,
            }
        }
        case .pickup: {
            return Entity {
                flags = {.interactable, .ability_pickup},
                size = {30, 30},
                pickup_type = PickupType(0),
                texture = .star,
            }
        }
        case .brick_wall: {
            return Entity {
                flags = {.static_hitbox},
                size = {100, 100},
                texture = .brick_wall,
            }
        }
        case .corner_wall: {
            return Entity {
                flags = {.static_hitbox},
                size = {100, 100},
                texture = .corner_wall,
            }
        }
        case .brick_wall_vertical: {
            return Entity {
                flags = {.static_hitbox},
                size = {50, 100},
                texture = .brick_wall_vertical,
            }
        }
        case .brick_wall_corner_left: {
            return Entity {
                flags = {.static_hitbox},
                size = {100, 100},
                texture = .brick_wall_corner_left,
            }
        }
        case .brick_wall_corner_right: {
            return Entity {
                flags = {.static_hitbox},
                size = {100, 100},
                texture = .brick_wall_corner_right,
            }
        }
        case .gem: {
            return Entity {
                flags = {.gem},
                size = {15, 15},
                texture = .gem,
            }
        }
        case .crate: {
            return Entity {
                flags = {.solid_hitbox, .has_health},
                size = {80, 80},
                texture = .crate,
                mass = 20,
                max_health = 50,
                health = 50
            }
        }
        case .door: {
            return Entity {
                flags = {.door, .static_hitbox, .interactable},
                size = {100, 100},
                texture = .door,
            }
        }
        case .speeder: {
            return Entity {
                flags = {.ai, .solid_hitbox, .has_health},
                abilities = {.speed},
                size = {20, 20},
                mass = 1,
                texture = .cuber,
                ai_type = .speeder,
                health = ai_health(.speeder),
                max_health = ai_health(.speeder),
            }
        }
        case .drone: {
            return Entity{
                flags = {.ai, .solid_hitbox, .has_health},
                abilities = {.armour},
                size = {80, 80},
                mass = 80,
                texture = .drone,
                ai_type = .drone,
                health = ai_health(.drone),
                max_health = ai_health(.drone),
                armour = MAX_ARMOUR
            } 
        }
        case .orc: {
            return Entity {
                flags = {.ai, .solid_hitbox, .has_health},
                abilities = {.dash},
                size = {100, 150},
                mass = 200,
                texture = .orc,
                ai_type = .orc,
                health = ai_health(.orc),
                max_health = ai_health(.orc),
            }
        }
        case .wizard: {
            return Entity {
                flags = {.ai, .solid_hitbox, .has_health},
                abilities = {.potion},
                size = {75, 100},
                mass = 20,
                texture = .wizard,
                ai_type = .wizard,
                health = ai_health(.wizard),
                max_health = ai_health(.wizard),
            }
        }
        case .potion: {
            return Entity {
                flags = {.potion, .trigger_hitbox},
                size = {20, 20},
                texture = .potion,
            }
        }
        case .spludge: {
            return Entity {
                flags = {.spludge},
                size = {250, 180},
                texture = .spludge,
            }
        }
    }

    unreachable()
}

get_entity_with_flag :: proc(flag: EntityFlag) -> ^Entity {
    for &entity in state.entities[0:state.entity_count] {
        if flag in entity.flags {
            return &entity
        }
    }

    return nil
}

get_entity_with_id :: proc(id: int) -> ^Entity {
    for &entity in state.entities[0:state.entity_count] {
        if entity.id == id {
            return &entity
        }
    }

    return nil
}

// return an entity that is intersecting with this position WARNING SLOW
get_entity_on_position :: proc(position: v2) -> ^Entity {
    for &entity in state.entities[0:state.entity_count] {
        collided := aabb_collided(position, {1, 1}, entity.position, entity.size)
        if collided {
            return &entity
        }
    }

    return nil
}

entity_take_damage :: proc(entity: ^Entity, damage: f32) {
    assert(.has_health in entity.flags)

    when GOD_MODE {
        if .player in entity.flags {
            return
        }
    }

    if .player in entity.flags {
        if .is_dashing in entity.flags {
            return 
        }
    }

    // if entity has armour take from that first
    // if damage is more then armour left take remainder
    // from the health

    damage_to_health: f32

    if .armour in entity.abilities && entity.armour > 0 {
        entity.armour -= damage
        entity.armour_regen_cooldown = ARMOUR_REGEN_COOLDOWN

        remainder := -entity.armour
        if remainder > 0 {
            damage_to_health = remainder
            entity.armour = 0
        }
    } else {
        damage_to_health = damage
    }

    entity.health -= damage_to_health
    if entity.health < 0 {
        entity.health = 0
    }
}

ai_speed :: proc(type: AiType) -> f32 {
    switch type {
        case .speeder:  return 190 // multiplied by speed ability
        case .drone:    return 220
        case .orc:      return 190
        case .wizard:   return 220
    }

    unreachable()
}

ai_gem_drop_amount :: proc(type: AiType) -> int {
    switch type {
        case .speeder:  return 1 
        case .drone:    return 3
        case .orc:      return 20
        case .wizard:   return 5
    }

    unreachable()
}

ai_ability :: proc(type: AiType) -> Ability {
    switch type {
        case .speeder:  return .speed         
        case .drone:    return .armour
        case .orc:      return .dash         
        case .wizard:   return .dash         
    }

    unreachable()
}

ai_kills_for_ability :: proc(type: AiType) -> int {
    switch type {
        case .speeder:  return 40         
        case .drone:    return 20
        case .orc:      return 10
        case .wizard:   return 10
    }

    unreachable()
}

ai_spawn_chance :: proc(type: AiType) -> f32 {
    switch type {
        case .speeder:  return  0.4         
        case .drone:    return  0.1 
        case .orc:      return  0.5
        case .wizard:   return  0.2 
    }

    unreachable()
}

ai_damage :: proc(type: AiType) -> f32 {
    switch type {
        case .speeder:  return  4
        case .drone:    return  20
        case .orc:      return  150
        case .wizard:   return  0
    }

    unreachable()
}

ai_health :: proc(type: AiType) -> f32 {
    switch type {
        case .speeder:  return  20
        case .drone:    return  140
        case .orc:      return  600
        case .wizard:   return  180
    }

    unreachable()
}

ai_attack_cooldown :: proc(type: AiType) -> f32 {
    switch type {
        case .speeder:  return  0.5
        case .drone:    return  0.5
        case .orc:      unreachable()
        case .wizard:   return  1.8
    }

    unreachable()
}

attack_cooldown_for_weapon_level :: proc(weapon_level: int) -> f32 {
    assert(weapon_level > 0 && weapon_level <= MAX_WEAPON_LEVEL)

    switch weapon_level {
        case 1: return 0.12
        case 2: return 0.07
        case 3: return 0.04
        case 4: return 0.02
    }

    unreachable()
}

damage_for_weapon_level :: proc(weapon_level: int) -> f32 {
    assert(weapon_level > 0 && weapon_level <= MAX_WEAPON_LEVEL)

    switch weapon_level {
        case 1: return 10
        case 2: return 15
        case 3: return 20
        case 4: return 25
    }

    unreachable()
}

gems_needed_for_level :: proc(weapon_level: int) -> int {
    assert(weapon_level > 0 && weapon_level <= MAX_WEAPON_LEVEL)

    switch weapon_level {
        case 1: return 0
        case 2: return 150
        case 3: return 3_000
        case 4: return 10_000
    }

    unreachable()
}

get_aim_input :: proc(relative_position: v2) -> v2 {
    switch state.input_mode {
        case .gamepad: {
            return {
                state.gamepad.axes[glfw.GAMEPAD_AXIS_RIGHT_X],
                -state.gamepad.axes[glfw.GAMEPAD_AXIS_RIGHT_Y]
            }
        }
        case .mouse_and_keyboard: {
            mouse_world_position := screen_position_to_world_position(state.mouse_position)
            aim_vector := mouse_world_position - relative_position
            
            // get NaN if the length is 0
            if linalg.length(aim_vector) != 0 {
                return linalg.normalize(aim_vector)
            } else {
                return {}
            }
        }
    }

    unreachable()
}

get_movement_input :: proc() -> v2 {
    switch state.input_mode {
        case .gamepad: {
            return {
                state.gamepad.axes[glfw.GAMEPAD_AXIS_LEFT_X],
                -state.gamepad.axes[glfw.GAMEPAD_AXIS_LEFT_Y] // inverted for some reason ??
            }
        }
        case .mouse_and_keyboard: {
            input: v2

            if state.keys[glfw.KEY_A] == .pressed {
                input.x -= 1
            }
            
            if state.keys[glfw.KEY_D] == .pressed {
                input.x += 1
            }

            if state.keys[glfw.KEY_W] == .pressed {
                input.y += 1
            }
            
            if state.keys[glfw.KEY_S] == .pressed {
                input.y -= 1
            }

            return input
        }
    }

    unreachable()
}

get_shooting_input :: proc() -> f32 {
    switch state.input_mode {
        case .gamepad: {
            // trigger input is from -1 to -1, -1 being no input
            // and 1 being fulling pressed, need to convert this to 0 -> 1
            raw_trigger_input := state.gamepad.axes[glfw.GAMEPAD_AXIS_RIGHT_TRIGGER]
            return (raw_trigger_input + 1) * 0.5
        }
        case .mouse_and_keyboard: {
            if state.mouse[glfw.MOUSE_BUTTON_LEFT] == .pressed {
                return 1
            } else {
                return 0 
            }
        }
    }

    unreachable()
}

get_interact_input :: proc() -> InputState {
    switch state.input_mode {
        case .gamepad: {
            // returning "down" which normally means just pressing once
            // but checking for press, need to track state of gamepad button
            // presses the same way we do for keys but this is fine for now
            // - 25/01/25
            if state.gamepad.buttons[glfw.GAMEPAD_BUTTON_X] == glfw.PRESS {
                return .down
            }

            return .up 
        }
        case .mouse_and_keyboard: {
            return state.keys[glfw.KEY_E] 
        }
    }

    unreachable()
}

get_dash_input :: proc() -> InputState {
    switch state.input_mode {
        case .gamepad: {
            if state.gamepad.buttons[glfw.GAMEPAD_BUTTON_RIGHT_BUMPER] == glfw.PRESS {
                return .down
            }

            return .up 
        }
        case .mouse_and_keyboard: {
            return state.keys[glfw.KEY_F] 
        }
    }

    unreachable()
}

// returns overlap, distance and if collided 
aabb :: proc(position_a: v2, size_a: v2, position_b: v2, size_b: v2) -> (v2, v2, bool) {
    distance := position_b - position_a
    distance_abs := v2{abs(distance.x), abs(distance.y)}
    distance_for_collision := (size_a + size_b) * v2{0.5, 0.5}

    collision := distance_for_collision[0] >= distance_abs[0] && distance_for_collision[1] >= distance_abs[1]
    overlap_amount := distance_for_collision - distance_abs

    return overlap_amount, distance, collision
}

aabb_collided :: proc(position_a: v2, size_a: v2, position_b: v2, size_b: v2) -> bool {
    _, _, collided := aabb(position_a, size_a, position_b, size_b)
    return collided 
}

distance_between_entity_edges :: proc(entity_a: ^Entity, entity_b: ^Entity) -> v2 {
    center_distance := linalg.abs(entity_a.position - entity_b.position)
    edge_distance := center_distance - ((entity_a.size + entity_b.size) * 0.5)

    return edge_distance
}

on_trigger_collision :: proc(trigger: ^Entity, other: ^Entity) {
    if .has_health in other.flags {
        entity_take_damage(other, damage_for_weapon_level(state.player_state.weapon_level)) 
    }
}

ability_from_pickup_type :: proc(type: PickupType) -> Ability {
    switch type {
        case .armour:   return .armour
        case .speed:    return .speed
        case .dash:     return .dash
        case .potion:   return .potion
    }

    unreachable()
}

pickup_type_from_ability :: proc(ability: Ability) -> PickupType {
    switch ability {
        case .armour:   return .armour
        case .speed:    return .speed
        case .dash:     return .dash
        case .potion:   return .potion
    }

    unreachable()
}

BoxColliderIterator :: struct {
    index: int,
    position: v2,
    size: v2
}

new_box_collider_iterator :: proc(position: v2, size: v2) -> BoxColliderIterator {
    return BoxColliderIterator {
        index = 0,
        position = position,
        size = size,
    }
}

next :: proc(iterator: ^BoxColliderIterator) -> ^Entity {
    for iterator.index < state.entity_count {
        other := &state.entities[iterator.index]
        iterator.index += 1
        
        collision := aabb_collided(iterator.position, iterator.size, other.position, other.size)
        if collision {
            return other
        }
    }

    return nil
}

// -------------------------- @editor -----------------------
Editor :: struct {
    live_level: bool,
    use_grid: bool,
    grid_size: v2,
    use_highlight: bool,
    camera_move_speed: f32,
    entity_move_speed: f32,
    selected_entity_id: int,
}

update_editor :: proc() {
    { // camera contols
        move_input: v2

        if state.keys[glfw.KEY_A] == .pressed {
            move_input.x -= 1
        }

        if state.keys[glfw.KEY_D] == .pressed {
            move_input.x += 1
        }

        if state.keys[glfw.KEY_W] == .pressed {
            move_input.y += 1
        }

        if state.keys[glfw.KEY_S] == .pressed {
            move_input.y -= 1
        }

        if linalg.length(move_input) != 0 {
            move_amount := move_input * state.editor.camera_move_speed
            state.camera.position += move_amount
        }
    }

    { // clicking
        if state.mouse[glfw.MOUSE_BUTTON_LEFT] == .down {
            mouse_world_position := screen_position_to_world_position(state.mouse_position)
            new_selected_entity := get_entity_on_position(mouse_world_position)
            if new_selected_entity != nil {
                state.editor.selected_entity_id = new_selected_entity.id
            }
        }
    }

    update_selected_entity: {
        selected_entity := get_entity_with_id(state.editor.selected_entity_id)
        if selected_entity == nil {
            break update_selected_entity
        }

        { // move selected entity
            desired_key_state: InputState = .down if state.editor.use_grid else .pressed
    
            move_input: v2
    
            if state.keys[glfw.KEY_LEFT] == desired_key_state {
                move_input.x -= 1
            }
    
            if state.keys[glfw.KEY_RIGHT] == desired_key_state {
                move_input.x += 1
            }
    
            if state.keys[glfw.KEY_UP] == desired_key_state {
                move_input.y += 1
            }
    
            if state.keys[glfw.KEY_DOWN] == desired_key_state {
                move_input.y -= 1
            }
    
            if linalg.length(move_input) != 0 {
                if state.editor.use_grid {
                    grid_index := selected_entity.position / state.editor.grid_size
    
                    grid_index.x = math.trunc(grid_index.x)
                    grid_index.y = math.trunc(grid_index.y)
    
                    grid_index += move_input
                    selected_entity.position = grid_index * state.editor.grid_size
                } else {
                    move_amount := move_input * state.editor.entity_move_speed
                    selected_entity.position += move_amount
                }
            }

            SCROLL_SPEED :: 80
            MIN_SIZE     :: 5

            // zoom in
            if state.mouse_scroll > 0 {
                state.camera.orthographic_size -= SCROLL_SPEED
            }

            if state.mouse_scroll < 0 {
                state.camera.orthographic_size += SCROLL_SPEED
            }

            if state.camera.orthographic_size < MIN_SIZE {
                state.camera.orthographic_size = MIN_SIZE
            }
        }

        { // rotate selected entity
            if state.keys[glfw.KEY_LEFT_SHIFT] == .pressed && state.keys[glfw.KEY_R] == .down {
                selected_entity.rotation -= 90
            }
            else if state.keys[glfw.KEY_R] == .down {
                selected_entity.rotation += 90
            }
        }

        { // duplicate selected entity
            if state.keys[glfw.KEY_SPACE] == .down {
                copy := selected_entity^
                new_entity := create_entity(copy)
                state.editor.selected_entity_id = new_entity.id
            }
        }

        { // delete selected entity
            if state.keys[glfw.KEY_DELETE] == .down {
                // do this to get an imediate deletion, setting the flag
                // means you need to wait for an update cycle in game
                for index in 0..<state.entity_count {
                    if state.entities[index].id == selected_entity.id {
                        delete_entity_from_buffer(index)
                        state.editor.selected_entity_id = -1
                    }
                }
            }
        }
    }
}

editor_ui :: proc() {
    selected_entity := get_entity_with_id(state.editor.selected_entity_id)
    if selected_entity == nil {
        state.editor.selected_entity_id = -1
    }

    if state.editor.use_highlight && selected_entity != nil {
        draw_rectangle(selected_entity.position, selected_entity.size * 1.5, alpha(GREEN, 0.2))

        if .nest in selected_entity.flags {
            draw_circle(selected_entity.position, selected_entity.spawn_radius, alpha(BLUE, 0.2))
        }
    }

    if state.editor.use_grid {
        GRID_WIDTH  :: 500
        GRID_HEIGHT :: 300
        LINE_WIDTH  :: 3

        x_size := state.editor.grid_size.x
        start_x := -((GRID_WIDTH / 2) * x_size) 

        y_size := state.editor.grid_size.y
        start_y := -((GRID_HEIGHT / 2) * y_size) 

        for x in 0..<GRID_WIDTH {
            draw_rectangle({start_x + f32(x) * x_size, 0}, {LINE_WIDTH, GRID_HEIGHT * y_size}, alpha(RED, 0.2))
        }

        for y in 0..<GRID_HEIGHT {
            draw_rectangle({0, start_y + f32(y) * y_size}, {GRID_WIDTH * x_size, LINE_WIDTH}, alpha(RED, 0.2))
        }

        draw_circle({}, 15, alpha(BLUE, 0.4))
    }

    in_screen_space = true

    { // mode text
        size : f32 = 25
        position := v2{state.width * 0.5, state.height - size}

        if state.editor.live_level {
            draw_rectangle(position, {300, size + 10}, RED)
        }

        draw_text("Editor", position, size, WHITE, .center)
    } 

    { // inspector imgui window
        // imgui.ShowDemoWindow()

	if imgui.Begin("Inspector", flags = {.NoCollapse}) {
            if !state.editor.live_level && imgui.Button("Save Level") {
                ok := save_level(LEVEL_SAVE_NAME)
                if !ok {
                    log.error("failed to save level")
                }
            }

            imgui.SameLine()

            if imgui.Button("Load Level") {
                ok := load_level(LEVEL_SAVE_NAME)
                if !ok {
                    log.error("failed to load level")
                }
            }

            if imgui.CollapsingHeader("Editor") {
                imgui.Indent()
                defer imgui.Unindent()

                if imgui.Button("25x100") {
                    state.editor.grid_size = {25, 100}
                }

                imgui.Checkbox("Use grid", &state.editor.use_grid)
                if state.editor.use_grid {
                    imgui.InputFloat2("grid size", &state.editor.grid_size)
                }

                imgui.Checkbox("Use highlight", &state.editor.use_highlight)
                imgui.SliderFloat("camera speed", &state.editor.camera_move_speed, 0, 15)
                imgui.SliderFloat("entity speed", &state.editor.entity_move_speed, 0, 15)
            }

            if imgui.CollapsingHeader("State") {
                imgui.Indent()
                defer imgui.Unindent()

                imgui.Text("width: %f", state.width)
                imgui.Text("height: %f", state.height)
                imgui.Text("height: %f", state.height)
                {
                    label := fmt.tprintf("input mode: %v", state.input_mode)
                    c_label := strings.clone_to_cstring(label, context.temp_allocator)
                    imgui.Text(c_label)
                }
                imgui.Text("mouse position: %f, %f", state.mouse_position.x, state.mouse_position.y)
                imgui.Text("time: %f", state.time)
                imgui.InputFloat2("camera position", &state.camera.position)
                imgui.SliderFloat("orthographic size", &state.camera.orthographic_size, 10, 5_000)
                imgui.InputFloat("near plane", &state.camera.near_plane)
                imgui.InputFloat("far plane", &state.camera.far_plane)
                imgui.Text("entity count: %d", state.entity_count)
                imgui.Text("id counter: %d", state.id_counter)
                imgui.Text("quad count: %d", state.renderer.quad_count)
            }

            if imgui.CollapsingHeader("Prefabs") {
                imgui.Indent()
                defer imgui.Unindent()

                for prefab in Prefab {
                    label := fmt.tprintf("Create %v", prefab)
                    c_label := strings.clone_to_cstring(label, context.temp_allocator)

                    create_prefab := imgui.Button(c_label)

                    if create_prefab {
                        prefab_instance := create_entity_from_prefab(prefab)
                        entity_instance := create_entity(prefab_instance)
                        entity_instance.position = state.camera.position
                        state.editor.selected_entity_id = entity_instance.id
                    }
                }
            }

            entity_editor:
            if imgui.CollapsingHeader("Entity Editor") {
                imgui.Indent()
                defer imgui.Unindent()

                if selected_entity == nil {
                    imgui.Text("No Entity selected")
                    break entity_editor
                }

                if imgui.Button("Unselect") {
                    state.editor.selected_entity_id = -1
                    break entity_editor
                }

                imgui.SameLine()

                if imgui.Button("Delete") {
                    // do this to get an imediate deletion, setting the flag
                    // means you need to wait for an update cycle in game
                    for index in 0..<state.entity_count {
                        if state.entities[index].id == selected_entity.id {
                            delete_entity_from_buffer(index)
                            break entity_editor
                        }
                    }
                }

                if imgui.Button("Rotate CCW") {
                    selected_entity.rotation -= 90
                }

                imgui.SameLine()

                if imgui.Button("Rotate CW") {
                    selected_entity.rotation += 90
                }

                if imgui.Button("Set size as grid") {
                    state.editor.grid_size = selected_entity.size
                }

                imgui.Separator()

                imgui.Text("id: %d", c.int(selected_entity.id))
                    
                if imgui.CollapsingHeader("Flags") {
                    for flag in EntityFlag {
                        label := fmt.tprintf("%v", flag)
                        c_label := strings.clone_to_cstring(label, context.temp_allocator)
         
                        checked := flag in selected_entity.flags
                        imgui.Checkbox(c_label, &checked)
         
                        if checked {
                            selected_entity.flags += {flag}
                        } else {
                            selected_entity.flags -= {flag}
                        }
                    }
                }
    
                if imgui.CollapsingHeader("Abilities") {
                    for ability in Ability {
                        label := fmt.tprintf("%v", ability)
                        c_label := strings.clone_to_cstring(label, context.temp_allocator)
         
                        checked := ability in selected_entity.abilities
                        imgui.Checkbox(c_label, &checked)
         
                        if checked {
                            selected_entity.abilities += {ability}
                        } else {
                            selected_entity.abilities -= {ability}
                        }
                    }
                }
    
                imgui.Text("created time: %f", selected_entity.created_time)
                imgui.Checkbox("only boss battle", &selected_entity.only_boss_battle)
                imgui.InputFloat2("position", &selected_entity.position)
                imgui.InputFloat2("size", &selected_entity.size)
                imgui.InputFloat2("velocity", &selected_entity.velocity)
                imgui.InputFloat("rotation", &selected_entity.rotation)
                imgui.InputFloat("mass", &selected_entity.mass)
                    
                if imgui.CollapsingHeader("Texture") {
                    current := i32(selected_entity.texture)
                    for texture, i in TextureHandle {
                        label := fmt.tprintf("%v", texture)
                        c_label := strings.clone_to_cstring(label, context.temp_allocator)
                        
                        selected := imgui.RadioButton(c_label, i32(i) == current) 
                        if selected {
                            selected_entity.texture = texture
                        }
                    }
                }
    
                imgui.InputFloat("attack cooldown", &selected_entity.attack_cooldown)
                imgui.InputFloat2("aim direction", &selected_entity.aim_direction)

    
                imgui.InputFloat("health", &selected_entity.health)
                imgui.InputFloat("max health", &selected_entity.max_health)
    
                imgui.InputFloat("armour", &selected_entity.armour)
                imgui.InputFloat("armour regen cooldown", &selected_entity.armour_regen_cooldown) 
                    
                if imgui.CollapsingHeader("Ai type") {
                    current := i32(selected_entity.ai_type)
    
                    for type, i in AiType {
                        label := fmt.tprintf("%v", type)
                        c_label := strings.clone_to_cstring(label, context.temp_allocator)
    
                        selected := imgui.RadioButton(c_label, i32(i) == current) 
                        if selected {
                            selected_entity.ai_type = type
                        }
                    }
                }
    
                imgui.InputScalar("cluster size", .S64, &selected_entity.cluster_size, format = "%d")
                imgui.InputFloat("spawn rate", &selected_entity.spawn_rate)
                imgui.InputFloat("spawn cooldown", &selected_entity.spawn_cooldown)
                imgui.InputScalar("speeders to spawn", .S64, &selected_entity.speeders_to_spawn, format = "%d")
                imgui.InputScalar("drones to spawn", .S64, &selected_entity.drones_to_spawn, format = "%d")
                imgui.InputScalar("orcs to spawn", .S64, &selected_entity.orcs_to_spawn, format = "%d")
                imgui.InputScalar("total spawns", .S64, &selected_entity.total_spawns, format = "%d")
                imgui.SliderFloat("spawn radius", &selected_entity.spawn_radius, 0, 2000)
    
                if imgui.CollapsingHeader("pickup type") {
                    current := i32(selected_entity.pickup_type)
    
                    for type, i in PickupType {
                        label := fmt.tprintf("%v", type)
                        c_label := strings.clone_to_cstring(label, context.temp_allocator)
    
                        selected := imgui.RadioButton(c_label, i32(i) == current) 
                        if selected {
                            selected_entity.pickup_type = type
                        }
                    }
                }
            }

	    imgui.End()
        }
    }
}

// -------------------------- @renderer -----------------------
v2 :: [2]f32
v3 :: [3]f32
v4 :: [4]f32
Mat4 :: linalg.Matrix4f32

GL_MAJOR :: 4
GL_MINOR :: 6

Vertex :: struct {
    position: v3,
    colour: v4,
    highlight_colour: v4,
    uv: v2,
    draw_type: i32,
}

Quad :: struct {
    vertices: [4]Vertex
}

DEFAULT_UV :: [4]v2 {
    {0, 1},
    {1, 1},
    {1, 0},
    {0, 0}
}

DrawType :: enum {
    rectangle,
    circle,
    texture,
    font
}

TextureHandle :: enum {
    cuber,
    player,
    armour,
    star,
    x_button,
    drone,
    nest,
    brick_wall,
    corner_wall,
    brick_wall_vertical,
    brick_wall_corner_left,
    brick_wall_corner_right,
    gem,
    crate,
    door,
    window,
    orc,
    wizard,
    potion,
    spludge,
    heart,
    armour_bar,
    fireball,
    dash_icon,
    speed_lines,
}

Texture :: struct {
    width: int,
    height: int,
    aspect_ratio: f32,
    uv: [4]v2,
    data: [^]byte
}

Atlas :: struct {
    width: int,
    height: int,
    data: [^]byte
}

FontHandle :: enum {
    alagard,
    baskerville
}

Font :: struct {
    characters: []stbtt.bakedchar,
    bitmap: [^]byte,
    bitmap_width: int,
    bitmap_height: int,
    filter: TextureFilter,
}

TextureFilter :: enum {
    nearest,
    linear
}

TextAllignment :: enum {
    center,
    bottom_left
}

RED         :: v4{1, 0, 0, 1}
GREEN       :: v4{0, 1, 0, 1}
BLUE        :: v4{0, 0, 1, 1}

WHITE       :: v4{1, 1, 1, 1}
BLACK       :: v4{0, 0, 0, 1}

YELLOW      :: v4{0.95, 0.97, 0, 1}
SKY_BLUE    :: v4{0.45, 0.8, 0.75, 1}

Renderer :: struct {
    quads: []Quad,
    quad_count: int,

    texture_atlas: Atlas,
    textures: [TextureHandle]Texture,
    font: Font,

    vertex_array_id: u32,
    vertex_buffer_id: u32,
    index_buffer_id: u32,
    shader_program_id: u32,
    atlas_texture_id: u32,
    font_texture_id: u32,
}

in_screen_space := false

init_imgui :: proc(renderer: ^Renderer) -> bool {
    imgui.CHECKVERSION()
	imgui.CreateContext()

	io := imgui.GetIO()
	io.ConfigFlags += {.NavEnableKeyboard, .NavEnableGamepad}

	io.ConfigFlags += {.DockingEnable}
	io.ConfigFlags += {.ViewportsEnable}

	style := imgui.GetStyle()
	style.WindowRounding = 1
	style.Colors[imgui.Col.WindowBg].w = 1
	imgui.StyleColorsLight()

	imgui_impl_glfw.InitForOpenGL(state.window, true)
	imgui_impl_opengl3.Init("#version 150")

    return true
}

init_renderer :: proc(renderer: ^Renderer) -> bool {
    { // initialise opengl
        gl.load_up_to(GL_MAJOR, GL_MINOR, glfw.gl_set_proc_address)

        when OPENGL_MESSAGES {
            gl.DebugMessageCallback(opengl_message_callback, nil)
        }
    
        // blend settings
        gl.Enable(gl.BLEND)
        gl.BlendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA)
    
        V :: 0
        gl.ClearColor(V, V, V, 1)
    }

    { // shaders 
        BUFFER_SIZE :: 512
        compile_status: i32
        link_status: i32
        error_buffer: [BUFFER_SIZE]u8

        vertex_shader_source := #load("./shaders/vertex.shader", cstring)
        fragment_shader_source := #load("./shaders/fragment.shader", cstring)

        vertex_shader := gl.CreateShader(gl.VERTEX_SHADER)
        defer gl.DeleteShader(vertex_shader)

        gl.ShaderSource(vertex_shader, 1, &vertex_shader_source, nil)
        gl.CompileShader(vertex_shader)

        gl.GetShaderiv(vertex_shader, gl.COMPILE_STATUS, &compile_status)
        if compile_status == 0 {
            gl.GetShaderInfoLog(vertex_shader, BUFFER_SIZE, nil, &error_buffer[0])
            log.errorf("failed to compile vertex shader: %v", strings.string_from_ptr(&error_buffer[0], 512))
            return false
        }

        fragment_shader := gl.CreateShader(gl.FRAGMENT_SHADER)
        defer gl.DeleteShader(fragment_shader)

        gl.ShaderSource(fragment_shader, 1, &fragment_shader_source, nil)
        gl.CompileShader(fragment_shader)

        gl.GetShaderiv(fragment_shader, gl.COMPILE_STATUS, &compile_status)
        if compile_status == 0 {
            gl.GetShaderInfoLog(fragment_shader, BUFFER_SIZE, nil, &error_buffer[0])
            log.errorf("failed to compile fragment shader: %v", strings.string_from_ptr(&error_buffer[0], 512))
            return false
        }

        shader_program := gl.CreateProgram()
        gl.AttachShader(shader_program, vertex_shader)
        gl.AttachShader(shader_program, fragment_shader)
        gl.LinkProgram(shader_program)
            
        gl.GetProgramiv(shader_program, gl.LINK_STATUS, &link_status);
        if link_status == 0 {
            gl.GetProgramInfoLog(shader_program, BUFFER_SIZE, nil, &error_buffer[0]);
            log.errorf("failed to link shader program: %v", strings.string_from_ptr(&error_buffer[0], 512))
            return false
        }

        // sets which uniform is asigned to which texture slot in the fragment shader
        gl.UseProgram(shader_program)
        gl.Uniform1i(gl.GetUniformLocation(shader_program, "face_texture"), 0)
        gl.Uniform1i(gl.GetUniformLocation(shader_program, "font_texture"), 1)

        renderer.shader_program_id = shader_program
    }

    { // vertex array
        vertex_array: u32
        gl.GenVertexArrays(1, &vertex_array)
        gl.BindVertexArray(vertex_array)

        renderer.vertex_array_id = vertex_array
    }

    { // vertex buffer
        vertex_buffer: u32
        gl.GenBuffers(1, &vertex_buffer)
            
        gl.BindBuffer(gl.ARRAY_BUFFER, vertex_buffer)
        gl.BufferData(gl.ARRAY_BUFFER, size_of(Quad) * len(renderer.quads), &renderer.quads[0], gl.DYNAMIC_DRAW)

        renderer.vertex_buffer_id = vertex_buffer
    }

    { // index buffer
        // get copied to gpu so only need temp allocator - 04/01/25
        indices := make([]u32, MAX_QUADS * 6, context.temp_allocator)

        i := 0
        for i < len(indices) {
            // vertex offset pattern to draw a quad
            // { 0, 1, 2,  0, 2, 3 }
            indices[i + 0] = auto_cast ((i/6)*4 + 0)
            indices[i + 1] = auto_cast ((i/6)*4 + 1)
            indices[i + 2] = auto_cast ((i/6)*4 + 2)
            indices[i + 3] = auto_cast ((i/6)*4 + 0)
            indices[i + 4] = auto_cast ((i/6)*4 + 2)
            indices[i + 5] = auto_cast ((i/6)*4 + 3)
            i += 6
        }

        index_buffer: u32
        gl.GenBuffers(1, &index_buffer)

        gl.BindBuffer(gl.ELEMENT_ARRAY_BUFFER, index_buffer)
        gl.BufferData(gl.ELEMENT_ARRAY_BUFFER, size_of(u32) * len(indices), &indices[0], gl.STATIC_DRAW)

        renderer.index_buffer_id = index_buffer
    }

    { // attributes
        // attribute index, component count, component type, normalised, object size, attribute offset in object
        gl.VertexAttribPointer(0, 3, gl.FLOAT, gl.FALSE, size_of(Vertex), 0)                            // position
        gl.VertexAttribPointer(1, 4, gl.FLOAT, gl.FALSE, size_of(Vertex), 3 * size_of(f32))             // colour
        gl.VertexAttribPointer(2, 4, gl.FLOAT, gl.FALSE, size_of(Vertex), (3 + 4) * size_of(f32))       // highlight colour
        gl.VertexAttribPointer(3, 2, gl.FLOAT, gl.FALSE, size_of(Vertex), (3 + 4 + 4) * size_of(f32))   // uv
        gl.VertexAttribIPointer(4, 1, gl.INT, size_of(Vertex), (3 + 4 + 4 + 2) * size_of(f32))          // draw type

        gl.EnableVertexAttribArray(0)
        gl.EnableVertexAttribArray(1)
        gl.EnableVertexAttribArray(2)
        gl.EnableVertexAttribArray(3)
        gl.EnableVertexAttribArray(4)
    }

    renderer.atlas_texture_id = send_bitmap_to_gpu(renderer, renderer.texture_atlas.width, renderer.texture_atlas.height, renderer.texture_atlas.data, .nearest, .rgba)
    renderer.font_texture_id = send_bitmap_to_gpu(renderer, renderer.font.bitmap_width, renderer.font.bitmap_height, renderer.font.bitmap, .linear, .r)

    return true
}

send_bitmap_to_gpu :: proc(renderer: ^Renderer, width: int, height: int, data: [^]byte, filter: TextureFilter, format: enum {r, rgba}) -> u32 {
    gl_filter: i32 
    gl_format: i32

    switch filter {
        case .nearest:  gl_filter = gl.NEAREST
        case .linear:   gl_filter = gl.LINEAR
    }

    switch format {
        case .r:        gl_format = gl.RED
        case .rgba:     gl_format = gl.RGBA
    }

    texture_id: u32

    gl.GenTextures(1, &texture_id) 

    gl.BindTexture(gl.TEXTURE_2D, texture_id) 
    gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT) // s is x wrap
    gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.REPEAT) // t is y wrap

    gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl_filter)
    gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl_filter)

    gl.TexImage2D(gl.TEXTURE_2D, 0, gl_format, i32(width), i32(height), 0, u32(gl_format), gl.UNSIGNED_BYTE, data)

    return texture_id
}

unload_bitmap_on_gpu :: proc(renderer: ^Renderer, id: u32) {
    _id := id
    gl.DeleteTextures(1, &_id)
}

reload_textures :: proc(renderer: ^Renderer) -> bool {
    ok: bool

    log.info("reloading all textures")

    load_textures(renderer) or_return
    build_texture_atlas(renderer) or_return

    unload_bitmap_on_gpu(renderer, renderer.atlas_texture_id)
    unload_bitmap_on_gpu(renderer, renderer.font_texture_id)

    renderer.atlas_texture_id = send_bitmap_to_gpu(renderer, renderer.texture_atlas.width, renderer.texture_atlas.height, renderer.texture_atlas.data, .nearest, .rgba)
    renderer.font_texture_id = send_bitmap_to_gpu(renderer, renderer.font.bitmap_width, renderer.font.bitmap_height, renderer.font.bitmap, .linear, .r)

    return true 
}

// microui_text_width_callback :: proc(font: mui.Font, str: string) -> i32 {
    // return 100
// }
// 
// microui_text_height_callback :: proc(font: mui.Font) -> i32 {
    // return 10
// }

draw_rectangle :: proc(position: v2, size: v2, colour: v4) {
    draw_quad(position, size, 0, colour, {}, DEFAULT_UV, .rectangle)
}

draw_texture :: proc(texture: TextureHandle, position: v2, size: v2, rotation : f32 = 0,  colour := WHITE, highlight_colour := WHITE) {
    draw_quad(position, size, rotation, colour, highlight_colour, state.renderer.textures[texture].uv, .texture)
}

draw_circle :: proc(position: v2, radius: f32, colour: v4) {
    draw_quad(position, {radius * 2, radius * 2}, 0, colour, {}, DEFAULT_UV, .circle)
}

draw_text :: proc(text: string, position: v2, font_size: f32, colour: v4, allignment: TextAllignment) {
    if len(text) == 0 {
        return
    }

    Glyph :: struct {
        position: v2,
        size: v2,
        uvs: [4]v2,
    }

    glyphs := make([]Glyph, len(text), context.temp_allocator)

    total_text_width: f32
    text_height: f32

    for c, i in text {
        advanced_x: f32
        advanced_y: f32
    
        alligned_quad: stbtt.aligned_quad

        // this is the the data for the aligned_quad we're given, with y+ going down
        //	   x0, y0       x1, y0
        //     s0, t0       s1, t0
        //	    o tl        o tr
        
        
        //     x0, y1      x1, y1
        //     s0, t1      s1, t1
        //	    o bl        o br
        // 
        // x, and y and expected vertex positions
        // s and t are texture uv position
        stbtt.GetBakedQuad(&state.renderer.font.characters[0], i32(state.renderer.font.bitmap_width), i32(state.renderer.font.bitmap_height), i32(c) - 32, &advanced_x, &advanced_y, &alligned_quad, false)

        bottom_y := -alligned_quad.y1
        top_y := -alligned_quad.y0

        height := top_y - bottom_y
        width := alligned_quad.x1 - alligned_quad.x0
        
        if height > text_height {
            text_height = height
        }

        top_left_uv     := v2{alligned_quad.s0, alligned_quad.t0}
        top_right_uv    := v2{alligned_quad.s1, alligned_quad.t0}
        bottom_right_uv := v2{alligned_quad.s1, alligned_quad.t1}
        bottom_left_uv  := v2{alligned_quad.s0, alligned_quad.t1}

        glyphs[i] = {
            position = {
                total_text_width,
                bottom_y,
            },
            size = {
                width,
                height,
            },
            uvs = {
                top_left_uv,
                top_right_uv,
                bottom_right_uv,
                bottom_left_uv
            }
        }
          
        // if the character is not the last then add the advanced x to the total width
        // because this includes the with of the character and also the kerning gap added
        // for the next character, if it is the last one then just take the width and have
        // no extra gap at the end - 20/01/25
        if i < len(text) - 1 {
            total_text_width += advanced_x
        } else {
            total_text_width += width
        }
    }


    pivot_point_translation: v2
    scale := font_size / text_height

    switch allignment {
        case .center: {
            bounding_box := v2{total_text_width, text_height}
            pivot_point_translation = (-bounding_box * 0.5) * scale
        }
        case .bottom_left:
            // characters are aligned by default so do nothing...
    }

    for &glyph in glyphs {
        scaled_position     := glyph.position * scale // needs to be scaled because gaps between characters need to scale also
        scaled_size         := glyph.size * scale
        translated_position := scaled_position + pivot_point_translation + position

        // draw quad needs position to be centre of quad so just convert that here
        draw_quad(translated_position + (scaled_size * 0.5), scaled_size, 0, colour, {}, glyph.uvs, .font);
    } 
}

draw_quad :: proc(position: v2, size: v2, rotation: f32, colour: v4, highlight_colour: v4, uv: [4]v2, draw_type: DrawType) {
    transformation_matrix: Mat4

    if in_screen_space {
        ndc_position := screen_position_to_ndc({position.x, position.y, 0})
        ndc_size := size / (v2{state.width, state.height} * 0.5)         
        transformation_matrix = linalg.matrix4_translate(ndc_position) * linalg.matrix4_scale(v3{ndc_size.x, ndc_size.y, 1})
    } else {
        // model matrix
        transformation_matrix = linalg.matrix4_translate(v3{position.x, position.y, 10}) * 
        linalg.matrix4_rotate_f32(-linalg.to_radians(rotation), {0, 0, 1}) *
        linalg.matrix4_scale(v3{size.x, size.y, 1})
    
        // model view matrix
        transformation_matrix = get_view_matrix() * transformation_matrix
    
        // model view projection
        transformation_matrix = get_projection_matrix() * transformation_matrix
    }

    quad := &state.renderer.quads[state.renderer.quad_count]
    state.renderer.quad_count += 1

    quad.vertices[0].position = (transformation_matrix * v4{-0.5,  0.5, 0, 1}).xyz  // top left
    quad.vertices[1].position = (transformation_matrix * v4{ 0.5,  0.5, 0, 1}).xyz  // top right
    quad.vertices[2].position = (transformation_matrix * v4{ 0.5, -0.5, 0, 1}).xyz  // bottom right
    quad.vertices[3].position = (transformation_matrix * v4{-0.5, -0.5, 0, 1}).xyz  // bottomleft

    quad.vertices[0].colour = colour
    quad.vertices[1].colour = colour
    quad.vertices[2].colour = colour
    quad.vertices[3].colour = colour

    quad.vertices[0].highlight_colour = highlight_colour
    quad.vertices[1].highlight_colour = highlight_colour
    quad.vertices[2].highlight_colour = highlight_colour
    quad.vertices[3].highlight_colour = highlight_colour

    quad.vertices[0].uv = uv[0]
    quad.vertices[1].uv = uv[1]
    quad.vertices[2].uv = uv[2]
    quad.vertices[3].uv = uv[3]

    draw_type_value: i32
    switch draw_type {
        case .rectangle:
            draw_type_value = 0
        case .circle:
            draw_type_value = 1
        case .texture:
            draw_type_value = 2
        case .font:
            draw_type_value = 3
    }

    quad.vertices[0].draw_type = draw_type_value
    quad.vertices[1].draw_type = draw_type_value
    quad.vertices[2].draw_type = draw_type_value
    quad.vertices[3].draw_type = draw_type_value
}

screen_position_to_world_position :: proc(screen_position: v2) -> v2 {
    ndc_vec_3 := screen_position_to_ndc({screen_position.x, screen_position.y, state.camera.near_plane})
    ndc_position := v4{ndc_vec_3.x, ndc_vec_3.y, ndc_vec_3.z, 1}
    aspect_ratio := state.width / state.height

    inverse_vp := linalg.inverse(get_projection_matrix() *  get_view_matrix())
    world_position := inverse_vp * ndc_position
    world_position /= world_position.w

    return {world_position.x, world_position.y}
}

screen_position_to_ndc :: proc(position: v3) -> v3 {
    // the z co-ordinate is not the same in every graphics api
    // this is currently assuming d3d so the z value is normalised
    // between 0 -> 1 based on its distance in the camera near and
    // far planes. For open gl this would need to be -1 -> 1
    // for others e.g. metal I do not know
    // - 11/01/25

    // just using -1 for z for near plane until layers are setup again

    assert(state.camera.near_plane < state.camera.far_plane)

    distance_from_near_plane := position.z - state.camera.near_plane
    distance_between_planes := state.camera.far_plane - state.camera.near_plane
    z_in_ndc := distance_from_near_plane / distance_between_planes

    return {
        ((position.x / state.width) * 2) - 1,
        ((position.y / state.height) * 2) - 1,
        -1 
    }
}

get_view_matrix :: proc() -> Mat4 {
    // the comments descibing what these are is what the internet says but for some reason it acts the 
    // oppisite so the values for eye and centre are flipped
    return linalg.matrix4_look_at_f32(
        {state.camera.position.x, state.camera.position.y, 1},      // camera position
        {state.camera.position.x, state.camera.position.y, 0},      // what it is looking at
        {0, 1, 0}						    // what is considered "up"
    )
}

get_projection_matrix :: proc() -> Mat4 {
    aspect_ratio := f32(state.width) / f32(state.height)
    size := state.camera.orthographic_size

    return linalg.matrix_ortho3d_f32(
        -size * aspect_ratio, 
        size * aspect_ratio, 
        -size, size,
        state.camera.near_plane, state.camera.far_plane, false
    )
}

load_textures :: proc(renderer: ^Renderer) -> bool {
    RESOURCE_DIR :: "resources/textures/"
    DESIRED_CHANNELS :: 4

    for texture in TextureHandle {
        name := get_texture_name(texture)
        
        path := fmt.tprint(RESOURCE_DIR, name, sep="")
        
        png_data, ok := os.read_entire_file(path)
        if !ok {
            log.errorf("error loading texture file %v", path)
            return false
        }
    
        stbi.set_flip_vertically_on_load(1)
        width, height, channels: i32
    
        data := stbi.load_from_memory(raw_data(png_data), auto_cast len(png_data), &width, &height, &channels, DESIRED_CHANNELS)
        if data == nil {
            log.errorf("error reading texture data with stbi: %v", path)
            return false
        }
    
        if channels != DESIRED_CHANNELS {
            log.errorf("error loading texture %v, expected %v channels got %v", path, DESIRED_CHANNELS, channels)
            return false
        }
    
        log.infof("loaded texture \"%v\" [%v x %v : %v bytes]", path, width, height, len(png_data))

        renderer.textures[texture] = {
            width = int(width),
            height = int(height),
            aspect_ratio = f32(width) / f32(height),
            data = data
        }
    }

    return true
}

build_texture_atlas :: proc(renderer: ^Renderer) -> bool {
    ATLAS_WIDTH     :: 256
    ATLAS_HEIGHT    :: 256
    BYTES_PER_PIXEL :: 4
    CHANNELS        :: 4
    ATLAS_BYTE_SIZE :: ATLAS_WIDTH * ATLAS_HEIGHT * BYTES_PER_PIXEL
    ATLAS_PATH      :: "build/atlas.png"

    atlas_data := make([^]byte, ATLAS_BYTE_SIZE)

    if false { // fill in default atlas data 
        i: int
        for i < ATLAS_BYTE_SIZE {
            atlas_data[i]       = 0     // r
            atlas_data[i + 1]   = 255   // g
            atlas_data[i + 2]   = 0     // b
            atlas_data[i + 3]   = 255   // a
    
            i += 4
        }
    }

    { // copy textures into atlas with rect pack
        RECT_COUNT :: len(TextureHandle)
        TEXTURE_PADDING_PIXELS :: 1

        rp_context: stbrp.Context
        nodes:      [ATLAS_WIDTH]stbrp.Node
        rects:      [RECT_COUNT]stbrp.Rect

        stbrp.init_target(&rp_context, ATLAS_HEIGHT, ATLAS_HEIGHT, &nodes[0], ATLAS_WIDTH)

        for texture, i in TextureHandle {
            info := &renderer.textures[texture]

            rects[i] = {
                id = c.int(texture),
                w = stbrp.Coord(info.width) + (TEXTURE_PADDING_PIXELS * 2),
                h = stbrp.Coord(info.height) + (TEXTURE_PADDING_PIXELS * 2),
            }
        }

        status := stbrp.pack_rects(&rp_context, &rects[0], RECT_COUNT)
        if status == 0 {
            log.error("error packing textures into atlas")
            return false
        }

        for i in 0..< len(rects) {
            rect := &rects[i] 
            texture_info := &renderer.textures[TextureHandle(rect.id)]

            // actual points that represent the texture,
            // the values stored in the rect include padding
            real_x := rect.x + TEXTURE_PADDING_PIXELS
            real_y := rect.y + TEXTURE_PADDING_PIXELS
            real_w := rect.w - (TEXTURE_PADDING_PIXELS * 2)
            real_h := rect.h - (TEXTURE_PADDING_PIXELS * 2)

            bottom_y_uv := f32(real_y)          / f32(ATLAS_HEIGHT)
            top_y_uv    := f32(real_y + real_h) / f32(ATLAS_HEIGHT)
            left_x_uv   := f32(real_x)          / f32(ATLAS_HEIGHT)
            right_x_uv  := f32(real_x + real_w) / f32(ATLAS_HEIGHT)

            texture_info.uv = {
                {left_x_uv, top_y_uv},      // top left
                {right_x_uv, top_y_uv},     // top right
                {right_x_uv, bottom_y_uv},  // bottom right
                {left_x_uv, bottom_y_uv},   // bottom left
            }

            for row in 0..< real_h {
                source_row := mem.ptr_offset(texture_info.data, row * real_w * BYTES_PER_PIXEL)
                dest_row   := mem.ptr_offset(atlas_data, ((real_y + row) * ATLAS_WIDTH + real_x) * BYTES_PER_PIXEL) // flipped textures in atlas

                mem.copy(dest_row, source_row, int(real_w) * BYTES_PER_PIXEL)
            }
        }
    } 

    when WRITE_DEBUG_IMAGES {
        stbi.flip_vertically_on_write(true)

        status := stbi.write_png(ATLAS_PATH, ATLAS_WIDTH, ATLAS_HEIGHT, CHANNELS, atlas_data, ATLAS_WIDTH * BYTES_PER_PIXEL)
        if status == 0 {
            log.error("error writing atlas png")
            return false
        }

        log.infof("wrote texture atlas to \"%v\" [%v x %v]", ATLAS_PATH, ATLAS_WIDTH, ATLAS_HEIGHT)
    }

    renderer.texture_atlas = Atlas {
        width = ATLAS_WIDTH,
        height = ATLAS_HEIGHT,
        data = atlas_data
    }

    log.infof("built texture atlas [%v x %v %v bytes uncompressed]", ATLAS_WIDTH, ATLAS_HEIGHT, ATLAS_BYTE_SIZE)

    return true
}

get_texture_name :: proc(texture: TextureHandle) -> string {
    switch texture {
        case .cuber:
            return "cuber.png"
       case .player:
            return "player.png"
        case .armour:
            return "armour.png"
        case .star:
            return "star.png"
        case .x_button:
            return "x_button.png"
        case .drone:
            return "drone.png"
        case .nest:
            return "nest.png"
        case .brick_wall:
            return "brick_wall.png"
        case .corner_wall:
            return "corner_wall.png"
        case .brick_wall_vertical:
            return "brick_wall_vertical.png"
        case .brick_wall_corner_left:
            return "brick_wall_corner_left.png"
        case .brick_wall_corner_right:
            return "brick_wall_corner_right.png"
        case .gem:
            return "gem.png"
        case .crate:
            return "crate.png"
        case .door:
            return "door.png"
        case .window:
            return "window.png"
        case .orc:
            return "orc.png"
        case .wizard:
            return "wizard.png"
        case .potion:
            return "potion.png"
        case .spludge:
            return "spludge.png"
        case .heart:
            return "heart.png"
        case .armour_bar:
            return "armour_bar.png"
        case .fireball:
            return "fireball.png"
        case .dash_icon:
            return "dash_icon.png"
        case .speed_lines:
            return "speed_lines.png"
    }

    unreachable()
}

load_font :: proc(renderer: ^Renderer, font: FontHandle, bitmap_width: int, bitmap_height: int, font_height: f32, filter: TextureFilter) -> bool {
    RESOURCE_DIR :: "resources/fonts/"
    CHAR_COUNT     :: 96

    font_info := Font {
        characters = make([]stbtt.bakedchar, CHAR_COUNT),
        bitmap = make([^]byte, bitmap_width * bitmap_height),
        bitmap_width = bitmap_width,
        bitmap_height = bitmap_height,
        filter = filter
    }

    path := fmt.tprint(RESOURCE_DIR, font_file_name(font), sep="")

    font_data, ok := os.read_entire_file(path)
    if !ok {
        log.errorf("error loading font file \"%v\"", path)
        return false
    }

    bake_result := stbtt.BakeFontBitmap(
        raw_data(font_data), 
        0, 
        font_height, 
        font_info.bitmap,
        i32(bitmap_width), 
        i32(bitmap_height), 
        32, 
        CHAR_COUNT, 
        slice.as_ptr(font_info.characters)
    )

    if bake_result <= 0 {
        log.errorf("error baking bitmap for font %v", path)
        return false
    }

    when WRITE_DEBUG_IMAGES {
        output_path :: "build/font.png"

        stbi.flip_vertically_on_write(false)

        write_result := stbi.write_png(output_path,i32(bitmap_width), i32(bitmap_height), 1, &font_info.bitmap[0], i32(bitmap_width))	
        if write_result == 0 {
            log.error("could not write font \"%v\" to output image \"%v\"", path, output_path)
            return false
        }

        log.infof("wrote font image to \"%v\" [%v x %v %v bytes uncompressed]", output_path, bitmap_width, bitmap_height, font_info.bitmap_width * font_info.bitmap_height)
    }

    log.infof("loaded font \"%v\"", path)

    renderer.font = font_info
   
    return true
}

font_file_name :: proc(font: FontHandle) -> string {
    switch font {
        case .alagard:
            return "alagard.ttf"
        case .baskerville:
            return "LibreBaskerville.ttf"
    }

    unreachable()
}

brightness :: proc(colour: v4, brightness: f32) -> v4 {
    new_colour := colour
    new_colour.rgb *= brightness
    return new_colour
}

alpha :: proc(colour: v4, alpha: f32) -> v4 {
    return {colour.r, colour.g, colour.b, alpha}
}

// mox colour gets applied to base based t from 0 -> 1
mix :: proc(base_colour: v4, mix_colour: v4, t: f32) -> v4 {
    return base_colour + (mix_colour - base_colour) * t
}

opengl_message_callback :: proc "c" (source: u32, type: u32, id: u32, severity: u32, length: i32, message: cstring, userParam: rawptr) {
    context = custom_context()

    message_string := strings.string_from_ptr(transmute([^]u8) message, int(length))
    
    log_string := fmt.tprintf(
        "opengl message: %v %v %v %v %v", 
        gl.GL_Enum(source), 
        gl.GL_Enum(type), 
        id, 
        gl.GL_Enum(severity), 
        message_string
    )

    #partial switch gl.GL_Enum(severity) {
        case gl.GL_Enum.DEBUG_SEVERITY_LOW: {
            log.debug(log_string)
        }
        case gl.GL_Enum.DEBUG_SEVERITY_MEDIUM: {
            log.error(log_string)
        }
        case gl.GL_Enum.DEBUG_SEVERITY_HIGH: {
            log.fatal(log_string)
        }
        case gl.GL_Enum.DEBUG_SEVERITY_NOTIFICATION: {
            log.info(log_string)
        }
    }
}

// -------------------------- @window -------------------------
create_window :: proc(width: f32, height: f32, title: cstring) -> (glfw.WindowHandle, bool) {
    ok := glfw.Init();
    if !ok {
        return nil, false
    }

    glfw.WindowHint(glfw.CONTEXT_VERSION_MAJOR, GL_MAJOR)
    glfw.WindowHint(glfw.CONTEXT_VERSION_MINOR, GL_MINOR)
    glfw.WindowHint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)
    // glfw.WindowHint(glfw.OPENGL_FORWARD_COMPAT, gl.TRUE) macos

    when OPENGL_MESSAGES {
        // enable opengl error callback
        glfw.WindowHint(glfw.OPENGL_DEBUG_CONTEXT, gl.TRUE)
    }

    window := glfw.CreateWindow(i32(width), i32(height), title, nil, nil)
    if window == nil {
        glfw.Terminate()
        return nil, false
    }

    glfw.MakeContextCurrent(window)

    glfw.SwapInterval(1 if V_SYNC else 0)
    glfw.SetErrorCallback(glfw_error_callback)
    glfw.SetKeyCallback(window, glfw_key_callback)
    glfw.SetMouseButtonCallback(window, glfw_mouse_button_callback)
    glfw.SetCursorPosCallback(window, glfw_mouse_move_callback)
    glfw.SetScrollCallback(window, glfw_scroll_callback)
    glfw.SetFramebufferSizeCallback(window, glfw_size_callback)

    return window, true
}

glfw_error_callback :: proc "c" (error: c.int, description: cstring) {
    context = custom_context()
    log.errorf("glfw window error: [%v] %v", error, description)
}

glfw_key_callback :: proc "c" (window: glfw.WindowHandle, key: c.int, scancode: c.int, action: c.int, mods: c.int) {
    context = custom_context()

    if key < 0 || key >= len(state.keys) {
        log.warn("input key ignored as it is not supported")
        return
    }

    // https://www.glfw.org/docs/latest/input_guide.html
    switch action {
        case glfw.RELEASE:  state.keys[key] = .up
        case glfw.PRESS:    state.keys[key] = .down
        case glfw.REPEAT: 
    } 
}

glfw_mouse_button_callback :: proc "c" (window: glfw.WindowHandle, button: c.int, action: c.int, mods: c.int) {
    // https://www.glfw.org/docs/latest/input_guide.html
    switch action {
        case glfw.RELEASE:  state.mouse[button] = .up
        case glfw.PRESS:    state.mouse[button] = .down
        case glfw.REPEAT: 
    }
}

glfw_mouse_move_callback :: proc "c" (window: glfw.WindowHandle, x: f64, y: f64) {
    // glfw thinks 0,0 is top left, this translates it to bottom left
    adjusted_y := -(f32(y) - state.height)

    state.mouse_position = {f32(x), adjusted_y}
}

glfw_scroll_callback :: proc "c" (window: glfw.WindowHandle, x: f64, y: f64) {
    state.mouse_scroll = f32(y)
}

glfw_size_callback :: proc "c" (window: glfw.WindowHandle, width: c.int, height: c.int) {
    gl.Viewport(0, 0, width, height)
    state.width = f32(width)
    state.height = f32(height)
}

// -------------------------- @random -------------------------
Timer :: struct{ 
    id: TimeId, 
    time_started: f64
}

TimeId :: distinct u64

wait_list: small_array.Small_Array(MAX_ENTITIES, Timer)

// Warning: assumes enum values are not overwritten
next_enum_value :: proc(t: $T) -> T 
    where intrinsics.type_is_enum(T)
{
    next_index := int(t) + 1

    if next_index >= len(T) {
        next_index = 0
    }

    return T(next_index)
}

// https://easings.net/#easeInSine
ease_in_sine :: proc(t: f32) -> f32 {
    assert(t >= 0 && t <= 1)
    return 1 - math.cos_f32((t * math.PI) * 0.5)
}

// https://easings.net/#easeOutSine
ease_out_sine :: proc(t: f32) -> f32 {
    assert(t >= 0 && t <= 1)
    return math.sin_f32((t * math.PI) * 0.5)
}

// https://easings.net/#easeInCubic
ease_in_cubic :: proc(t: f32) -> f32 {
    assert(t >= 0 && t <= 1)
    return t * t * t
}

wait :: proc(id: TimeId, seconds: f64) -> bool {
    for i in 0..<wait_list.len {
        waiter := &wait_list.data[i]

        if waiter.id == id {
            if waiter.time_started + seconds < state.time {
                small_array.unordered_remove(&wait_list, i)
                return true
            }

            return false
        }
    }

    small_array.append(&wait_list, Timer{id = id, time_started = state.time})

    return false    
}

cancel :: proc(id: TimeId) {
    for i in 0..<wait_list.len {
        waiter := &wait_list.data[i]

        if waiter.id == id {
            small_array.unordered_remove(&wait_list, i)
        }
    }
}

custom_context :: proc() -> runtime.Context {
    c := runtime.default_context()

    c.logger = {
        procedure = log_callback,
        lowest_level = .Debug when ODIN_DEBUG else .Warning
    }

    return c
}

log_callback :: proc(data: rawptr, level: runtime.Logger_Level, text: string, options: bit_set[runtime.Logger_Option], location := #caller_location) {
    when LOG_COLOURS {
        switch level {
        case .Debug:
        case .Info:
            fmt.print(ansi.CSI + ansi.FG_CYAN + ansi.SGR)
        case .Warning: 
            fmt.print(ansi.CSI + ansi.FG_YELLOW + ansi.SGR)
        case .Error:
            fmt.print(ansi.CSI + ansi.FG_BRIGHT_RED + ansi.SGR)
        case .Fatal:
            fmt.print(ansi.CSI + ansi.FG_RED + ansi.SGR)
        }
    }

    file := filepath.base(location.file_path)
    fmt.printfln("[%v] %v(%v:%v) %v", level, file, location.line, location.column, text) 

    when LOG_COLOURS {
        if level != .Debug {
            fmt.print(ansi.CSI + ansi.RESET + ansi.SGR)
        }
    }
}
