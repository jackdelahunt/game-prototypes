package src

// TODO:
// player gets gold from killing enemies, context.temp_allocator

import "core:fmt"
import "core:log"
import "base:runtime"
import "core:math/linalg"
import "core:time"
import "core:os"
import "core:strings"
import "core:math"
import "core:slice"
import "core:math/noise"
import "core:math/rand"
import "core:path/filepath"
import "core:encoding/ansi"
import sa "core:container/small_array"

import stbi "vendor:stb/image"
import stbtt "vendor:stb/truetype"

import sg "sokol/gfx"
import sapp "sokol/app"
import slog "sokol/log"
import sglue "sokol/glue"

import shaders "shaders"

DEFAULT_SCREEN_WIDTH	:: 1500
DEFAULT_SCREEN_HEIGHT	:: 1000

MAX_ENTITIES	:: 2048
MAX_QUADS	:: 2048

MAX_WAVES :: 20

PLAYER_SPEED :: 90
AI_SPEED :: 30

BASIC_ENEMY_DAMAGE :: 50
BASIC_ENEMY_HEALTH :: 100
NEXUS_HEALTH :: 1000

PLACE_RADIUS :: 50

TOWER_DAMAGE :: 50

SPAWNER_COOLDOWN :: 2

GOLD_FROM_DAMAGE :: 10
GOLD_FROM_BASIC_ENEMY :: 50

TICKS_PER_SECONDS :: 20
TICK_RATE :: 1.0 / TICKS_PER_SECONDS

LEVEL_SPAWNER_COUNT :: 8
LEVEL_WIDTH         :: 400
LEVEL_HEIGHT        :: 1200
NAV_MESH_WIDTH      :: LEVEL_WIDTH / 20
NAV_MESH_HEIGHT     :: LEVEL_HEIGHT / 20

// @settings
setting_start_gold      : uint = 10000
settings_spawning       : bool = true
settings_nav_mesh       : bool = false

// @state
State :: struct {
    // window
    screen_width: f32,
    screen_height: f32,

    // timing
    tick_timer: f64,

    // inputs
    key_inputs: #sparse [Key]InputState,
    key_inputs_this_frame: #sparse [Key]InputState,
    mouse_button_inputs: [3]InputState,
    mouse_screen_position: Vector2,
    mouse_world_position: Vector2, // only calculated at the start of every frame
    character_input: u32,

    // player global state
    gold: uint,
    selected_defence: DefenceType,

    // game state
    entities: []Entity,
    entity_count: uint,
    wave: uint,
    enemies_to_spawn: uint,
    enemies_to_kill: uint,
    wave_started: bool,

    // nav mesh state
    nav_mesh: NavMesh,
    nav_paths: sa.Small_Array(LEVEL_SPAWNER_COUNT, []NavMeshPosition),

    // command state
    in_command_mode: bool,
    command_input_buffer: [64]u8,
    command_input_length: uint,
    command_output_buffer: [1024]u8,
    command_output_length: uint,

    // renderer state
    camera_position: Vector2,
    zoom: f32,
    quads: []Quad,
    quad_count: uint,
    render_pipeline: sg.Pipeline,
    bindings: sg.Bindings,
    pass_action: sg.Pass_Action
}

state := State {}

// @context
game_context := runtime.default_context()

// @navmesh
NavMesh :: struct {
    nodes: [NAV_MESH_HEIGHT][NAV_MESH_WIDTH]NavNode,
}

NavNode :: struct {
    world_position: Vector2,
    nav_mesh_position: Vector2i,
    blocked: bool,
}

NavMeshPosition :: Vector2i

create_nav_mesh :: proc() -> NavMesh {
    MIN_X : f32 : -(LEVEL_WIDTH / 2) * 0.9
    MAX_X : f32 : (LEVEL_WIDTH / 2) * 0.9
    STEP_X : f32 : (MAX_X - MIN_X) / (NAV_MESH_WIDTH - 1)

    MIN_Y : f32 : -(LEVEL_HEIGHT / 2) * 0.95
    MAX_Y : f32 : (LEVEL_HEIGHT / 2) * 0.95
    STEP_Y : f32 : (MAX_Y - MIN_Y) / (NAV_MESH_HEIGHT - 1)

    DOWN :: NavMeshPosition{0, -1}
    LEFT :: NavMeshPosition{-1, 0}

    nav_mesh: NavMesh

    for y in 0..<NAV_MESH_HEIGHT {
        for x in 0..<NAV_MESH_WIDTH {
            node := &nav_mesh.nodes[y][x]

            node.blocked = false

            // set world position
            node.world_position = {
                MIN_X + (f32(x) * STEP_X),
                MIN_Y + (f32(y) * STEP_Y)
            }

            // set nav mesh position
            node.nav_mesh_position = {x, y}
        }
    }

    return nav_mesh
}

bake_navmesh :: proc(nav_mesh: ^NavMesh) {
    for &entity, i in state.entities[0:state.entity_count] {
        if !(.BLOCKS_NAV_MESH in entity.flags) {
            continue
        }

        left_bound := entity.position.x - (entity.size.x * 0.5)
        right_bound := entity.position.x + (entity.size.x * 0.5)

        up_bound := entity.position.y + (entity.size.y * 0.5)
        down_bound := entity.position.y - (entity.size.y * 0.5)

        for y in 0..<NAV_MESH_HEIGHT {
            for x in 0..<NAV_MESH_WIDTH {
                node := &nav_mesh.nodes[y][x]

                if node.world_position.x >= left_bound && node.world_position.x <= right_bound && 
                    node.world_position.y >= down_bound && node.world_position.y <= up_bound {
                        node.blocked = true
                }
            }
        }
    }
}


closest_node :: proc(nav_mesh: ^NavMesh, world_position: Vector2) -> NavNode {
    best_distance : f32 = math.F32_MAX
    best_node: NavNode

    for y in 0..<NAV_MESH_HEIGHT {
        for x in 0..<NAV_MESH_WIDTH {
            node := nav_mesh.nodes[y][x]
            distance := length(node.world_position - world_position)

            if distance < best_distance {
                best_node = node
                best_distance = distance
            }
        }
    }

    return best_node
}

get_node :: proc(nav_mesh: ^NavMesh, position: NavMeshPosition) -> NavNode {
    return nav_mesh.nodes[position.y][position.x]
}

get_node_ptr :: proc(nav_mesh: ^NavMesh, position: NavMeshPosition) -> ^NavNode {
    return &nav_mesh.nodes[position.y][position.x]
}

