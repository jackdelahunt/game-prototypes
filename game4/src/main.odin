package src

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

DEFAULT_SCREEN_WIDTH	:: 1200
DEFAULT_SCREEN_HEIGHT	:: 900

TICKS_PER_SECOND :: 20.0
TICK_RATE :: 1.0 / TICKS_PER_SECOND

MAX_ENTITIES	:: 5_000
MAX_QUADS	:: 15_000

GRID_WIDTH  :: 5
GRID_HEIGHT :: 5
GRID_TILE_SIZE :: 50

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

    // game state
    entities: []Entity,
    entity_count: uint,
    grid: [GRID_HEIGHT][GRID_WIDTH]GridTile,

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

// @grid
GridTile :: struct {
    is_floor: bool,
    entity_id: Maybe(EntityId),
} 

setup_grid :: proc() {
    for y in 0..<GRID_HEIGHT {
        for x in 0..<GRID_WIDTH {
            state.grid[y][x] = {
                is_floor = true,
                entity_id = nil
            } 
        }
    }

    // set camera to be the centre of the grid
    total_width := f32(GRID_WIDTH * GRID_TILE_SIZE)
    total_height := f32(GRID_HEIGHT * GRID_TILE_SIZE)

    state.camera_position = {total_width / 2, total_height / 2}
}

valid :: proc(grid_position: Vector2i) -> bool {
    return (grid_position.x >= 0 && grid_position.x < GRID_WIDTH) &&
           (grid_position.y >= 0 && grid_position.y < GRID_HEIGHT)
}

set_entity_in_grid :: proc (entity: ^Entity, position: Vector2i) {
    assert(entity.grid_position == position, "need to set entity grid position before setting it in the grid")
    state.grid[position.y][position.x].entity_id = entity.id
}

unset_entity_in_grid :: proc (position: Vector2i) {
    state.grid[position.y][position.x].entity_id = nil
}

get_tile :: proc(grid_position: Vector2i) -> ^GridTile {
    assert(valid(grid_position), "tried to get tile with invalid position")

    return &state.grid[grid_position.y][grid_position.x]
}

grid_position_to_world :: proc(grid_position: Vector2i) -> Vector2 {
    return Vector2 {
        f32(grid_position.x * GRID_TILE_SIZE),
        f32(grid_position.y * GRID_TILE_SIZE)
    }
}

// @Direction
Direction :: enum {
    NONE,
    UP,
    DOWN,
    LEFT,
    RIGHT
}

direction_grid_offset :: proc(direction: Direction) -> Vector2i {
    switch direction {
    case .UP:
        return {0, 1}
    case .DOWN:
        return {0, -1}
    case .LEFT:
        return {-1, 0}
    case .RIGHT:
        return {1, 0}
    case .NONE:
        assert(false, "tried to get grid offset from no direction")         
    }

    return {} // unreachable
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

//  @colour
Colour :: distinct Vector4

WHITE	    :: Colour{1, 1, 1, 1}
BLACK	    :: Colour{0, 0, 0, 1}
RED	    :: Colour{1, 0, 0, 1}
GREEN	    :: Colour{0, 1, 0, 1}
BLUE	    :: Colour{0, 0, 1, 1}
LIGHT_GRAY  :: Colour{0.8, 0.8, 0.8, 1}
YELLOW	    :: Colour{1, 0.9, 0.05, 1}
PINK	    :: Colour{0.8, 0.05, 0.6, 1}
BROWN       :: Colour{0.35, 0.16, 0.08, 1}

alpha :: proc(colour: Colour, alpha: f32) -> Colour {
    return {colour.r, colour.g, colour.b, alpha} 
}

// level == 1      -> same colour
// level == 0.5    -> 50% as beight
// level == 1.5    -> 150% as beight
// alpha is not effected
brightness :: proc(colour: Colour, level: f32) -> Colour {
    return Colour {
        colour.r * level,
        colour.g * level,
        colour.b * level,
        colour.a,
    }
}

// @entity
Entity :: struct {
    // meta
    id: EntityId,
    flags: bit_set[EntityFlag],
    inactive: bool,

    // core
    position: Vector2,
    size: Vector2,
    rotation: f32,
    shape: EntityShape,
    colour: Colour,
    grid_position: Vector2i
}

EntityId :: uint

EntityFlag :: enum {
    NONE = 0,
    PLAYER,
    PUSHABLE,
    DELETE,
}

EntityShape :: enum {
    RECTANGLE,
    CIRCLE
}

create_entity :: proc(entity: Entity) -> ^Entity {
    @(static)
    id_counter: uint = 0

    ptr := &state.entities[state.entity_count]
    state.entity_count += 1

    ptr^ = entity

    ptr.id = id_counter
    id_counter += 1

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

get_entity_with_id :: proc(id: EntityId) -> ^Entity {
    for &entity in state.entities[0:state.entity_count] {
        if entity.id == id {
            return &entity
        }
    }

    return nil
}

///////////////////////////////// @textures
Texture :: struct {
    width: i32,
    height: i32,
    data: [^]byte
}

face_texture: Texture

// @fonts
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

// @main
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

    state = State {
        screen_width = state.screen_width,
        screen_height = state.screen_height,
        camera_position = {0, 0},
        zoom = 2,
        entities = make([]Entity, MAX_ENTITIES),
        quads = make([]Quad, MAX_QUADS)
    }

    { // game setup
        setup_grid()
        create_player({0, 0})
        create_rock({2, 2})
        create_wall({3, 3})
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

// @frame
frame :: proc() {
    free_all(context.temp_allocator)

    delta_time := auto_cast sapp.frame_duration()
    state.tick_timer += delta_time

    // only does once per frame as it it expensive
    state.mouse_world_position = screen_position_to_world_position(state.mouse_screen_position)

    if state.tick_timer >= TICK_RATE {
        apply_inputs()
        update()
        state.tick_timer = 0
    }

    draw(auto_cast delta_time) 
}

// @apply_inputs
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

// @event
window_event_callback :: proc "c" (event: ^sapp.Event) {
    context = runtime.default_context()
    
    if event == nil {
        return
    }

    switch event.type {
    case .MOUSE_MOVE:
        // before setting need to convert to our own screen
        // co-ordinate system, sokol uses top left of screen as
        // 0,0 with y increaseing going down
        // we like 0,0 at bottom left with y increasing going up
        x := event.mouse_x
        y := state.screen_height - event.mouse_y
        state.mouse_screen_position = {x, y}
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









