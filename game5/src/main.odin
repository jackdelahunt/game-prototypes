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

import "vendor:glfw"
import gl "vendor:OpenGL"
import stbi "vendor:stb/image"
import stbtt "vendor:stb/truetype"
import stbrp "vendor:stb/rect_pack"

// TODO:
// player getting abilities from enemimes
// differant enemies with different abilties
// nest to spawn enemies
// player can destroy nests

// TODO, abilities:
// level 1:
//  - armour I
//  - speed
// level 2:
//  - power dash (tank enemy that dashes at player, player can dash at emeies)
// level 3:
//  - multi spawn (on death enemy spawn 4 smaller versions of itself, player can spawn smaller versions to attack enemies)
// level 4:
//  - armour 2?? same as armour just more
//  - lava pit (enemies shoots projectiles to cover floor in lava, player does the same)

// record:
// start: 21/01/2025
// total time: 9:10 hrs
// start: 23:30

// indev settings
LOG_COLOURS         :: false
OPENGL_MESSAGES     :: false
WRITE_DEBUG_IMAGES  :: true
V_SYNC              :: true
DRAW_ARMOUR_BUBBLE  :: true
NO_ENEMY_ATTACK     :: false
CAN_RELOAD_TEXTURES :: true

// internal settings
MAX_ENTITIES :: 50_000

// gameplay settings
MAX_PLAYER_HEALTH       :: 100
PLAYER_SPEED            :: 400
PLAYER_SHOOT_COOLDOWN   :: 0.03
PLAYER_REACH_SIZE       :: 100

MAX_AI_HEALTH           :: 80
AI_SPEED                :: 200
AI_ATTACK_COOLDOWN      :: 0.5
AI_ATTACK_DISTANCE      :: 10

BULLET_SPEED            :: 1000

MAX_ARMOUR              :: 100
ARMOUR_REGEN_RATE       :: 25
ARMOUR_REGEN_COOLDOWN   :: 5

// player settings
GAMEPAD_STICK_DEADZONE      :: 0.15
GAMEPAD_TRIGGER_DEADZONE    :: -0.6 // -1 is no input

// -------------------------- @global ---------------------------
state: State

State :: struct {
    width: f32,
    height: f32,
    window: glfw.WindowHandle,
    keys: [348]InputState,
    gamepad: glfw.GamepadState,
    time: f64,
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
}

InputState :: enum {
    up,
    down,
    pressed,
}