get_node_connections :: proc(nav_mesh: ^NavMesh, node_position: NavMeshPosition) -> [4]^NavNode {
    UP      :: NavMeshPosition{0, 1}
    DOWN    :: NavMeshPosition{0, -1}
    LEFT    :: NavMeshPosition{-1, 0}
    RIGHT   :: NavMeshPosition{1, 0}

    neighbour_positions := [4]NavMeshPosition {
        node_position + UP,
        node_position + DOWN,
        node_position + LEFT,
        node_position + RIGHT,
    }

    nodes: [4]^NavNode

    for p, i in neighbour_positions {
        if p.x >= 0 && p.x < NAV_MESH_WIDTH {
            if p.y >= 0 && p.y < NAV_MESH_HEIGHT {
                nodes[i] = &nav_mesh.nodes[p.y][p.x]
                continue
            }
        }

        nodes[i] = nil
    }

    return nodes
}

a_star :: proc(nav_mesh: ^NavMesh, start: NavMeshPosition, end: NavMeshPosition) -> ([]NavMeshPosition, bool) {
    open_list := make([dynamic]^NavNode, context.temp_allocator)
    g_scores := make(map[^NavNode]int, context.temp_allocator)
    f_scores := make(map[^NavNode]int, context.temp_allocator)
    came_from := make(map[^NavNode]^NavNode, context.temp_allocator)

    start_node := get_node_ptr(nav_mesh, start)
    append(&open_list, start_node)

    g_scores[start_node] = 0 // g score is distance to start so start is 0
    f_scores[start_node] = h_score(start_node, end)

    for len(open_list) > 0 {
        lowest_score := 100_000_000
        current_index := -1
        current: ^NavNode

        // find lowest cost node in the open list
        for node, i in open_list {
            score := f_scores[node] 
            if score < lowest_score {
                lowest_score = score
                current = node
                current_index = i
            }
        }

        if equal(current.nav_mesh_position, end) {
            return build_path(came_from, current), true // FOUND
        }

        ordered_remove(&open_list, current_index) 

        for neighbour in get_node_connections(nav_mesh, current.nav_mesh_position) {
            if neighbour == nil || neighbour.blocked {
                continue
            }

            maybe_new_g_score := g_scores[current] + 1
            score, ok := g_scores[neighbour]
            if !ok || maybe_new_g_score < score {
                came_from[neighbour] = current
                g_scores[neighbour] = score
                f_scores[neighbour] = score + h_score(neighbour, end)

                for node in open_list {
                    if node == neighbour {
                        break
                    }  
                }

                append(&open_list, neighbour)
            }
        }
    }

    return nil, false
}

h_score :: proc(node: ^NavNode, end: NavMeshPosition) -> int {
    return abs(node.nav_mesh_position.x - end.x) + abs(node.nav_mesh_position.y - end.y)
}

build_path :: proc(from_map: map[^NavNode]^NavNode, end: ^NavNode) -> []NavMeshPosition {
    positions := make([dynamic]NavMeshPosition)

    current := end

    for current != nil {
        append(&positions, current.nav_mesh_position)

        next, ok := from_map[current]
        if !ok {
            break
        }

        current = next
    }

    s := positions[0:]
    slice.reverse(s)
    return s
}

nav_node_equal :: proc(a: NavNode, b: NavNode) -> bool {
    return equal(a.nav_mesh_position, b.nav_mesh_position)
}

nav_mesh_position_equal :: proc(a: NavMeshPosition, b: NavMeshPosition) -> bool {
    return a.x == b.x && a.y == b.y
}

// @input
InputState :: enum {
    UP,
    DOWN,
    PRESSING,
    RELEASED
}

Key :: sapp.Keycode
MouseButton :: sapp.Mousebutton

///////////////////////////////// @colour
Colour :: distinct Vector4

WHITE	    :: Colour{1, 1, 1, 1}
BLACK	    :: Colour{0, 0, 0, 1}
RED	    :: Colour{0.9, 0.05, 0.05, 1}
DARK_RED    :: Colour{0.4, 0.05, 0.05, 1}
GREEN	    :: Colour{0.05, 0.9, 0.05, 1}
BLUE	    :: Colour{0.05, 0.05, 0.9, 1}
GRAY	    :: Colour{0.4, 0.4, 0.4, 1}
SKY_BLUE    :: Colour{0.07, 0.64, 0.72, 1}
YELLOW	    :: Colour{1, 0.9, 0.05, 1}
PINK	    :: Colour{0.8, 0.05, 0.6, 1}

with_alpha :: proc(colour: Colour, alpha: f32) -> Colour {
    return {colour.r, colour.g, colour.b, alpha} 
}

///////////////////////////////// @entity
Entity :: struct {
    // meta
    flags: bit_set[EntityFlag],
    dynamic_flags: bit_set[DynamicEntityFlag],

    // core
    position: Vector2,
    velocity: Vector2,
    size: Vector2,
    rotation: f32,
    colour: Colour,
    inactive: bool,
    
    // flag: health
    health: f32,
    max_health: f32,

    // flag: defence
    defence_type: DefenceType,
    defence_cooldown: f32,

    // flag: spawner
    spawner_cooldown: f32,

    // flag: ai
    nav_path: int,         // spawners also set this to pass to ai
    nav_path_index: int,
    entity_target: EntityFlag,

    // flag: weapon
    weapon: WeaponType,
    magazine_ammo: uint,
    firing_cooldown: f32,
    reload_cooldown: f32,
}

EntityFlag :: enum {
    NONE = 0,
    PLAYER,
    AI,
    NEXUS,
    DEFENCE,
    SPAWNER,
    PROJECTILE,
    HAS_HEALTH,
    HAS_WEAPON,
    SOLID_HITBOX,
    STATIC_HITBOX,
    TRIGGER_HITBOX,
    BLOCKS_NAV_MESH,
}

DynamicEntityFlag :: enum {
    NONE = 0,
    DELETE,
    FIRING,
    RELOADING
}

create_entity :: proc(entity: Entity) -> ^Entity {
    ptr := &state.entities[state.entity_count]
    state.entity_count += 1

    ptr^ = entity

    { // basic checking fo values with their flags
        if .HAS_HEALTH in entity.flags {
            assert(entity.health > 0 && entity.max_health > 0)
            assert(entity.health <= entity.max_health)
        }
    
        if .DEFENCE in entity.flags {
            assert(entity.defence_type != .NONE)
        }
    
        if .HAS_WEAPON in entity.flags {
            assert(entity.weapon != .NONE)
        }
    
        if .SOLID_HITBOX in entity.flags {
            assert(!(.STATIC_HITBOX in entity.flags))
        }
    
        if .STATIC_HITBOX in entity.flags {
            assert(!(.SOLID_HITBOX in entity.flags))
        }

        if .BLOCKS_NAV_MESH in entity.flags {
            assert(.STATIC_HITBOX in entity.flags)
        }
    }

    return ptr
}

