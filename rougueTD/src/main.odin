package src

// TODO:
// player can buy defences
// player can attack enemies
// mouse world position

import "core:fmt"
import "core:log"
import "base:runtime"
import "core:math/linalg"
import "core:time"
import "core:os"
import "core:strings"
import "core:math"
import "core:path/filepath"
import "core:encoding/ansi"

import stbi "vendor:stb/image"
import stbtt "vendor:stb/truetype"

import sg "sokol/gfx"
import sapp "sokol/app"
import slog "sokol/log"
import sglue "sokol/glue"

import shaders "shaders"

DEFAULT_SCREEN_WIDTH	:: 1300
DEFAULT_SCREEN_HEIGHT	:: 900

MAX_ENTITIES	:: 256
MAX_QUADS	:: 512

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

NO_DEBUG :: false
DEBUG_GIVE_MONEY    :: false when NO_DEBUG else true
DEBUG_NO_SPAWNING   :: false when NO_DEBUG else true

///////////////////////////////// @state
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

    // player global state
    gold: uint,
    selected_defence: DefenceType,

    // game state
    entities: [MAX_ENTITIES]Entity,
    entity_count: uint,

    // renderer state
    camera_position: Vector2,
    zoom: f32,
    quads: [MAX_QUADS]Quad,
    quad_count: uint,
    render_pipeline: sg.Pipeline,
    bindings: sg.Bindings,
    pass_action: sg.Pass_Action
}

state := State {
    screen_width = DEFAULT_SCREEN_WIDTH,
    screen_height = DEFAULT_SCREEN_HEIGHT,
    gold = 10_000 when DEBUG_GIVE_MONEY else 0,
    zoom = 2.5
}

///////////////////////////////// @context
game_context := runtime.default_context()

///////////////////////////////// @input
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
    
    // flag: health
    health: f32,
    max_health: f32,

    // flag: defence
    defence_type: DefenceType,
    defence_cooldown: f32,

    // flag: spawner
    spawner_cooldown: f32,

    // flag: weapon
    weapon: WeaponType,
    magazine_ammo: uint,
    firing_cooldown: f32,
    reload_cooldown: f32,
}

EntityFlag :: enum {
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
    TRIGGER_HITBOX
}

DynamicEntityFlag :: enum {
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

    { // init state
	
	// floor
	create_entity(Entity {
	    position = {0, 0}, 
	    size = {300, 300}, 
	    colour = WHITE
	})

	// player
	create_entity(Entity {
	    flags = {.PLAYER, .HAS_WEAPON, .SOLID_HITBOX},
	    position = {0, 30}, 
	    size = {10, 10}, 
	    colour = BLUE,
	    weapon = .SHOTGUN,
	    magazine_ammo = magazine_size(.SHOTGUN)
	})

	create_entity(Entity {
		flags = {.AI, .HAS_HEALTH, .SOLID_HITBOX},
		position = {0, 60}, 
		size = {10, 10}, 
		colour = DARK_RED,
		health = BASIC_ENEMY_HEALTH,
		max_health = BASIC_ENEMY_HEALTH
	    })

	// nexus
	create_entity(Entity {
	    flags = {.NEXUS, .HAS_HEALTH, .STATIC_HITBOX},
	    position = {0, 0}, 
	    size = {100, 40}, 
	    colour = GRAY,
	    health = NEXUS_HEALTH,
	    max_health = NEXUS_HEALTH
	})

	// spawner
	create_entity(Entity {
	    flags = {.SPAWNER},
	    position = {-90, 90}, 
	    size = {10, 10}, 
	    colour = PINK,
	})
    }

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

///////////////////////////////// @frame
frame :: proc() {
    delta_time := auto_cast sapp.frame_duration()
    state.tick_timer += delta_time

    // only does once per frame as it it expensive
    state.mouse_world_position = screen_position_to_world_position(state.mouse_screen_position)

    if state.tick_timer >= TICK_RATE {
	apply_inputs()
	update()
	state.tick_timer = 0
    }

    physics(auto_cast delta_time)
    draw() 
}

///////////////////////////////// @apply_inputs
apply_inputs :: proc() {
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
update :: proc() {
    if state.key_inputs[.ESCAPE] == .DOWN {
	sapp.quit()
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

	    nexus := get_entity_with_flag(.NEXUS)
	    if nexus == nil {
		entity.velocity = 0
		break ai
	    }

	    delta := nexus.position - entity.position
	    distance := length(delta)
	    direction := normalize(delta)
	    entity.velocity = direction * AI_SPEED

	    if distance < 10 {
		entity.dynamic_flags += {.DELETE}
		entity_take_damage(nexus, BASIC_ENEMY_DAMAGE)
	    }
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

	    if DEBUG_NO_SPAWNING {
		break spawner
	    }

	    if entity.spawner_cooldown > 0 {
		break spawner
	    }

	    entity.spawner_cooldown = SPAWNER_COOLDOWN

	    create_entity(Entity {
		flags = {.AI, .HAS_HEALTH, .SOLID_HITBOX},
		position = entity.position, 
		size = {10, 10}, 
		colour = DARK_RED,
		health = BASIC_ENEMY_HEALTH,
		max_health = BASIC_ENEMY_HEALTH
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

on_trigger_collision :: proc(trigger: ^Entity, other: ^Entity) {
    if !(.AI in other.flags) {
	return
    }

    assert(.HAS_HEALTH in other.flags)

    log.info("giving damage")
    entity_take_damage(other, 20)
}

///////////////////////////////// @physics
physics :: proc(delta_time: f32) {
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
		if !(distance_for_collision[0] >= distance_abs[0] && distance_for_collision[1] >= distance_abs[1])
                {
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
draw :: proc() {
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

	draw_rectangle(entity.position, entity.size, entity_colour, entity.rotation)	
    }

    in_screen_space = true

    player_ammo: {
	player := get_entity_with_flag(.PLAYER)
	if player == nil {
	    break player_ammo
	}

	string_buffer: [20]u8
        builder := strings.builder_from_bytes(string_buffer[0:])
        text := fmt.sbprintf(&builder, "Ammo: %v", player.magazine_ammo) // utf8 lol
	
	draw_rectangle({state.screen_width - 150, 25}, {300, 50}, BLACK)
	draw_text(text, {state.screen_width - 275, 25}, YELLOW, 3)
    }

    { // display gold count
        string_buffer: [20]u8
        builder := strings.builder_from_bytes(string_buffer[0:])
        text := fmt.sbprintf(&builder, "%v", state.gold)
   
	background := Vector2{f32(25 * (1 + len(text))), 60}
        draw_rectangle(background * 0.5, background, BLACK)
	draw_text(text, {20, background.y / 2}, YELLOW, 3)
    }

    { // bottom defence layout
	card_width : f32 = state.screen_width * 0.15
	padding : f32 = card_width * 0.2
	card_y : f32 = state.screen_height - 25
	card_start_x : f32 = 100

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
    case .QUIT_REQUESTED:
	sapp.quit()
    case .INVALID, 
	 .CHAR, 
	 .MOUSE_SCROLL, .MOUSE_ENTER, .MOUSE_LEAVE, 
	 .TOUCHES_BEGAN, .TOUCHES_ENDED, .TOUCHES_MOVED, .TOUCHES_CANCELLED, 
	 .RESIZED, .ICONIFIED, .RESTORED, .FOCUSED, .UNFOCUSED, .SUSPENDED, .RESUMED, 
	 .CLIPBOARD_PASTED, .FILES_DROPPED:
    }
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









