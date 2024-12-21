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

DEFAULT_SCREEN_WIDTH	:: 1500
DEFAULT_SCREEN_HEIGHT	:: 1000

MAX_ENTITIES	:: 256
MAX_QUADS	:: 512

PLAYER_SPEED :: 90
AI_SPEED :: 30

BASIC_ENEMY_DAMAGE :: 50
BASIC_ENEMY_HEALTH :: 100
NEXUS_HEALTH :: 1000

TOWER_COST :: 500
TOWER_COOLDOWN :: 1
TOWER_RANGE :: 25
TOWER_DAMAGE :: 50

SPAWNER_COOLDOWN :: 2

GOLD_FROM_DAMAGE :: 10
GOLD_FROM_BASIC_ENEMY :: 50

TICKS_PER_SECONDS :: 20
TICK_RATE :: 1.0 / TICKS_PER_SECONDS

NO_DEBUG :: false
DEBUG_GIVE_MONEY :: false when NO_DEBUG else true

///////////////////////////////// @state
state : State = {}

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

    // player global state
    gold: uint,

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
}

EntityFlag :: enum {
    PLAYER,
    AI,
    NEXUS,
    DEFENCE,
    SPAWNER,
    HAS_HEALTH,
    DELETE,
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

///////////////////////////////// @defences
DefenceType :: enum {
    NONE,
    SHORT_TOWER,
    HIGH_TOWER,
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
	state.screen_width = DEFAULT_SCREEN_WIDTH
	state.screen_height = DEFAULT_SCREEN_HEIGHT

	state.tick_timer = 0
	state.gold = 10_000 if DEBUG_GIVE_MONEY else 0
	state.camera_position = {0, 0}
	state.zoom = 0.01

	// floor
	create_entity(Entity {
	    position = {0, 0}, 
	    size = {300, 300}, 
	    colour = WHITE
	})

	// player
	create_entity(Entity {
	    flags = {.PLAYER},
	    position = {0, 30}, 
	    size = {10, 10}, 
	    colour = BLUE
	})	

	// nexus
	create_entity(Entity {
	    flags = {.NEXUS, .HAS_HEALTH},
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

    // update pass
    for &entity in state.entities[0:state.entity_count] {
	// count down each cooldown in the entity
	entity.defence_cooldown = clamp(entity.defence_cooldown - TICK_RATE, 0, math.F32_MAX)
	entity.spawner_cooldown = clamp(entity.spawner_cooldown - TICK_RATE, 0, math.F32_MAX)

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

	    { // defence placing
		if state.key_inputs[.F] == .DOWN && state.gold >= defence_cost(.HIGH_TOWER) {
		    create_entity(Entity{
			flags = {.DEFENCE},
			position = entity.position,
			size = {10, 10},
			colour = GREEN,
			defence_type = .HIGH_TOWER
		    })

		    state.gold -= defence_cost(.HIGH_TOWER)
		}

		if state.key_inputs[.G] == .DOWN && state.gold >= defence_cost(.SHORT_TOWER) {
		    create_entity(Entity{
			flags = {.DEFENCE},
			position = entity.position,
			size = {15, 15},
			colour = YELLOW,
			defence_type = .SHORT_TOWER
		    })

		    state.gold -= defence_cost(.SHORT_TOWER)
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
		entity.flags += {.DELETE}
		entity_take_damage(nexus, BASIC_ENEMY_DAMAGE)
	    }
	}

	health: {
	    if !(.HAS_HEALTH in entity.flags) {
		break health
	    }

	    if entity.health <= 0 {
		entity.flags += {.DELETE}
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

	    if entity.spawner_cooldown > 0 {
		break spawner
	    }

	    entity.spawner_cooldown = SPAWNER_COOLDOWN

	    create_entity(Entity {
		flags = {.AI, .HAS_HEALTH},
		position = entity.position, 
		size = {10, 10}, 
		colour = DARK_RED,
		health = BASIC_ENEMY_HEALTH,
		max_health = BASIC_ENEMY_HEALTH
	    })
	}
    }

    // delete pass
    i : uint = 0
    for i < state.entity_count {
	entity := &state.entities[i]

	if .DELETE in entity.flags {
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

///////////////////////////////// @physics
physics :: proc(delta_time: f32) {
    for &entity in state.entities[0:state.entity_count] {
	entity.position += entity.velocity * delta_time
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
	draw_rectangle(entity.position, entity.size, entity.rotation, entity.colour)	

	health: {
	    if !(.HAS_HEALTH in entity.flags) {
		break health
	    }

	    if entity.health == entity.max_health {
		break health
	    }

	    health_ratio := entity.health / entity.max_health
	    health_bar_width := max(entity.size.x * 0.75, 15)
	    draw_rectangle(entity.position + {0, (entity.size.y * 0.5) + 5}, {health_bar_width, 4}, 0, DARK_RED)
	    draw_rectangle(entity.position + {0, (entity.size.y * 0.5) + 5}, {health_bar_width * health_ratio, 4}, 0, RED)
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
	    draw_rectangle(entity.position + {0, (entity.size.y * 0.5) + 5}, {bar_width, 3}, 0, BLUE)
	    draw_rectangle(entity.position + {0, (entity.size.y * 0.5) + 5}, {bar_width * cooldown_ratio, 3}, 0, SKY_BLUE)

	}
    }

    world_pos := screen_position_to_world_position(state.mouse_screen_position)
    log.debug(state.mouse_screen_position, world_pos)

    draw_circle(world_pos, 10, RED)

    in_screen_space = true

    string_buffer: [120]u8
    builder := strings.builder_from_bytes(string_buffer[0:])
    text := fmt.sbprintf(&builder, "%v", state.gold)

    background_width := (1 + cast(f32) len(text)) * 0.15

    draw_rectangle({-1, 1}, {background_width, 0.3}, 0, BLACK)
    draw_text(text, {-0.95, 0.9}, YELLOW, 100)
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