get_entity_with_flag :: proc(flag: EntityFlag) -> ^Entity {
    for &entity in state.entities[0:state.entity_count] {
        if flag in entity.flags {
            return &entity
        }
    }

    return nil
}

entity_take_damage :: proc(entity: ^Entity, damage: f32) {
    assert(.HAS_HEALTH in entity.flags)

    entity.health -= damage
    if entity.health < 0 {
        entity.health = 0
    }
}

///////////////////////////////// @weapons
WeaponType :: enum {
    NONE,
    SHOTGUN
}

magazine_size :: proc(weapon: WeaponType) -> uint {
    switch weapon {
    case .NONE:
    case .SHOTGUN:
        return 5
    }

    panic("weapon type is none")
}

firing_cooldown :: proc(weapon: WeaponType) -> f32 {
    switch weapon {
    case .NONE:
    case .SHOTGUN:
        return 1
    }

    panic("weapon type is none")
}

reload_cooldown :: proc(weapon: WeaponType) -> f32 {
    switch weapon {
    case .NONE:
    case .SHOTGUN:
        return 3
    }

    panic("weapon type is none")
}

create_projectiles_for_weapon :: proc(position: Vector2, direction: Vector2, weapon: WeaponType) {
    SPEED :: 150

    directions := [3]Vector2 {
    direction,
    rotate_normalised_vector(direction, 15),
    rotate_normalised_vector(direction, -15),
    }

    for d in directions {
        create_entity(Entity {
        flags = {.PROJECTILE, .TRIGGER_HITBOX},
        position = position,
        size = {3, 3}, 
        colour = BLACK,
        velocity = d * SPEED
        })
    }
}

///////////////////////////////// @defences
DefenceType :: enum {
    NONE,
    SHORT_TOWER,
    HIGH_TOWER,
}

defence_display_name :: proc(defence: DefenceType)  -> string {
    switch defence {
    case .HIGH_TOWER:
        return "High Tower"
    case .SHORT_TOWER:
        return "Short Tower"
    case .NONE:
        panic("defence type is none")
    }

    return "" // unreachable
}

defence_range :: proc(defence: DefenceType) -> f32 {
    switch defence {
    case .HIGH_TOWER:
        return 35
    case .SHORT_TOWER:
        return 25
    case .NONE:
        panic("defence type is none")
    }

    return 0 // unreachable
}

defence_cost :: proc(defence: DefenceType) -> uint {
    switch defence {
    case .HIGH_TOWER:
        return 600
    case .SHORT_TOWER:
        return 500
    case .NONE:
        panic("defence type is none")
    }

    return 0 // unreachable
}

defence_colour :: proc(defence: DefenceType) -> Colour {
    switch defence {
    case .HIGH_TOWER:
        return GREEN
    case .SHORT_TOWER:
        return YELLOW
    case .NONE:
        panic("defence type is none")
    }

    return WHITE // unreachable
}

defence_cooldown :: proc(defence: DefenceType) -> f32 {
    switch defence {
    case .HIGH_TOWER:
        return 3
    case .SHORT_TOWER:
        return 1.5
    case .NONE:
        panic("defence type is none")
    }

    return 0 // unreachable
}

///////////////////////////////// @textures
Texture :: struct {
    width: i32,
    height: i32,
    data: [^]byte
}

face_texture: Texture

///////////////////////////////// @fonts
// i dont know why any of these were chosen but i just want
// to get fonts working and I dont want to look into it
font_bitmap_w :: 256 
font_bitmap_h :: 256
char_count :: 96

Font :: struct {
    characters: [char_count]stbtt.bakedchar,
    bitmap: [font_bitmap_w * font_bitmap_h]byte
}

alagard: Font

///////////////////////////////// @main
main :: proc() {
    game_context.logger.lowest_level = .Debug if ODIN_DEBUG else .Warning
    game_context.logger.procedure = log_callback
    game_context.allocator.procedure = allocator_callback

    context = game_context

    { // load resources
        loading_ok := true
    
        loading_ok = load_textures()
        if !loading_ok {
            log.fatal("error loading textures.. exiting")
            return
        }
        
        loading_ok = load_fonts()
        if !loading_ok {
            log.fatal("error loading fonts.. exiting")
            return
        }
    }

    init_state()
    setup_game()

    sapp.run({
        init_cb = renderer_init,
        frame_cb = renderer_frame,
        cleanup_cb = renderer_cleanup,
        event_cb = window_event_callback,
        width = DEFAULT_SCREEN_WIDTH,
        height = DEFAULT_SCREEN_HEIGHT,
        window_title = "sokol window",
        icon = { sokol_default = true },
        logger = { func = slog.func },
    })
}



///////////////////////////////// @init
init_state :: proc() {
    state = State {
        screen_width = state.screen_width,
        screen_height = state.screen_height,
        entities = make([]Entity, MAX_ENTITIES),
        quads = make([]Quad, MAX_QUADS)
    }
}

