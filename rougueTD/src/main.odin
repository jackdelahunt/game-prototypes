package src

import "core:fmt"
import "core:log"
import "base:runtime"
import "core:math/linalg"
import "core:time"
import "core:os"

import stbi "vendor:stb/image"
import stbtt "vendor:stb/truetype"

import sg "sokol/gfx"
import sapp "sokol/app"
import slog "sokol/log"
import sglue "sokol/glue"

import shaders "shaders"

SCREEN_WIDTH	:: 1000
SCREEN_HEIGHT	:: 750

MAX_ENTITIES	:: 256
MAX_QUADS	:: 512

PLAYER_SPEED :: 90
AI_SPEED :: 30

BASIC_ENEMY_DAMAGE :: 50
NEXUS_HEALTH :: 1000

TICKS_PER_SECONDS :: 20
SECONDS_PER_TICK :: 1 / TICKS_PER_SECONDS

///////////////////////////////// @state
state : State = {}

State :: struct {
    // timing
    tick_timer: f64,

    // inputs
    key_inputs: #sparse [Key]InputState,
    mouse_button_inputs: [3]InputState,
    mouse_screen_position: Vector2,

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

///////////////////////////////// @input
InputState :: enum {
    UP,
    DOWN,
}

Key :: sapp.Keycode
MouseButton :: sapp.Mousebutton

///////////////////////////////// @colour
Colour :: distinct Vector4

White	:: Colour{1, 1, 1, 1}
Red	:: Colour{0.9, 0.05, 0.05, 1}
DarkRed	:: Colour{0.4, 0.05, 0.05, 1}
Green	:: Colour{0.05, 0.9, 0.05, 1}
Blue	:: Colour{0.05, 0.05, 0.9, 1}
Gray	:: Colour{0.4, 0.4, 0.4, 1}
SkyBlue :: Colour{0.07, 0.64, 0.72, 1}

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
}

EntityFlag :: enum {
    PLAYER,
    AI,
    NEXUS,
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
    { // load resources
	loading_ok := true

	loading_ok = load_textures()
	if !loading_ok {
	    fmt.println("error loading textures.. exiting")
	    return
	}
    
	loading_ok = load_fonts()
	if !loading_ok {
	    fmt.println("error loading fonts.. exiting")
	    return
	}
    }

    { // init state
	state.tick_timer = 0

	state.camera_position = {0, 0}
	state.zoom = 0.01

	// floor
	create_entity(Entity {
	    position = {0, 0}, 
	    size = {300, 300}, 
	    colour = White
	})

	// player
	create_entity(Entity {
	    flags = {.PLAYER},
	    position = {0, 30}, 
	    size = {10, 10}, 
	    colour = Blue
	})	

	// nexus
	create_entity(Entity {
	    flags = {.NEXUS, .HAS_HEALTH},
	    position = {0, 0}, 
	    size = {100, 40}, 
	    colour = Gray,
	    health = NEXUS_HEALTH,
	    max_health = NEXUS_HEALTH
	})
    }

    sapp.run({
	init_cb = renderer_init,
	frame_cb = renderer_frame,
	cleanup_cb = renderer_cleanup,
	event_cb = window_event_callback,
	width = SCREEN_WIDTH,
	height = SCREEN_HEIGHT,
	window_title = "sokol window",
	icon = { sokol_default = true },
	logger = { func = slog.func },
    })
}

///////////////////////////////// @frame
frame :: proc() {
    delta_time := sapp.frame_duration()
    state.tick_timer += delta_time

    if state.tick_timer >= SECONDS_PER_TICK {
	update()
	state.tick_timer = 0
    }

    physics(auto_cast delta_time)
    draw() 
}

///////////////////////////////// @update
update :: proc() {
    if state.key_inputs[.ESCAPE] == .DOWN {
	sapp.quit()
    }

    if state.key_inputs[.SPACE] == .DOWN {
	create_entity(Entity {
	    flags = {.AI},
	    position = {-90, 90}, 
	    size = {10, 10}, 
	    colour = Red
	})
    }

    // update pass
    for &entity in state.entities[0:state.entity_count] {
	player: {
	    if !(.PLAYER in entity.flags) {
		break player
	    }

	    input_vector: Vector2

	    if state.key_inputs[.A] == .DOWN {
		input_vector.x -= 1
	    }
     
	    if state.key_inputs[.D] == .DOWN {
		input_vector.x += 1
	    }
     
	    if state.key_inputs[.S] == .DOWN {
		input_vector.y -= 1
	    }
     
	    if state.key_inputs[.W] == .DOWN {
		input_vector.y += 1
	    }

	    entity.velocity = normalize(input_vector) * PLAYER_SPEED
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

///////////////////////////////// @draw
draw :: proc() {
    for &entity in state.entities[0:state.entity_count] {
	draw_quad(entity.position, entity.size, entity.rotation, entity.colour)

	health: {
	    if !(.HAS_HEALTH in entity.flags) {
		break health
	    }

	    health_ratio := entity.health / entity.max_health
	    health_bar_width := entity.size.x * 0.75

	    draw_quad(entity.position + {0, (entity.size.y * 0.5) + 10}, {health_bar_width, 5}, 0, DarkRed)
	    draw_quad(entity.position + {0, (entity.size.y * 0.5) + 10}, {health_bar_width * health_ratio, 5}, 0, Red)
	}
    }
}

load_textures :: proc() -> bool {
    RESOURCE_DIR :: "resources/textures/"

    path := fmt.tprint(RESOURCE_DIR, "face", ".png", sep="")
    
    png_data, ok := os.read_entire_file(path)
    if !ok {
	fmt.printfln("error loading texture file %v", path)
	return false
    }

    stbi.set_flip_vertically_on_load(1)
    width, height, channels: i32

    data := stbi.load_from_memory(raw_data(png_data), auto_cast len(png_data), &width, &height, &channels, 4)
    if data == nil {
	fmt.printfln("error reading texture data with stbi: %v", path)
	return false
    }

    fmt.printfln("loaded texture \"%v\" [%v x %v : %v bytes]", path, width, height, len(png_data))

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
	fmt.printfln("error loading font file %v", path)
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
	fmt.printfln("not enough space in bitmap buffer for font %v", path)
	return false
    }

    output_path :: "build/alagard.png"
    write_result := stbi.write_png(output_path, auto_cast font_bitmap_w, auto_cast font_bitmap_h, 1, auto_cast &alagard.bitmap, auto_cast font_bitmap_w)	
    if write_result == 0 {
	// dont return false here because this is not needed for runtime
	fmt.printfln("could not write font \"%v\" to output image \"%v\"", path, output_path)
    }

    fmt.printfln("loaded font \"%v\" [%v bytes]", path, len(ttf_data))
   
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

	state.key_inputs[event.key_code] = .DOWN if event.type == .KEY_DOWN else .UP
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