main :: proc() {
    context = custom_context()
    
    state = {
        width = 1440,
        height = 1080,
        camera = {
            position = {0, 0},
            // length in world units from camera centre to top edge of camera view
            // length of camera centre to side edge is this * aspect ratio
            orthographic_size = 450,
            near_plane = 0.01,
            far_plane = 100
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
    }

    start()

    for !glfw.WindowShouldClose(state.window) {
        if state.keys[glfw.KEY_ESCAPE] == .down {
            glfw.SetWindowShouldClose(state.window, true)
        }

        now := glfw.GetTime()
        delta_time := f32(now - state.time)
        state.time = now 

        input()
        update(delta_time)
        physics(delta_time)
        draw(delta_time)

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

        glfw.SwapBuffers(state.window)
    }

    glfw.DestroyWindow(state.window)
    glfw.Terminate()
}

// -------------------------- @game -----------------------
Entity :: struct {
    // meta
    flags: bit_set[EntityFlag],
    created_time: f64,

    // global
    position: v2,
    size: v2,
    velocity: v2,
    mass: f32,
    texture: TextureHandle,
    attack_cooldown: f32,

    // flag: player
    aim_direction: v2,

    // flag: ability pickup
    pickup_type: PickupType,

    // ability: armour
    armour: f32,
    armour_regen_cooldown: f32,

    // flag: has_health
    health: f32,
}

EntityFlag :: enum {
    player,
    ai,
    projectile,
    ability_pickup,

    armour_ability,
    speed_ability,

    solid_hitbox,
    static_hitbox,
    trigger_hitbox,
    interactable,

    has_health,

    to_be_deleted
}

PickupType :: enum {
    armour,
    speed
}

start :: proc() {
    create_player({100, 100})
    // create_ai({-300, -300})
    create_ability_pickup({0, 0}, .armour)
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

    glfw.PollEvents()

    // TODO: handle disconnect ??
    if glfw.GetGamepadState(glfw.JOYSTICK_1, &state.gamepad) == 0 {
        log.error("no gamepad detected")
    }
}

update :: proc(delta_time: f32) {
    if state.gamepad.axes[glfw.GAMEPAD_AXIS_LEFT_TRIGGER] > GAMEPAD_TRIGGER_DEADZONE {
        create_ai({0, 0}) 
    }

    when CAN_RELOAD_TEXTURES {
        if state.keys[glfw.KEY_R] == .down {
            ok := reload_textures(&state.renderer)
            if !ok {
                log.error("tried to reload textures but failed...")
            }
        }
    }

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
        }

        player_update: {
            if !(.player in entity.flags) {
                break player_update
            }

            { // set aim
                aim_vector := v2 {
                    state.gamepad.axes[glfw.GAMEPAD_AXIS_RIGHT_X],
                    -state.gamepad.axes[glfw.GAMEPAD_AXIS_RIGHT_Y]
                }

                input_length := linalg.length(aim_vector)
    
                if input_length > GAMEPAD_STICK_DEADZONE {
                    entity.aim_direction = linalg.normalize(aim_vector)
                }
            }

            { // movement
                entity.velocity = 0
    
                input_vector := v2 {
                    state.gamepad.axes[glfw.GAMEPAD_AXIS_LEFT_X],
                    -state.gamepad.axes[glfw.GAMEPAD_AXIS_LEFT_Y] // inverted for some reason ??
                }
    
                input_length := linalg.length(input_vector)
    
                if input_length > GAMEPAD_STICK_DEADZONE {
                    if input_length > 1 {
                        input_vector = linalg.normalize(input_vector)
                    }
    
                    entity.velocity = input_vector * PLAYER_SPEED
                }
            }

            shooting: { 
                if state.gamepad.axes[glfw.GAMEPAD_AXIS_RIGHT_TRIGGER] < GAMEPAD_TRIGGER_DEADZONE {
                    break shooting
                }

                if entity.attack_cooldown != 0 {
                    break shooting
                }
                   
                entity.attack_cooldown = PLAYER_SHOOT_COOLDOWN
                create_bullet(entity.position, entity.aim_direction * BULLET_SPEED)
            }

            interact: {
                if state.gamepad.buttons[glfw.GAMEPAD_BUTTON_X] != glfw.PRESS {
                    break interact
                }

                for &other in state.entities[0:state.entity_count] {
                    if !(.interactable in other.flags) {
                        continue 
                    }

                    assert(.ability_pickup in other.flags, "only interactables are pickups right now")

                    if linalg.distance(entity.position, other.position) < PLAYER_REACH_SIZE {
                        pickups_ability := ability_from_pickup_type(other.pickup_type)

                        entity.flags += {pickups_ability}
                        other.flags += {.to_be_deleted}
                    }
                }
            }
        }

        ai_update: {
            if !(.ai in entity.flags) {
                break ai_update
            }

            entity.velocity = 0

            player := get_entity_with_flag(.player)
            if player == nil {
                break ai_update
            }

            { // move
                direction := linalg.normalize(player.position - entity.position)
                entity.velocity = direction * AI_SPEED    
            }

            attack: { // attack
                if entity.attack_cooldown > 0 {
                    break attack
                }

                when NO_ENEMY_ATTACK {
                    break attack
                }

                distance := distance_between_entity_edges(&entity, player)
                if linalg.max(distance) < AI_ATTACK_DISTANCE {
                    hit_player := aabb_collided(entity.position, entity.size + AI_ATTACK_DISTANCE * 2, player.position, player.size)
                    if hit_player {
                        log.info("hit")
                        entity_take_damage(player, 10)
                        entity.attack_cooldown = AI_ATTACK_COOLDOWN
                    }
                }
            }
        }

        projectile_update: {
            if !(.projectile in entity.flags) {
                break projectile_update
            }

            if state.time - entity.created_time > 3 {
                entity.flags += {.to_be_deleted} 
            }
        }

        health_update: {
            if !(.has_health in entity.flags) {
                break health_update
            }

            if entity.health == 0 {
                entity.flags += {.to_be_deleted}

                if .ai in entity.flags {
                    // create_ai({0, 0})
                }
            }
        }

        armour_update: {
            if !(.armour_ability in entity.flags) {
                break armour_update
            }
            
            if entity.armour != MAX_ARMOUR && entity.armour_regen_cooldown == 0 {
                entity.armour += ARMOUR_REGEN_RATE * delta_time

                if entity.armour > MAX_ARMOUR {
                    entity.armour = MAX_ARMOUR
                }
            }
        }
    }

    i := 0
    for i < state.entity_count {
        entity := &state.entities[i]
    
        if .to_be_deleted in entity.flags {
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

        if .has_health in entity.flags {
            health_bar_width := entity.size.x * 2
            percentage_health_left := entity.health / max_health(&entity)

            highlight_colour = mix(RED, SKY_BLUE, percentage_health_left)
        }

        if .armour_ability in entity.flags && DRAW_ARMOUR_BUBBLE {
            armour_colour: v4

            if entity.armour == MAX_ARMOUR {
                armour_colour = YELLOW
            } else {
                armour_colour = BLUE
            }

            armour_alpha := entity.armour / MAX_ARMOUR

            draw_texture(.armour, entity.position, entity.size * 2.5, alpha(WHITE, armour_alpha), alpha(armour_colour, armour_alpha))
        }

        if .player in entity.flags {
            for &other in state.entities[0:state.entity_count] {
                if !(.interactable in other.flags) {
                    continue 
                }

                if linalg.distance(entity.position, other.position) < PLAYER_REACH_SIZE {
                    ICON_SIZE :: 25

                    icon_position := other.position + {0, (other.size.y * 0.5) + ICON_SIZE}

                    draw_texture(.x_button, icon_position, {ICON_SIZE, ICON_SIZE}, WHITE, WHITE)
                }
            }
        }

        draw_texture(entity.texture, entity.position, entity.size, base_colour, highlight_colour)
    } 

    in_screen_space = true

    player_hud: { // player info
        player := get_entity_with_flag(.player)
        if player == nil {
            break player_hud
        }

        { // armour
            armour_bar_width    : f32 = state.width * 0.3
            armour_bar_height   : f32 = 50
            bar_position := v2{state.width * 0.5, state.height - (armour_bar_height * 0.5)}

            bar_colour := SKY_BLUE
            background_bar_colour := brightness(bar_colour, 0.4)

            percentage_of_armour := player.armour / MAX_ARMOUR

            if percentage_of_armour == 1 {
                draw_rectangle(bar_position, v2{armour_bar_width, armour_bar_height} + 10, YELLOW)
            }

            draw_rectangle(bar_position, {armour_bar_width, armour_bar_height}, background_bar_colour)
            draw_rectangle(bar_position, {armour_bar_width * percentage_of_armour, armour_bar_height}, bar_colour)
        }

        { // health
            health_bar_width    : f32 = state.width * 0.2
            health_bar_height   : f32 = 20

            bar_colour := RED
            background_bar_colour := brightness(bar_colour, 0.4)

            percentage_of_health := player.health / MAX_PLAYER_HEALTH
           
            draw_rectangle({state.width * 0.5, state.height - (health_bar_height * 0.5)}, {health_bar_width, health_bar_height}, background_bar_colour)
            draw_rectangle({state.width * 0.5, state.height - (health_bar_height * 0.5)}, {health_bar_width * percentage_of_health, health_bar_height}, bar_colour)
        }
    }

    { // game info 
        text := fmt.tprintf("E: %v/%v       Q: %v/%v", state.entity_count, MAX_ENTITIES, state.renderer.quad_count, MAX_QUADS)
        draw_text(text, {10, 10}, 30, BLACK, .bottom_left)
    }

    { // fps
        fps := math.trunc(1 / delta_time)
        text := fmt.tprintf("%v", fps)
        draw_text(text, {10, state.height - 35}, 30, BLACK, .bottom_left)
    }
}