setup_game :: proc() {
    {
        state.gold = setting_start_gold
        state.zoom = 2.5
        state.nav_mesh = create_nav_mesh()
        state.wave =  1
        state.wave_started = false
        state.in_command_mode = false
        state.enemies_to_spawn = enemies_for_wave(1)
        state.enemies_to_kill = enemies_for_wave(1)
    }

    { // generate level
        // floor
        create_entity(Entity {
            position = {0, 0}, 
            size = {LEVEL_WIDTH, LEVEL_HEIGHT}, 
            colour = WHITE
        })

        // player
        create_entity(Entity {
            flags = {.PLAYER, .HAS_WEAPON, .SOLID_HITBOX},
            position = {0, -LEVEL_HEIGHT / 3}, 
            size = {10, 10}, 
            colour = BLUE,
            weapon = .SHOTGUN,
            magazine_ammo = magazine_size(.SHOTGUN)
        }) 
        
        // nexus
        nexus := create_entity(Entity {
            flags = {.NEXUS, .HAS_HEALTH, .STATIC_HITBOX},
            position = {0, -(LEVEL_HEIGHT / 2)}, 
            size = {100, 10}, 
            colour = SKY_BLUE,
            health = NEXUS_HEALTH,
            max_health = NEXUS_HEALTH
        })

        nexus_node := closest_node(&state.nav_mesh, nexus.position)

        // generate walls
        for y in 2..<NAV_MESH_HEIGHT {
            for x in 0..<NAV_MESH_WIDTH {
                MAX_SIZE : f32 : 20
                MIN_SIZE : f32 : 5
                NOISE_SCALE :: 0.15
    
                f := noise.noise_2d_improve_x(69, {auto_cast x, auto_cast y} * NOISE_SCALE)
                f = (f + 1) * 0.5 // make f from 0 -1
                if f < 0.7 {
                    continue
                }

                position := get_node_ptr(&state.nav_mesh, {x, y}).world_position
                create_entity(Entity{
                    flags = {.STATIC_HITBOX, .BLOCKS_NAV_MESH},
                    position = position, 
                    size = {20, 20}, 
                    colour = {WHITE.r - f, WHITE.g - f, WHITE.b - f, 1},
                })
            }
        }

        // done before spawners are created because they need the nav mesh
        bake_navmesh(&state.nav_mesh)

        // spawners
        for i in 0..<LEVEL_SPAWNER_COUNT {
            MIN_Y :: -(LEVEL_HEIGHT / 2) * 0.5
            MAX_Y :: (LEVEL_HEIGHT / 2) * 0.9
            RANGE_Y :: MAX_Y - MIN_Y

            MIN_X :: -(LEVEL_WIDTH / 2) * 0.9
            MAX_X :: (LEVEL_WIDTH / 2) * 0.9
            RANGE_X :: MAX_X - MIN_X

            spawner_position: Vector2

            // keeps trying to find a node that is not blocked in the 
            // nav mesh to spawn on top of
            for true {
                y_random := rand.float32()
                x_random := rand.float32()
                
                testing_position := Vector2{MIN_X + (RANGE_X * x_random), MIN_Y + (RANGE_Y * y_random)}
                node := closest_node(&state.nav_mesh, testing_position)
                if !node.blocked {
                    spawner_position = node.world_position
                    break
                }
            }

            spawner := create_entity(Entity {
                flags = {.SPAWNER},
                position = spawner_position, 
                // inactive = true,
                size = {10, 10}, 
                colour = PINK,
                nav_path = i
            })

            spawner_node := closest_node(&state.nav_mesh, spawner.position)
            path, ok := a_star(&state.nav_mesh, spawner_node.nav_mesh_position, nexus_node.nav_mesh_position)
            if !ok {
                log.warn("there was a problem generating nav mesh path for a spawner")
                continue
            }

            sa.append(&state.nav_paths, path)
        }
    }
}

reset_game :: proc() {
    state.entity_count = 0
    state.quad_count = 0
    
    for path in sa.slice(&state.nav_paths) {
        delete(path)
    }

    state.nav_paths.len = 0
}


///////////////////////////////// @frame
frame :: proc() {
    free_all(context.temp_allocator)

    delta_time := auto_cast sapp.frame_duration()
    state.tick_timer += delta_time

    // only does once per frame as it it expensive
    state.mouse_world_position = screen_position_to_world_position(state.mouse_screen_position)
    log.info(state.mouse_screen_position, screen_position_to_ndc(state.mouse_screen_position))

    if state.tick_timer >= TICK_RATE {
        apply_inputs()
        update_game()
        update()
        state.tick_timer = 0
    }

    physics(auto_cast delta_time)
    draw(auto_cast delta_time) 
}

///////////////////////////////// @apply_inputs
apply_inputs :: proc() {
    if !state.in_command_mode {
        state.character_input = 0
    }

    for index in 0..<len(state.key_inputs) {	
        // input state from key_inputs_this_frame can only 
        // be DOWN or UP, need to apply this to the key
        // inputs while allowing for pressing and released states
    
        state_last_tick := &state.key_inputs[auto_cast index]
        state_last_frame := state.key_inputs_this_frame[auto_cast index]
    
        switch state_last_tick^ {
        case .UP:
            if state_last_frame == .DOWN {
                state_last_tick^ = .DOWN
            }
        case .DOWN:
            if state_last_frame == .DOWN {
                state_last_tick^ = .PRESSING
            }
            else if state_last_frame == .UP {
                state_last_tick^ = .RELEASED
            }
        case .PRESSING:
            if state_last_frame == .UP {
                state_last_tick^ = .RELEASED
            }
        case .RELEASED:
            if state_last_frame == .UP {
                state_last_tick^ = .UP
            }
            else if state_last_frame == .DOWN {
                state_last_tick^ = .DOWN
            }
        }
    }
}

///////////////////////////////// @update
update_game :: proc() {
    if state.enemies_to_kill == 0 {
        state.wave += 1
        state.wave_started = false

        state.enemies_to_spawn = enemies_for_wave(state.wave)
        state.enemies_to_kill = enemies_for_wave(state.wave)
    }
}

