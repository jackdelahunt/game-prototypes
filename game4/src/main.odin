package src

// TODO:
// make levels
//  - phase 1: basic pushing with buttons    : 0/3
//  - phase 2: key and doors                 : 0/3
//  - phase 3: no undo                       : 0/3
//  - phase 4: lamps and mirrors             : 0/3
//  - phase 5: everything                    : 0/3

// hint for each level

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
import "core:encoding/json"
import "core:mem"

import sa "core:container/small_array"

import stbi "vendor:stb/image"
import stbtt "vendor:stb/truetype"

import sg "sokol/gfx"
import sapp "sokol/app"
import slog "sokol/log"
import sglue "sokol/glue"

import shaders "shaders"

DEFAULT_SCREEN_WIDTH	:: 1080
DEFAULT_SCREEN_HEIGHT	:: 720

TICKS_PER_SECOND    :: 30.0
TICK_RATE           :: 1.0 / TICKS_PER_SECOND
TICKS_PER_UNDO      :: 2 // TODO: this is not actually happening this fast

MAX_ENTITIES	:: 1_000
MAX_QUADS	:: 10_000

MAX_SAVE_POINTS :: 200

GRID_WIDTH      :: 5
GRID_HEIGHT     :: 5
GRID_TILE_SIZE  :: 50

START_MAXIMISED :: false

START_LEVEL :: LevelId.TEST
REPEAT_LEVEL :: false

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
    entity_count: int,
    current_level: LevelId,
    level: Level,
    save_this_tick: bool,

    // renderer state
    camera: struct {
        position: Vector2,
        // length in world units from camera centre to top edge of camera view
        // length of camera centre to side edge is this * aspect ratio
        orthographic_size: f32,
        near_plane: f32,
        far_plane: f32
    },
    quads: []Quad,
    quad_count: int,
    render_pipeline: sg.Pipeline,
    bindings: sg.Bindings,
    pass_action: sg.Pass_Action
}

state := State {}

// @entity
Entity :: struct {
    // meta
    id: EntityId,
    flags: bit_set[EntityFlag],

    // core
    position: Vector2,
    grid_position: Vector2i,
    size: Vector2,
    rotation: f32,
    direction: Direction,

    // rendering
    shape: EntityShape,
    colour: Colour,
    layer: Layer,

    // flag: player
    player_type: PlayerType,

    // flag: watching
    watching: sa.Small_Array(5, EntityId),

    // flag: jump pad
    jump_pad_target: Vector2i,

    // flag: lamp
    lamp_type: LampType,
}

EntityId :: uint

EntityFlag :: enum {
    NONE = 0,

    // entity types
    PLAYER,
    BUTTON,
    KEY,
    KEY_DOOR,
    DOOR,
    LAMP,
    LIGHT_DETECTOR,
    MIRROR,
    LAUNCH_PAD,

    // state flags
    DELETED,
    ACTIVE_PLAYER,
    ACTIVATED,
    MOVEABLE,
    ROTATABLE,
    WATCHING,
    NON_BLOCKING,
    NO_UNDO,
    IN_PLAYER_HAND,
    KEY_USED
}

EntityShape :: enum {
    RECTANGLE,
    CIRCLE
}

PlayerType :: enum {
    PRIMARY,
    SECONDARY
}

// @level
Level :: struct {
    // computed
    id:             LevelId,
    tiles:          [][]bool,
    width:          int,
    height:         int,
    end:            Vector2i,
    complete:       bool,
    save_points:    [dynamic][]Entity,
 
    // defined in level file
    layout:         [][]TileLayout,
    entities:       []EntityConfig,
}

LevelId :: enum {
    TEST,
    ONE,
    TWO,
}

EntityConfig :: struct {
    position: Vector2i,
    watching: []Vector2i,
    direction: Direction,
    no_undo: bool,
    jump_pad_target: Vector2i,
}

TileLayout :: enum {
    EMPTY               = 0,
    THREE               = 1,
    END                 = 2,
    PLAYER              = 3,
    ALT_PLAYER          = 4,
    ROCK                = 5,
    BUTTON              = 6,
    WALL                = 7,
    DOOR                = 8,
    LAMP_LIGHT          = 9,
    LAMP_DEATH          = 10,
    LIGHT_DETECTOR_LIGHT= 11,
    LIGHT_DETECTOR_DEATH= 12,
    MIRROR              = 13,
    KEY                 = 14,
    KEY_DOOR            = 15,
    LAUNCH_PAD          = 16,
}