create_entity :: proc(entity: Entity) -> ^Entity {
    ptr := &state.entities[state.entity_count]
    state.entity_count += 1

    ptr^ = entity

    ptr.created_time = state.time

    return ptr
}

create_player :: proc(position: v2) -> ^Entity {
    return create_entity({
        flags = {.player, .has_health, .solid_hitbox},
        position = position,
        size = {40, 40},
        mass = 200,
        texture = .player,
        aim_direction = {0, 1},
        health = MAX_PLAYER_HEALTH,
        armour = MAX_ARMOUR
    })
}

create_ai :: proc(position: v2) -> ^Entity {
    return create_entity({
        flags = {.ai, .armour_ability, .solid_hitbox, .has_health},
        position = position,
        size = {20, 20},
        mass = 1,
        texture = .cuber,
        health = MAX_AI_HEALTH,
        armour = MAX_ARMOUR
    })
}

create_ability_pickup :: proc(position: v2, type: PickupType) -> ^Entity {
    return create_entity({
        flags = {.interactable, .ability_pickup},
        position = position,
        size = {20, 20},
        texture = .chip,
        pickup_type = type,
    })
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

get_entity_with_flag :: proc(flag: EntityFlag) -> ^Entity {
    for &entity in state.entities[0:state.entity_count] {
        if flag in entity.flags {
            return &entity
        }
    }

    return nil
}

// TODO: this sucks
max_health :: proc(entity: ^Entity) -> f32 {
    if .player in entity.flags {
        return MAX_PLAYER_HEALTH
    }

    if .ai in entity.flags {
        return MAX_AI_HEALTH
    }

    unreachable()
}

entity_take_damage :: proc(entity: ^Entity, damage: f32) {
    assert(.has_health in entity.flags)

    // if entity has armour take from that first
    // if damage is more then armour left take remainder
    // from the health

    damage_to_health: f32

    if .armour_ability in entity.flags && entity.armour > 0 {
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
        entity_take_damage(other, 10) 
    }
}

ability_from_pickup_type :: proc(type: PickupType) -> EntityFlag {
    switch type {
        case .armour:   return .armour_ability
        case .speed:    return .speed_ability
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

// -------------------------- @renderer -----------------------
v2 :: [2]f32
v3 :: [3]f32
v4 :: [4]f32
Mat4 :: linalg.Matrix4f32

GL_MAJOR :: 4
GL_MINOR :: 6
MAX_QUADS :: 70_000

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
    chip,
    x_button,
}

Texture :: struct {
    width: int,
    height: int,
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

draw_rectangle :: proc(position: v2, size: v2, colour: v4) {
    draw_quad(position, size, colour, {}, DEFAULT_UV, .rectangle)
}

draw_texture :: proc(texture: TextureHandle, position: v2, size: v2, colour: v4, highlight_colour: v4) {
    draw_quad(position, size, colour, highlight_colour, state.renderer.textures[texture].uv, .texture)
}

draw_circle :: proc(position: v2, radius: f32, colour: v4) {
    draw_quad(position, {radius * 2, radius * 2}, colour, {}, DEFAULT_UV, .circle)
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
        draw_quad(translated_position + (scaled_size * 0.5), scaled_size, colour, {}, glyph.uvs, .font);
    } 
}

draw_quad :: proc(position: v2, size: v2, colour: v4, highlight_colour: v4, uv: [4]v2, draw_type: DrawType) {
    transformation_matrix: Mat4

    if in_screen_space {
        ndc_position := screen_position_to_ndc({position.x, position.y, 0})
        ndc_size := size / (v2{state.width, state.height} * 0.5)         
        transformation_matrix = linalg.matrix4_translate(ndc_position) * linalg.matrix4_scale(v3{ndc_size.x, ndc_size.y, 1})
    } else {
        // model matrix
        transformation_matrix = linalg.matrix4_translate(v3{position.x, position.y, 10}) * linalg.matrix4_scale(v3{size.x, size.y, 1})
    
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

    { // fill in default atlas data 
        i: int
        for i < ATLAS_BYTE_SIZE {
            atlas_data[i]       = 255 // r
            atlas_data[i + 1]   = 0   // g
            atlas_data[i + 2]   = 255 // b
            atlas_data[i + 3]   = 255 // a
    
            i += 4
        }
    }

    { // copy textures into atlas with rect pack
        RECT_COUNT :: len(TextureHandle)
        
        rp_context: stbrp.Context
        nodes:      [ATLAS_WIDTH]stbrp.Node
        rects:      [RECT_COUNT]stbrp.Rect

        stbrp.init_target(&rp_context, ATLAS_HEIGHT, ATLAS_HEIGHT, &nodes[0], ATLAS_WIDTH)

        for texture, i in TextureHandle {
            info := &renderer.textures[texture]

            rects[i] = {
                id = c.int(texture),
                w = stbrp.Coord(info.width),
                h = stbrp.Coord(info.height),
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

            bottom_y_uv := f32(rect.y) / f32(ATLAS_HEIGHT)
            top_y_uv    := f32(rect.y + rect.h) / f32(ATLAS_HEIGHT)
            left_x_uv   := f32(rect.x) / f32(ATLAS_HEIGHT)
            right_x_uv    := f32(rect.x + rect.w) / f32(ATLAS_HEIGHT)

            texture_info.uv = {
                {left_x_uv, top_y_uv},      // top left
                {right_x_uv, top_y_uv},     // top right
                {right_x_uv, bottom_y_uv},  // bottom right
                {left_x_uv, bottom_y_uv},   // bottom left
            }

            for row in 0..< rect.h {
                source_row := mem.ptr_offset(texture_info.data, row * rect.w * BYTES_PER_PIXEL)
                dest_row   := mem.ptr_offset(atlas_data, ((rect.y + row) * ATLAS_WIDTH + rect.x) * BYTES_PER_PIXEL) // flipped textures in atlas

                mem.copy(dest_row, source_row, int(rect.w) * BYTES_PER_PIXEL)
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
        case .chip:
            return "chip.png"
        case .x_button:
            return "x_button.png"
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
    glfw.SetFramebufferSizeCallback(window, glfw_size_callback)

    return window, true
}

glfw_error_callback :: proc "c" (error: c.int, description: cstring) {
    context = custom_context()
    log.errorf("glfw window error: [%v] %v", error, description)
}

glfw_key_callback :: proc "c" (window: glfw.WindowHandle, key: c.int, scancode: c.int, action: c.int, mods: c.int) {
    // https://www.glfw.org/docs/latest/input_guide.html
    current_key := &state.keys[key]

    switch action {
        case glfw.RELEASE:  current_key^ = .up
        case glfw.PRESS:    current_key^ = .down
        case glfw.REPEAT: 
    }
}

glfw_size_callback :: proc "c" (window: glfw.WindowHandle, width: c.int, height: c.int) {
    gl.Viewport(0, 0, width, height)
    state.width = f32(width)
    state.height = f32(height)
}

// -------------------------- @random -------------------------
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