update :: proc() {
    if state.key_inputs[.ESCAPE] == .DOWN {
        sapp.quit()
    }

    if state.key_inputs[.K] == .DOWN {
        state.wave_started = true
    }

    // go through each number key and select defence
    for type, index in DefenceType {
        // first defence is none and we want to skip 0 number input
        if index == 0 {
            continue
        }
    
        current_key := Key(int(Key._0) + index)
    
        if state.key_inputs[current_key] == .DOWN {
            state.selected_defence = type
        }
    }

    // update pass
    for &entity in state.entities[0:state.entity_count] {
        if entity.inactive {
            continue
        }

        // count down each cooldown in the entity
        // some cooldown timers are not set here, reload cooldown is set in weapon 
        // because we want to do stuff when it reaches 0
        entity.defence_cooldown = clamp(entity.defence_cooldown - TICK_RATE, 0, math.F32_MAX)
        entity.spawner_cooldown = clamp(entity.spawner_cooldown - TICK_RATE, 0, math.F32_MAX)
        entity.firing_cooldown = clamp(entity.firing_cooldown - TICK_RATE, 0, math.F32_MAX)
    
        player: {
            if !(.PLAYER in entity.flags) {
                break player
            }
    
            if state.in_command_mode {
                break player
            }
    
            { // player movement
                input_vector: Vector2
            
                if state.key_inputs[.A] == .PRESSING {
                    input_vector.x -= 1
                }
             
                if state.key_inputs[.D] == .PRESSING {
                    input_vector.x += 1
                }
             
                if state.key_inputs[.S] == .PRESSING {
                    input_vector.y -= 1
                }
             
                if state.key_inputs[.W] == .PRESSING {
                    input_vector.y += 1
                }
            
                entity.velocity = normalize(input_vector) * PLAYER_SPEED 
            }
    
            place_defence: {
                if state.mouse_button_inputs[MouseButton.RIGHT] != .DOWN {
                    break place_defence
                }
        
                if state.selected_defence == .NONE {
                    break place_defence
                }
        
                if defence_cost(state.selected_defence) > state.gold {
                    break place_defence
                }
        
                place_position := state.mouse_world_position
                if length(place_position - entity.position) > PLACE_RADIUS {
                    break place_defence
                }
        
                create_entity(Entity{
                    flags = {.DEFENCE},
                    position = place_position,
                    size = {10, 10},
                    colour = defence_colour(state.selected_defence),
                    defence_type = state.selected_defence
                })
                
                state.gold -= defence_cost(state.selected_defence)
                state.selected_defence = .NONE
            }
    
            { // weapon interaction
                assert(entity.weapon != .NONE)
        
                    if state.key_inputs[.R] == .DOWN {
                        if entity.reload_cooldown == 0 && entity.magazine_ammo != magazine_size(entity.weapon) {
                            entity.dynamic_flags += {.RELOADING}
                        }
                    }
        
                if state.mouse_button_inputs[MouseButton.LEFT] == .DOWN {
                    if entity.firing_cooldown == 0 && entity.reload_cooldown == 0 && entity.magazine_ammo > 0 {
                        entity.dynamic_flags += {.FIRING}
                    }
                }
            }
        }
    
        ai: {
            if !(.AI in entity.flags) {
                break ai
            }

            if entity.nav_path_index == len(sa.get(state.nav_paths, entity.nav_path)) {
                entity.entity_target = .NEXUS
            }

            target_position: Vector2

            if entity.entity_target == .NONE {
                node_position := state.nav_paths.data[entity.nav_path][entity.nav_path_index]
                node := get_node(&state.nav_mesh, node_position)
                target_position = node.world_position 
            } else {
                target_entity := get_entity_with_flag(entity.entity_target)
                if target_entity == nil {
                    break ai
                }

                target_position = target_entity.position
            }

            delta := target_position - entity.position
            distance := length(delta)
            direction := normalize(delta)
            entity.velocity = direction * AI_SPEED

            if distance < 10 && entity.entity_target == .NONE {
                entity.nav_path_index += 1
            }
    
            // if distance < 10 {
                // entity.dynamic_flags += {.DELETE}
                // entity_take_damage(nexus, BASIC_ENEMY_DAMAGE)
            // }
        }
    
        health: {
            if !(.HAS_HEALTH in entity.flags) {
                break health
            }
    
            if entity.health <= 0 {
                entity.dynamic_flags += {.DELETE}
            }
        }
    
        defence: {
            if !(.DEFENCE in entity.flags) {
                break defence
            }
    
            if entity.defence_cooldown > 0 {
                break defence
            } 
    
            // TODO: once the cooldown is 0 but there are no enemies to
            // shoot that means we are doing this collision check every tick
            collider := new_circle_collider(entity.position, defence_range(entity.defence_type))
            other, ok := next(&collider)
            for ok {
                if .AI in other.flags && .HAS_HEALTH in other.flags {
                    // give damge and set the cooldown, this means if there
                    // is no enemies in range then the cooldown is not reset
                    entity_take_damage(other, TOWER_DAMAGE)
                    entity.defence_cooldown = defence_cooldown(entity.defence_type)
        
                    if other.health > 0 {
                        state.gold += GOLD_FROM_DAMAGE
                    }
                    else {
                        state.gold += GOLD_FROM_BASIC_ENEMY
                    }
                } 
                
                other, ok = next(&collider)
            }
        }
    
        spawner: {
            if !(.SPAWNER in entity.flags) {
                break spawner
            }
    
            if !settings_spawning {
                break spawner
            }

            if state.wave_started == false {
                break spawner
            }
    
            if entity.spawner_cooldown > 0 {
                break spawner
            }
    
            if state.enemies_to_spawn == 0 {
                break spawner
            }
    
            state.enemies_to_spawn -= 1
            entity.spawner_cooldown = SPAWNER_COOLDOWN
    
            create_entity(Entity {
                flags = {.AI, .HAS_HEALTH, .SOLID_HITBOX},
                position = entity.position, 
                size = {10, 10}, 
                colour = DARK_RED,
                health = BASIC_ENEMY_HEALTH,
                max_health = BASIC_ENEMY_HEALTH,
                nav_path = entity.nav_path,
                nav_path_index = 0,
            })
        }
    
        weapon: {
            if !(.HAS_WEAPON in entity.flags) {
                break weapon
            }
    
            if entity.reload_cooldown > 0 {
                entity.reload_cooldown = clamp(entity.reload_cooldown - TICK_RATE, 0, math.F32_MAX)
        
                if entity.reload_cooldown == 0 {
                    entity.magazine_ammo = magazine_size(entity.weapon)
                }
            }
    
            // auto reload when no ammo left
            if entity.magazine_ammo == 0 && entity.reload_cooldown == 0 {
                entity.dynamic_flags += {.RELOADING}
            }
    
            if .FIRING in entity.dynamic_flags {
                entity.dynamic_flags -= {.FIRING}
                entity.firing_cooldown = firing_cooldown(entity.weapon)
        
                assert(entity.magazine_ammo > 0, "Firing weapon without checking magazine amount")
                entity.magazine_ammo -= 1
        
                direction := normalize(state.mouse_world_position - entity.position)
                create_projectiles_for_weapon(entity.position, direction, entity.weapon)
            }
    
            if .RELOADING in entity.dynamic_flags {
                entity.dynamic_flags -= {.RELOADING}
                entity.reload_cooldown = reload_cooldown(entity.weapon)
            }
        }
    }

    // delete pass
    i : uint = 0
    for i < state.entity_count {
        entity := &state.entities[i]
    
        if .DELETE in entity.dynamic_flags {
            if .AI in entity.flags {
                assert(state.enemies_to_kill > 0)
                state.enemies_to_kill -= 1
            }

            // last value just decrement count
            if i == state.entity_count - 1 {
                state.entity_count -= 1
                break
            }
    
            // swap remove with last entity
            state.entities[i] = state.entities[state.entity_count - 1]
            state.entity_count -= 1
        } else {
            // if we did remove then we want to re-check the current
            // entity we swapped with so dont go to next index
            i += 1
        }
    }

    command_mode: {
        if state.key_inputs[.SLASH] == .DOWN {
            state.in_command_mode = !state.in_command_mode
        }

        if !state.in_command_mode {
            break command_mode
        }
    
        // character input to the command buffer
        if state.character_input != 0 {
            if state.command_input_length < len(state.command_input_buffer) - 1 {
                state.command_input_buffer[state.command_input_length] = cast(u8)state.character_input
                state.command_input_length += 1
                state.character_input = 0
            }
        }
    
        if state.key_inputs[.BACKSPACE] == .DOWN {
            if state.command_input_length > 0 {
                state.command_input_length -= 1
            }
        }
    
        if state.key_inputs[.ENTER] == .DOWN {
            command := state.command_input_buffer[0:state.command_input_length]
            state.command_input_length = 0
            run_command(command)
        }
    }
}