level_name :: proc(level: LevelId) -> string {
    switch level {
        case .TEST:     return "Test_Level"
        case .ONE:      return "A_New_Hope"
        case .TWO:      return "Islands"
    }

    unreachable()
}

next_level :: proc() {
    current_level_number := i64(state.current_level)

    if current_level_number == len(LevelId) - 1 {
        state.current_level = LevelId(0)
    } else {
        state.current_level = LevelId(current_level_number + 1)
    }

    restart()
}

previous_level :: proc() {
    current_level_number := i64(state.current_level)

    if current_level_number - 1 < 0 {
        state.current_level = LevelId(len(LevelId) - 1)
    } else {
        state.current_level = LevelId(current_level_number - 1)
    }

    restart()
}

save_tick :: proc() {
    state.save_this_tick = true  
}

create_save_point :: proc() {
    saved_entities := make([]Entity, state.entity_count, level_allocator)

    for i in 0..<state.entity_count {
        saved_entities[i] = state.entities[i]
    }

    append(&state.level.save_points, saved_entities)

    log.debugf("created save point with %v entities", len(saved_entities))
    log.debugf("%v total saved points", len(state.level.save_points))
}

load_save_point :: proc() {
    // the last save point in the list is considered the latest state of the game
    // to load the previous state, we pop the last save point off the list copy
    // the new save point at the end of the list. This does not get removed. 
    // Will only get removed when this is called again and it is the latest state
    // - 09/01/25

    if len(state.level.save_points) <= 1 {
        log.debug("tried to load save point, but not enough exist.. doing nothing")
        return
    }

    // removing current state and copying the remaining newest state
    _ = pop(&state.level.save_points) // remove current state
    saved_entities := state.level.save_points[len(state.level.save_points) - 1]

    for i in 0..<len(saved_entities) {
        current := &state.entities[i]
        saved := &saved_entities[i]

        // assumes entities are not moved in the buffer ever
        assert(current.id == saved.id)

        if .NO_UNDO in current.flags {
            continue
        }

        current^ = saved^
    }

    state.entity_count = len(saved_entities)
    log.debugf("loaded save point with %v entities", len(saved_entities))
}

fit_and_centre_camera :: proc() {
    total_width := f32(GRID_TILE_SIZE * state.level.width)
    total_height := f32(GRID_TILE_SIZE * state.level.height)

    state.camera.position = {total_width, total_height} * 0.5
    state.camera.position -= {GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.5 // centre camera on tile

    // orthographic size is found by getting what it would be to fit the width/height of the
    // level on level and using the largest of those so they are always going to fit
    // plus some padding - 11/01/25
    height_to_width_ratio := state.screen_height / state.screen_width 
    orthographic_size_w := (total_width * height_to_width_ratio * 0.5)
    orthographic_size_h := (total_height * 0.5)

    CAMERA_PADDING :: GRID_TILE_SIZE

    state.camera.orthographic_size = max(orthographic_size_w, orthographic_size_h) + CAMERA_PADDING
}

// @grid
GridTile :: struct {
    is_floor: bool,
} 

valid :: proc(grid_position: Vector2i) -> bool {
    return (grid_position.x >= 0 && grid_position.x < state.level.width) &&
           (grid_position.y >= 0 && grid_position.y < state.level.height)
}

is_tile_active :: proc(grid_position: Vector2i) -> bool {
    assert(valid(grid_position), "tried to get tile with invalid position")

    return state.level.tiles[grid_position.y][grid_position.x]
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
    LEFT,
    RIGHT,
    UP,
    DOWN,
}

oppisite :: proc(direction: Direction) -> Direction {
    switch direction {
        case .UP:       return .DOWN
        case .DOWN:     return .UP
        case .LEFT:     return .RIGHT
        case .RIGHT:    return .LEFT
        case .NONE:
    }

    unreachable()
}

clockwise :: proc(direction: Direction) -> Direction {
    switch direction {
        case .UP:       return .RIGHT
        case .DOWN:     return .LEFT
        case .LEFT:     return .UP
        case .RIGHT:    return .DOWN
        case .NONE:
    }

    unreachable()
}

// ouput direction that is reflected by a mirror based on the
// relative incoming direction and the mirror direction
// income direction is based on the direction relative to the 
// mirror
//
// if a beam travelling to the left hits the mirror, then the
// relative incoming direction is RIGHT as it is from the perspective
// of the mirror 
mirror_reflection :: proc(mirror_direction: Direction, relative_income_direction: Direction) -> Direction {
    switch mirror_direction {
        case .RIGHT, .LEFT: {
            switch relative_income_direction {
                case .UP: return .LEFT
                case .DOWN: return .RIGHT
                case .LEFT: return .UP
                case .RIGHT: return .DOWN
                case .NONE: unreachable()
            }
        }
        case .UP, .DOWN: {
            switch relative_income_direction {
                case .UP: return .RIGHT
                case .DOWN: return .LEFT
                case .LEFT: return .DOWN
                case .RIGHT: return .UP
                case .NONE: unreachable()
            }
        }
        case .NONE:
    }

    unreachable()
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
    }

    unreachable()
}

