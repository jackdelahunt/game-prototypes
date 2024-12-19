package src

// TODO:
// - drawing textures
// - drawing text

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

SCREEN_WIDTH	:: 900
SCREEN_HEIGHT	:: 700
MAX_ENTITIES	:: 256
MAX_QUADS	:: 512

///////////////////////////////// @state
state : State = {}

State :: struct {
    // timing
    start_time: time.Time,

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

Red	:: Colour{0.9, 0.05, 0.05, 1}
Green	:: Colour{0.05, 0.9, 0.05, 1}
Blue	:: Colour{0.05, 0.05, 0.9, 1}

///////////////////////////////// @entity
Entity :: struct {
    position: Vector2,
    size: Vector2,
    rotation: f32,
    colour: Colour,
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
    {
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
	state.start_time = time.now()

	state.camera_position = {0, 0}
	state.zoom = 0.8

	create_entity({-1.75, 0}, {1, 1}, {1, 0, 0, 0.33})
	create_entity({-1.25, 0}, {1, 1}, {0, 1, 0, 0.33})
	create_entity({-1.5, 0.5}, {1, 1}, {0, 0, 1, 0.33})
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

frame :: proc() {
    update()
    draw() 
}

update :: proc() {
    if state.key_inputs[.ESCAPE] == .DOWN {
	sapp.quit()
    }

    CAMERA_SPEED :: 0.1

    if state.key_inputs[.A] == .DOWN {
	state.camera_position.x -= CAMERA_SPEED
    }

    if state.key_inputs[.D] == .DOWN {
	state.camera_position.x += CAMERA_SPEED
    }

    if state.key_inputs[.S] == .DOWN {
	state.camera_position.y -= CAMERA_SPEED
    }

    if state.key_inputs[.W] == .DOWN {
	state.camera_position.y += CAMERA_SPEED
    }
}

create_entity :: proc(position: Vector2, size: Vector2, colour: Colour) {
    state.entities[state.entity_count] = Entity{position = position, size = size, rotation = 0, colour = colour}
    state.entity_count += 1
}

t_colour := Colour{0.2, 0.5, 0.8, 1}
t_colour_2 := Colour{0.3, 0.9, 0.6, 1}

draw :: proc() {
    for i in 0..<state.entity_count {
	draw_entity(&state.entities[i])

	if i == 1 {
	    state.entities[i].rotation += 0.05
	} else {
	    state.entities[i].rotation -= 0.02
	}
    }

    if true {
	delta_colour := Colour{0.005, 0.006, 0.004, 0}

	t_colour += delta_colour
	t_colour_2 += delta_colour * 0.9

	if t_colour.r > 1 {
	    t_colour.r = 0
	}

	if t_colour.g > 1 {
	    t_colour.g = 0
	}

	if t_colour.b > 1 {
	    t_colour.b = 0
	}

	if t_colour_2.r > 1 {
	    t_colour_2.r = 0
	}

	if t_colour_2.g > 1 {
	    t_colour_2.g = 0
	}

	if t_colour_2.b > 1 {
	    t_colour_2.b = 0
	}
    }

    draw_text("Hello sailor", {0, 1}, t_colour, 30)
    draw_text("Sugg", {0, -1}, t_colour_2, 20)
}

draw_quad :: proc(position: Vector2, size: Vector2, rotation: f32, colour: Colour, uvs: [4]Vector2, is_font := false) {
    width := sapp.widthf()
    height := sapp.heightf()

    model_matrix := translate_matrix(position) * scale_matrix(size) * rotate_matrix(rotation)
    view_matrix := view_matrix_from_position(state.camera_position) * scale_matrix(state.zoom)
    projection_matrix := linalg.matrix4_perspective_f32(90, width / height, 0, 10)
 
    model_view_projection := projection_matrix * view_matrix * model_matrix

    // the order that the vertices are drawen and that the 
    // index buffer is assuming is:
    //	    top left, top right, bottom right, bottom left

    quad := &state.quads[state.quad_count]
    state.quad_count += 1

    quad[0].position = (model_view_projection * Vector4{-0.5, 0.5, 0, 1}).xyz
    quad[1].position = (model_view_projection * Vector4{0.5, 0.5, 0, 1}).xyz
    quad[2].position = (model_view_projection * Vector4{0.5, -0.5, 0, 1}).xyz
    quad[3].position = (model_view_projection * Vector4{-0.5, -0.5, 0, 1}).xyz
 
    quad[0].colour = cast(Vector4) colour
    quad[1].colour = cast(Vector4) colour
    quad[2].colour = cast(Vector4) colour
    quad[3].colour = cast(Vector4) colour
 
    quad[0].texture_uv = uvs[0] 
    quad[1].texture_uv = uvs[1] 
    quad[2].texture_uv = uvs[2] 
    quad[3].texture_uv = uvs[3]

    texture_index : f32 = 0 if !is_font else 2

    quad[0].texture_index = texture_index
    quad[1].texture_index = texture_index
    quad[2].texture_index = texture_index
    quad[3].texture_index = texture_index
}

draw_text :: proc(text: string, position: Vector2, colour: Colour, pixels_per_unit: f32) {
    x: f32
    y: f32

    for c in text {
	position_offset := Vector2{x, y}

	advanced_x: f32
	advanced_y: f32

	q: stbtt.aligned_quad
	stbtt.GetBakedQuad(&alagard.characters[0], font_bitmap_w, font_bitmap_h, (cast(i32)c) - 32, &advanced_x, &advanced_y, &q, false)
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
   
	x += advanced_x / pixels_per_unit
	y += advanced_y / pixels_per_unit
	size := Vector2{ abs(q.x0 - q.x1), abs(q.y0 - q.y1) } / pixels_per_unit
    	
	bottom_left_uv := Vector2{ q.s0, q.t1 }
	top_right_uv := Vector2{ q.s1, q.t0 }
	bottom_right_uv := Vector2{q.s1, q.t1}
	top_left_uv := Vector2{q.s0, q.t0}
	
	draw_quad(
	    position + position_offset, 
	    size, 
	    0,
	    colour,
	    {top_left_uv, top_right_uv, bottom_right_uv, bottom_left_uv}, 
	    true)
    }
}

draw_entity :: proc(entity: ^Entity) {
    draw_quad(
	entity.position,
	entity.size,
	entity.rotation,
	entity.colour,
	{
	    {0, 1},
	    {1, 1},
	    {1, 0},
	    {0, 0},
	}
    )
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