on_trigger_collision :: proc(trigger: ^Entity, other: ^Entity) {
    if !(.AI in other.flags) {
        return
    }

    assert(.HAS_HEALTH in other.flags)
    entity_take_damage(other, 20)
}

on_solid_collision :: proc(entity: ^Entity, other: ^Entity) {
    if .AI in entity.flags && .NEXUS in other.flags {
        entity.dynamic_flags += {.DELETE}
        entity_take_damage(other, 20)
    }
}

///////////////////////////////// @physics
physics :: proc(delta_time: f32) {
    player := get_entity_with_flag(.PLAYER)
    if player != nil {
        state.camera_position.y = player.position.y
    }

    for _i in 0..<state.entity_count {
        entity := &state.entities[_i]
    
        // used to check if a collision occurs after
        // a velocity is applied
        start_position := entity.position
    
        entity.position += entity.velocity * delta_time
    
        solid_hitbox: {
            // this means if we are ever doing a collision the 
            // soldin one is always the entity, and if one is 
            // static then it is the other entity
            if !(.SOLID_HITBOX in entity.flags) {
                break solid_hitbox
            }
    
            for _o in 0..<state.entity_count {
                other := &state.entities[_o]
        
                if entity == other {
                    continue
                }
        
                if !(.SOLID_HITBOX in other.flags) && !(.STATIC_HITBOX in other.flags) {
                    continue
                }
        
                distance := other.position - entity.position
                distance_abs := Vector2{abs(distance.x), abs(distance.y)}
                distance_for_collision := (entity.size + other.size) * Vector2{0.5, 0.5}
        
                // basic AABB collision detection, everything is a square
                if !(distance_for_collision[0] >= distance_abs[0] && distance_for_collision[1] >= distance_abs[1]) {
                    continue;
                }
        
                overlap_amount := distance_for_collision - distance_abs
                other_static := .STATIC_HITBOX in other.flags
        
                // if there is an overlap then measure on which axis has less 
                // overlap and equally move both entities by that amount away
                // from each other
                //
                // if other is static only move entity
                // by the full overlap instead of sharing it
                if overlap_amount[0] < overlap_amount[1] {
                    if other_static { 
                        x_push_amount := overlap_amount[0]
                        entity.position[0] -= math.sign(distance[0]) * x_push_amount
                    } 
                    else  {
                        x_push_amount := overlap_amount[0] * 0.5;
                        entity.position[0] -= math.sign(distance[0]) * x_push_amount
                        other.position[0] += math.sign(distance[0]) * x_push_amount;
                    }
                } 
                else {
                    if other_static { 
                        y_push_amount := overlap_amount[1]
                        entity.position[1] -= math.sign(distance[1]) * y_push_amount
                    } 
                    else  {
                        y_push_amount := overlap_amount[1] * 0.5;
                        entity.position[1] -= math.sign(distance[1]) * y_push_amount
                        other.position[1] += math.sign(distance[1]) * y_push_amount;
                    }
                }
    
                on_solid_collision(entity, other) 
            } 
        }
        
        trigger_hitbox: {
            if !(.TRIGGER_HITBOX in entity.flags) {
                break trigger_hitbox
            }
    
            for _o in 0..<state.entity_count {
                other := &state.entities[_o]
        
                if entity == other {
                    continue
                }
        
                if  !(.SOLID_HITBOX in other.flags) && 
                    !(.STATIC_HITBOX in other.flags) &&
                    !(.TRIGGER_HITBOX in other.flags)
                {
                    continue
                }
        
                // collision for each other entiity is checked twice for trigger
                // hitboxes, trigger collision events are only when a new collision
                // starts so if there was a collision last frame then dont do anything
        
                { // collision last frame
                    distance := other.position - start_position
                    distance_abs := Vector2{abs(distance.x), abs(distance.y)}
                    distance_for_collision := (entity.size + other.size) * Vector2{0.5, 0.5}
           
                    // if there was a collision then don't check for this frame
                    if (distance_for_collision[0] >= distance_abs[0] && distance_for_collision[1] >= distance_abs[1])
                    {
                        continue;
                    }
                }
        
                { // collision this frame
                    distance := other.position - entity.position
                    distance_abs := Vector2{abs(distance.x), abs(distance.y)}
                    distance_for_collision := (entity.size + other.size) * Vector2{0.5, 0.5}
            
                    // basic AABB collision detection, everything is a square
                    if !(distance_for_collision[0] >= distance_abs[0] && distance_for_collision[1] >= distance_abs[1])
                    {
                        continue;
                    }
                }
        
                on_trigger_collision(entity, other)
            }
        }
    }
}

CircleCollider :: struct {
    index: uint,
    position: Vector2,
    radius: f32
}

new_circle_collider :: proc(position: Vector2, radius: f32) -> CircleCollider {
    return CircleCollider {
        index = 0,
        position = position,
        radius = radius
    }
}

next :: proc(collider: ^CircleCollider) -> (^Entity, bool) {
    start := collider.index

    if start >= state.entity_count {
        return nil, false
    }

    for &entity in state.entities[start:state.entity_count] {
        collider.index += 1
    
        if length(entity.position - collider.position) < collider.radius {
            return &entity, true
        }
    }

    return nil, false
}