direction_to_position :: proc(start: Vector2i, end: Vector2i) -> Direction {
    delta := end - start

    if abs(delta.x) >= abs(delta.y) {
        if delta.x > 0 {
            return .RIGHT
        }
        else {
            return .LEFT
        }
    }
    else {
        if delta.y > 0 {
            return .UP
        }
        else {
            return .DOWN
        }
    }
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

DARK_GRAY   :: Colour{0.2, 0.2, 0.2, 1}
LIGHT_GRAY  :: Colour{0.8, 0.8, 0.8, 1}

YELLOW	    :: Colour{1, 0.9, 0.05, 1}
PINK	    :: Colour{0.8, 0.05, 0.6, 1}
BROWN       :: Colour{0.35, 0.16, 0.08, 1}
SKY_BLUE    :: Colour{0.2, 0.6, 0.8, 1}
ORANGE	    :: Colour{0.95, 0.6, 0, 1}

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

greyscale :: proc(colour: Colour) -> Colour {
    length := length(colour.rgb) / 3.0

    return Colour {
        length,
        length,
        length,
        colour.a,
    }
}

// @lamp
LampType :: enum {
    LIGHT,
    DEATH
}

lamp_colour :: proc(type: LampType) -> Colour {
    switch type {
        case .LIGHT: return ORANGE
        case .DEATH: return RED
    }

    unreachable()
}

create_entity :: proc(entity: Entity) -> ^Entity {
    @(static)
    id_counter: uint = 1

    ptr := &state.entities[state.entity_count]
    state.entity_count += 1

    ptr^ = entity

    ptr.id = id_counter
    id_counter += 1

    { // sanity checking some values
        assert(
            !(.NON_BLOCKING in entity.flags &&
            .MOVEABLE in entity.flags)
        )

        if .LAMP in entity.flags {
            assert(entity.direction != .NONE)
        }

        if .MIRROR in entity.flags {
            assert(entity.direction != .NONE || entity.direction != .UP || entity.direction != .DOWN)
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

get_entity_with_id :: proc(id: EntityId) -> ^Entity {
    for &entity in state.entities[0:state.entity_count] {
        if entity.id == id {
            return &entity
        }
    }

    return nil
}

get_first_entity_at_position :: proc(grid_position: Vector2i) -> ^Entity {
    for &entity in state.entities[0:state.entity_count] {
        if entity.grid_position == grid_position {
            return &entity
        }
    }

    return nil
}

get_entity_at_position_without_flags :: proc(grid_position: Vector2i, blocked_flags : bit_set[EntityFlag]) -> ^Entity {
    for &entity in state.entities[0:state.entity_count] {
        if entity.grid_position != grid_position {
            continue
        }

        if entity.flags & blocked_flags != {} {
            continue
        }

        return &entity
    }

    return nil
}

get_entity_at_position_with_flags :: proc(grid_position: Vector2i, searching_flags: bit_set[EntityFlag]) -> ^Entity {
    for &entity in state.entities[0:state.entity_count] {
        if entity.grid_position != grid_position {
            continue
        }

        if entity.flags & searching_flags == {} {
            continue
        }

        return &entity
    }

    return nil
}

GridPositionIterator :: struct {
    index: int,
    grid_position: Vector2i
}

new_grid_position_iterator :: proc(grid_position: Vector2i) -> GridPositionIterator {
    return {index = 0, grid_position = grid_position}
}

next :: proc(iterator: ^GridPositionIterator) -> ^Entity {
    for iterator.index < auto_cast state.entity_count {
        other := &state.entities[iterator.index]
        iterator.index += 1

        if other.grid_position == iterator.grid_position && !(.NON_BLOCKING in other.flags) {
            return other 
        }
    }

    return nil
}

// @textures
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
    context = custom_context()

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
        camera = {
            near_plane = 0.1,
            far_plane = 100
        },
        current_level = START_LEVEL,
        entities = make([]Entity, MAX_ENTITIES, eternal_allocator),
        quads = make([]Quad, MAX_QUADS, eternal_allocator)
    }

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
    // test_draw()
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

load_level :: proc(id: LevelId) -> bool {
    level := Level {
        id = id
    }

    name := level_name(id)
    path := fmt.tprintf("resources/levels/%v.level", name)

    { // read level file and load into level struct and do any needed conversion
        bytes, ok := os.read_entire_file(path, allocator = context.temp_allocator)
        if !ok {
            log.errorf("error loading level file %v", path)
            return false
        } 
    
        err := json.unmarshal_any(bytes, &level, spec = .JSON5, allocator = context.temp_allocator)
        if err != nil {
            log.errorf("error unmarshalling level file %v with error %v", path, err)
            return false
        }

        // reversing layout because you enter it as if y0 is the bottom
        // but it is actually the top, so just need to reverse the y axis
        slice.reverse(level.layout)
    }

    { // get and verify width and height of layout
        height := len(level.layout)
        width: int

        if height  == 0 {
            log.errorf("layout in level file %v is empty", path)
            return false
        }

        // width is assumed to be the first one, all others
        // are verified against this one
        width = len(level.layout[0])

        for &row, y in level.layout {
            if len(row) == width {
                continue
            }

            log.errorf("width of row (y=%v) in layout in level file %v does not match first row defined, expected %v, got %v", y, path, width, len(row))
            return false
        }

        level.width = width
        level.height = height
    }

    { // initialise tile grid based on given layout
        level.tiles = make([][]bool, level.height)
        for y in 0..<level.height {
            level.tiles[y] = make([]bool, level.width)
    
            for x in 0..<level.width {
                tile := level.layout[y][x]
                grid_position := Vector2i{x, y}
    
                is_floor := true
    
                switch tile {
                    case .EMPTY:                is_floor = false
                    case .THREE:
                    case .END:                  level.end = grid_position
                    case .PLAYER:               create_player(grid_position)
                    case .ALT_PLAYER:           create_secondary_player(grid_position)
                    case .ROCK:                 create_rock(grid_position)
                    case .BUTTON:               create_button(grid_position)
                    case .WALL:                 create_wall(grid_position)
                    case .DOOR:                 create_door(grid_position)
                    case .LAMP_LIGHT:           create_lamp(grid_position, .LIGHT)
                    case .LAMP_DEATH:           create_lamp(grid_position, .DEATH)
                    case .LIGHT_DETECTOR_LIGHT: create_light_detector(grid_position, .LIGHT)
                    case .LIGHT_DETECTOR_DEATH: create_light_detector(grid_position, .DEATH)
                    case .MIRROR:               create_mirror(grid_position)
                    case .KEY:                  create_key(grid_position)
                    case .KEY_DOOR:             create_key_door(grid_position)
                    case .LAUNCH_PAD:           create_launch_pad(grid_position)

                }
    
                level.tiles[y][x] = is_floor
            }
        }
    }

    level.save_points = make([dynamic][]Entity, level_allocator)

    // done before next step as it is assumed the level is set state and
    // entities are already created for this level
    state.level = level

    { // update the entities with the given config
        for entity_config, config_index in level.entities {
            entity := get_first_entity_at_position(entity_config.position)
            if entity == nil {
                 log.errorf("entities[%v].position (%v) is invalid no entity found, in level file %v", config_index, entity_config.position,  path)
                return false
            }

            // set watching
            for watching_position, watching_index in entity_config.watching {
                watching_entity := get_first_entity_at_position(watching_position)
                if watching_entity == nil {
                     log.errorf("entities[%v].watching[%v] (%v) is invalid no entity found, in level file %v", config_index, watching_index, watching_position,  path)
                    return false
                }

                sa.append(&entity.watching, watching_entity.id)
            }

            entity.direction = entity_config.direction
            entity.jump_pad_target = entity_config.jump_pad_target

            if entity_config.no_undo {
                entity.flags += {.NO_UNDO}
            }
            else {
                entity.flags -= {.NO_UNDO}
            }
        }
    }

    log.infof("loaded level %v", name)

    return true
}

load_textures :: proc() -> bool {
    RESOURCE_DIR :: "resources/textures/"

    path := fmt.tprint(RESOURCE_DIR, "face", ".png", sep="")
    
    png_data, ok := os.read_entire_file(path, eternal_allocator)
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

    ttf_data, ok := os.read_entire_file(path, eternal_allocator)
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