///////////////////////////////// @draw
draw :: proc(delta_time: f32) {
    for &entity in state.entities[0:state.entity_count] {
        // this can be changed based on entity state
        entity_colour := entity.colour
    
        player: {
            if !(.PLAYER in entity.flags) {
                break player
            }
    
            if state.selected_defence != .NONE {
                place_circle_colour := RED
                place_position := state.mouse_world_position
        
                // drawing placement preview
                if length(entity.position - place_position) <= PLACE_RADIUS {
                    place_circle_colour = GREEN
                    draw_rectangle(place_position, {10, 10}, defence_colour(state.selected_defence))
        
                    string_buffer: [10]u8
                        builder := strings.builder_from_bytes(string_buffer[0:])
        
                        text := fmt.sbprintf(&builder, "-%v", defence_cost(state.selected_defence))
                        draw_text(text, {place_position.x - 6, place_position.y + 8}, BLACK, 0.4)
                }
        
                draw_circle(entity.position, PLACE_RADIUS, with_alpha(place_circle_colour, 0.1))
            }
        }
    
        health: {
            if !(.HAS_HEALTH in entity.flags) {
                break health
            }
    
            if entity.health == entity.max_health {
                break health
            }
    
            health_ratio := entity.health / entity.max_health
            health_bar_width := max(entity.size.x * 0.75, 15)
            draw_rectangle(entity.position + {0, (entity.size.y * 0.5) + 5}, {health_bar_width, 4}, DARK_RED)
            draw_rectangle(entity.position + {0, (entity.size.y * 0.5) + 5}, {health_bar_width * health_ratio, 4}, RED)
        }
    
        defence: {
            if !(.DEFENCE in entity.flags) {
                break defence
            }
    
            draw_circle(entity.position, defence_range(entity.defence_type), with_alpha(entity.colour, 0.15))
    
            // cooldown bar
            if entity.defence_cooldown == 0 {
                break defence
            }
    
            max_cooldown := defence_cooldown(entity.defence_type)
    
            cooldown_ratio := 1 - (entity.defence_cooldown / max_cooldown)
            bar_width := max(entity.size.x * 0.9, 15)
            draw_rectangle(entity.position + {0, (entity.size.y * 0.5) + 5}, {bar_width, 3}, BLUE)
            draw_rectangle(entity.position + {0, (entity.size.y * 0.5) + 5}, {bar_width * cooldown_ratio, 3}, SKY_BLUE)
        }
    
        weapon: {
            if !(.HAS_WEAPON in entity.flags) {
                break weapon
            }
    
            if entity.firing_cooldown > 0 {
                entity_colour = YELLOW
            }
    
            if entity.reload_cooldown > 0 {
                entity_colour = RED
            }
        }

        spawner: {
            if !(.SPAWNER in entity.flags) {
                break spawner
            }

            if entity.inactive {
                entity_colour = GRAY
            }
        }
    
        draw_rectangle(entity.position, entity.size, entity_colour, entity.rotation)	
    }

    draw_nav_mesh: {
        if !settings_nav_mesh {
            break draw_nav_mesh
        }

        player := get_entity_with_flag(.PLAYER)
        if player == nil {
            break draw_nav_mesh
        }

        nexus := get_entity_with_flag(.NEXUS)
        if nexus == nil {
            break draw_nav_mesh
        }

        // drawing nodes
        for y in 0..< len(state.nav_mesh.nodes) {
            for x in 0..< len(state.nav_mesh.nodes[0]) {
                node := state.nav_mesh.nodes[y][x]
                if node.blocked {
                    continue
                }

                colour := BLACK
                for path in sa.slice(&state.nav_paths) {
                    if len(path) == 0 {
                        continue
                    }
                        
                    start_node_position := path[0]
                    end_node_position := path[len(path) - 1]

                    if equal(start_node_position, node.nav_mesh_position) {
                        colour = GREEN
                    }
                    else if equal(end_node_position, node.nav_mesh_position) {
                        colour = RED
                    } else {
                        for node_position in path {
                            if equal(node_position, node.nav_mesh_position) {
                                colour = SKY_BLUE
                            }
                        }
                    }
                }

                draw_circle(node.world_position, 3, with_alpha(colour, 1))
            }
        }

        // drawing lines between all nodes
        for path in sa.slice(&state.nav_paths) {
            if len(path) == 0 {
                continue
            }

            for position, i in path[0 : len(path) - 1] {
                source_node := get_node(&state.nav_mesh, position)
                destination_node := get_node(&state.nav_mesh, path[i + 1])
                
                draw_line(source_node.world_position, destination_node.world_position, 1, RED)
            }
        }
    } 

    in_screen_space = true

    { // top UI bar
        string_buffer: [256]u8
        builder := strings.builder_from_bytes(string_buffer[0:])

        ammo: uint
        player := get_entity_with_flag(.PLAYER)
        if player != nil {
            ammo = player.magazine_ammo
        }

        text := fmt.sbprintf(
            &builder, 
            "Wave: %v/%v   Ammo: %v   Enemies Left: %v   Gold: %v", 
            state.wave, MAX_WAVES, 
            ammo, 
            state.enemies_to_kill, 
            state.gold
        )

        colour := YELLOW
        if !state.wave_started {
            colour = WHITE
        }

        draw_rectangle({state.screen_width / 2, 25}, {state.screen_width, 50}, BLACK)
        draw_text(text, {25, 25}, colour, 3)
    }

    { // bottom defence layout
        card_width : f32 = state.screen_width * 0.15
        padding : f32 = card_width * 0.2
        card_y : f32 = state.screen_height - 25
        card_start_x : f32 = (card_width / 2) + 30
    
        for defence, i in DefenceType {
            if i == 0 {
                continue
            }
    
            card_colour := BLACK
            text_colour := WHITE
                
            if auto_cast state.selected_defence == i {
                text_colour = BLACK
                card_colour = YELLOW
            }
    
            card_x := card_start_x + (card_width * f32(i - 1)) + (padding * f32(i - 1))
            draw_rectangle({card_x, card_y}, {card_width, 40}, card_colour)
    
            string_buffer: [40]u8
    
            { // card text
                builder := strings.builder_from_bytes(string_buffer[0:])
                text := fmt.sbprintf(&builder, "%v      %v", defence_display_name(defence), defence_cost(defence))
        
                draw_text(text, {card_x - (card_width * 0.45), card_y}, text_colour, 1)
            }
    
            { // card number
                builder := strings.builder_from_bytes(string_buffer[0:])
                text := fmt.sbprintf(&builder, "press %v", i)
        
                draw_text(text, {card_x - (card_width * 0.45), card_y - 30}, card_colour, 1)
            }
        }
    }

    { // fps counter
        string_buffer: [256]u8
        builder := strings.builder_from_bytes(string_buffer[0:])

        text := fmt.sbprintf(&builder, "FPS: %v", 1 / delta_time)
        draw_text(text, {state.screen_width - 150, state.screen_height - 20}, YELLOW, 1)
    }

    if state.in_command_mode {
        draw_rectangle({state.screen_width / 2, state.screen_height / 2}, {state.screen_width, 20}, BLACK)
        draw_rectangle({state.screen_width / 2, (state.screen_height / 4) - 10}, {state.screen_width, state.screen_height / 2}, with_alpha(BLACK, 0.8))
    
        if state.command_input_length > 0 {
            input := transmute(string)state.command_input_buffer[0:state.command_input_length]
            draw_text(input, {5, state.screen_height / 2}, WHITE, 1)
        }

        if state.command_output_length > 0 {
            output := transmute(string)state.command_output_buffer[0:state.command_output_length]
            draw_text(output, {10, 10}, YELLOW, 1.2)
        }
    }
}

enemies_for_wave :: proc(wave: uint) -> uint {
    return 10 + (wave * 3)
}

///////////////////////////////// @commands
command_echo :: proc(message: string) {
    log.info(message)
}

command_kill :: proc() {
    for &entity in state.entities[0:state.entity_count] {
        if .AI in entity.flags {
            entity.dynamic_flags += {.DELETE}
        }
    }
}

command_restart :: proc() {
    // TODO:
    // because if we use the current logger the allocator debug info
    // will crsah it
    context.logger.procedure = log_callback

    reset_game()
    setup_game()
}

command_spawning :: proc(value: bool) {
    settings_spawning = value
    log.infof("Spawning set to %v", value)
}

command_nav :: proc() {
    settings_nav_mesh = !settings_nav_mesh
    log.infof("Nav Mesh display set to %v", settings_nav_mesh)
}

load_textures :: proc() -> bool {
    RESOURCE_DIR :: "resources/textures/"

    path := fmt.tprint(RESOURCE_DIR, "face", ".png", sep="")
    
    png_data, ok := os.read_entire_file(path)
    if !ok {
        log.errorf("error loading texture file %v", path)
        return false
    }

    stbi.set_flip_vertically_on_load(1)
    width, height, channels: i32

    data := stbi.load_from_memory(raw_data(png_data), auto_cast len(png_data), &width, &height, &channels, 4)
    if data == nil {
        log.errorf("error reading texture data with stbi: %v", path)
        return false
    }

    log.infof("loaded texture \"%v\" [%v x %v : %v bytes]", path, width, height, len(png_data))

    face_texture = Texture{
        width = width,
        height = height,
        data = data
    }

    return true
}

load_fonts :: proc() -> bool {
    RESOURCE_DIR :: "resources/fonts/"

    path := fmt.tprint(RESOURCE_DIR, "alagard", ".ttf", sep="")

    font_height := 15 // for some reason this only bakes properly at 15 ? it's a 16px font dou...

    ttf_data, ok := os.read_entire_file(path)
    if !ok || ttf_data == nil {
        log.errorf("error loading font file %v", path)
        return false
    }

    bake_result := stbtt.BakeFontBitmap(
        raw_data(ttf_data), 
        0, 
        auto_cast font_height, 
        auto_cast &alagard.bitmap,
        font_bitmap_w, 
        font_bitmap_h, 
        32, 
        char_count, 
        &alagard.characters[0]
    )

    if !(bake_result > 0) {
        log.errorf("not enough space in bitmap buffer for font %v", path)
        return false
    }

    output_path :: "build/alagard.png"
    write_result := stbi.write_png(output_path, auto_cast font_bitmap_w, auto_cast font_bitmap_h, 1, auto_cast &alagard.bitmap, auto_cast font_bitmap_w)	
    if write_result == 0 {
        // dont return false here because this is not needed for runtime
        log.error("could not write font \"%v\" to output image \"%v\"", path, output_path)
    }

    log.infof("loaded font \"%v\" [%v bytes]", path, len(ttf_data))
   
    return true
}

window_event_callback :: proc "c" (event: ^sapp.Event) {
    context = runtime.default_context()
    
    if event == nil {
        return
    }

    switch event.type {
    case .MOUSE_MOVE:
        state.mouse_screen_position = {event.mouse_x, event.mouse_y}
    case .MOUSE_UP: 
        fallthrough
    case .MOUSE_DOWN:
        if event.mouse_button == .INVALID {
            log.warn("got invalid mouse button input")
            return
        }

        state.mouse_button_inputs[event.mouse_button] = .DOWN if event.type == .MOUSE_DOWN else .UP
    case .KEY_UP: 
        fallthrough
    case .KEY_DOWN:
        if event.key_code == .INVALID {
            log.warn("got invalid key input")
            return
        }

        state.key_inputs_this_frame[event.key_code] = .DOWN if event.type == .KEY_DOWN else .UP
    case .CHAR:
        state.character_input = event.char_code
    case .QUIT_REQUESTED:
        sapp.quit()
    case .INVALID, 
     .MOUSE_SCROLL, .MOUSE_ENTER, .MOUSE_LEAVE, 
     .TOUCHES_BEGAN, .TOUCHES_ENDED, .TOUCHES_MOVED, .TOUCHES_CANCELLED, 
     .RESIZED, .ICONIFIED, .RESTORED, .FOCUSED, .UNFOCUSED, .SUSPENDED, .RESUMED, 
     .CLIPBOARD_PASTED, .FILES_DROPPED:
    }
}

contains :: proc(list: []$T, t: T, f: proc(a: T, b: T) -> bool) -> (int, bool) {
    for value, i in list {
        if f(value, t) {
            return i, true
        }
    }

    return -1, false
}

allocator_callback :: proc(allocator_data: rawptr, mode: runtime.Allocator_Mode, size, alignment: int, old_memory: rawptr, old_size: int, location: runtime.Source_Code_Location = #caller_location) -> ([]byte, runtime.Allocator_Error) {
    KB :: 1024
    MB :: KB * 1024
    GB :: MB * 1024

    size_string := "b"
    converted_size := size
    if size >= KB && size < MB {
        size_string = "Kb"
        converted_size /= KB
    }
    else if size >= MB && size < GB {
        size_string = "Mb" 
        converted_size /= MB
    }
    else if size >= GB {
        size_string = "Gb" 
        converted_size /= GB
    }

    log.debugf("[%v] %v -> %v (%v%v)", mode, old_size, size, converted_size, size_string, location = location)
    return runtime.default_context().allocator.procedure(allocator_data, mode, size, alignment, old_memory, old_size, location)
}

log_callback :: proc(data: rawptr, level: runtime.Logger_Level, text: string, options: bit_set[runtime.Logger_Option], location := #caller_location) {
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


    file := filepath.base(location.file_path)
    fmt.printfln("[%v] %v(%v:%v) %v", level, file, location.line, location.column, text) 

    if level != .Debug {
        fmt.print(ansi.CSI + ansi.RESET + ansi.SGR)
    }
}

command_log_callback :: proc(data: rawptr, level: runtime.Logger_Level, text: string, options: bit_set[runtime.Logger_Option], location := #caller_location) {
    bytes := transmute([]u8) fmt.tprintf("[%v] %v\n", level, text)

    state.command_output_length = len(bytes)
    for b, i in bytes {
        state.command_output_buffer[i] = b
    }

}










