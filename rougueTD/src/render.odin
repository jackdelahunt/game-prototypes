package src

import "core:fmt"
import "core:log"
import "base:runtime"
import "core:math/linalg"

import sg "sokol/gfx"
import sapp "sokol/app"
import slog "sokol/log"
import sglue "sokol/glue"

import stbtt "vendor:stb/truetype"

import shaders "shaders"

Mat4 :: linalg.Matrix4f32
Vector2 :: [2]f32
Vector3 :: [3]f32
Vector4 :: [4]f32

normalize :: linalg.normalize0
length :: linalg.length

Vertex :: struct {
    position: Vector3,
    colour: Vector4,
    texture_uv: Vector2,
    texture_index: f32
}

Quad :: [4]Vertex

DrawType :: enum {
    SOLID,
    CIRCLE,
    TEXTURE,
    FONT,
}

DEFAULT_UVS :: [4]Vector2{
    {0, 1},
    {1, 1},
    {1, 0},
    {0, 0},
}

in_screen_space := false

draw_rectangle :: proc(position: Vector2, size: Vector2, rotation: f32, colour: Colour) {
    draw_quad(position, size, rotation, colour, DEFAULT_UVS, .SOLID)
}

draw_circle :: proc(position: Vector2, radius: f32, colour: Colour) {
    draw_quad(position, {radius * 2, radius * 2}, 0, colour, DEFAULT_UVS, .CIRCLE)
}

draw_quad :: proc(position: Vector2, size: Vector2, rotation: f32, colour: Colour, uvs: [4]Vector2, draw_type: DrawType) {
    aspect_ratio := state.screen_width / state.screen_height

    model_view_projection : Mat4

    if !in_screen_space {
	model_matrix := translate_matrix(position) * scale_matrix(size) * rotate_matrix(linalg.to_radians(rotation))
	view_matrix := view_matrix_from_position(state.camera_position) * scale_matrix(state.zoom)
	projection_matrix := linalg.matrix4_perspective_f32(90, aspect_ratio, 0, 10)
	model_view_projection = projection_matrix * view_matrix * model_matrix
    }
    else {
	model_matrix := translate_matrix(position) * scale_matrix(size) * rotate_matrix(linalg.to_radians(rotation))
	model_matrix *= scale_matrix({1, aspect_ratio})
	model_view_projection = model_matrix
    }
 

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

    texture_index: f32
    switch draw_type {
    case .SOLID:
	texture_index = 0
    case .CIRCLE:
	texture_index = 1
    case .TEXTURE:
	texture_index = 2
    case .FONT:
	texture_index = 3
    }

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
	    .FONT
	)
    }
}

renderer_init :: proc "c" () {
    context = runtime.default_context()

    sg.setup({
	environment = sglue.environment(),
	logger = { func = slog.func },
    })

    // create vertex buffer
    state.bindings.vertex_buffers[0] = sg.make_buffer({
	usage = .DYNAMIC,
	size = size_of(Quad) * len(state.quads),
	label = "quad-vertices"
    })

    // create index buffer
    index_buffer: [len(state.quads) * 6]u16
    i := 0;
    for i < len(index_buffer) {
	// vertex offset pattern to draw a quad
	// { 0, 1, 2,  0, 2, 3 }
	index_buffer[i + 0] = auto_cast ((i/6)*4 + 0)
	index_buffer[i + 1] = auto_cast ((i/6)*4 + 1)
	index_buffer[i + 2] = auto_cast ((i/6)*4 + 2)
	index_buffer[i + 3] = auto_cast ((i/6)*4 + 0)
	index_buffer[i + 4] = auto_cast ((i/6)*4 + 2)
	index_buffer[i + 5] = auto_cast ((i/6)*4 + 3)
	i += 6;
    }

    state.bindings.index_buffer = sg.make_buffer({
	type = .INDEXBUFFER,
	data = { ptr = &index_buffer, size = size_of(index_buffer) },
	label = "quad-indices"
    })

    // just loading the face texture :^] 
    // bind face texture
    state.bindings.images[shaders.IMG_default_texture] = sg.make_image({
        width = 8,
        height = 8,
        label = "default_texture",
	data = {
	    subimage = {
                0 = {
                    0 = { ptr = face_texture.data, size = auto_cast(face_texture.width * face_texture.height * 4)}, // 4 bytes per pixel
                },
            },
        },
    })

    state.bindings.images[shaders.IMG_font_texture] = sg.make_image({
        width = auto_cast font_bitmap_w,
        height = auto_cast font_bitmap_h,
	pixel_format = .R8,
        label = "font_texture",
	data = {
	    subimage = {
                0 = {
                    0 = { ptr = auto_cast &alagard.bitmap, size = size_of(alagard.bitmap)},
                },
            },
        },
    })

    state.bindings.samplers[shaders.SMP_default_sampler] = sg.make_sampler({
	label = "default_sampler"
    })

    shader := sg.make_shader(shaders.basic_shader_desc(sg.query_backend()))

    state.render_pipeline = sg.make_pipeline({
	shader = shader,
	index_type = .UINT16,
	layout = {
	    attrs = {
		shaders.ATTR_basic_position = { format = .FLOAT3 },
		shaders.ATTR_basic_color0 = { format = .FLOAT4 },
		shaders.ATTR_basic_texture_uv0 = { format = .FLOAT2 },
		shaders.ATTR_basic_texture_index0 = { format = .FLOAT },
	    },
	},
	colors = {
	    0 = {
		blend = {
		    enabled = true,
		    src_factor_rgb = .SRC_ALPHA,
		    dst_factor_rgb = .ONE_MINUS_SRC_ALPHA,
		    op_rgb = .ADD,
		    src_factor_alpha = .ONE,
		    dst_factor_alpha = .ONE_MINUS_SRC_ALPHA,
		    op_alpha = .ADD,
		}
	    }
	},
	label = "basic-pipeline"
    })


    state.pass_action = {
        colors = {
	    0 = { load_action = .CLEAR, clear_value = {SKY_BLUE.r,  SKY_BLUE.g, SKY_BLUE.b, 1} },
        }
    }
}

renderer_frame :: proc "c" () {
    context = runtime.default_context()

    state.screen_width = sapp.widthf()
    state.screen_height = sapp.heightf()
  
    // reset quad data for this frame
    runtime.mem_zero(&state.quads, size_of(Quad) * len(state.quads))
    state.quad_count = 0

    // let the game create and set any quads it wants for this frame
    frame()

    if state.quad_count == 0 {
	return
    }

    // update vertex buffer with new quad data for this frame
    sg.update_buffer(
	state.bindings.vertex_buffers[0],
	{ ptr = &state.quads[0], size = size_of(Quad) * state.quad_count }
    )

    sg.begin_pass({action = state.pass_action, swapchain = sglue.swapchain()})

    sg.apply_pipeline(state.render_pipeline)
    sg.apply_bindings(state.bindings)

    sg.draw(0, 6 * state.quad_count, 1)
    
    sg.end_pass()
    sg.commit()

    in_screen_space = false
}

renderer_cleanup :: proc "c" () {
    sg.shutdown()
}

range :: proc(buffer: []$T) -> sg.Range {
    return sg.Range{
	ptr = &buffer[0],
	size = len(buffer) * size_of(T)
    }
}

translate_matrix :: proc(position: Vector2) -> Mat4 {
    return linalg.matrix4_translate_f32({position.x, position.y, 0})
}

scale_matrix :: proc(scale: Vector2) -> Mat4 {
	return linalg.matrix4_scale_f32(Vector3{scale.x, scale.y, 1});
}

rotate_matrix :: proc(radians: f32) -> Mat4 {
	return linalg.matrix4_rotate_f32(radians, Vector3{0, 0, 1});
}

view_matrix_from_position :: proc(position: Vector2) -> Mat4 {
    return linalg.matrix4_look_at_f32({position.x, position.y, 1}, {position.x, position.y, 0}, {0, 1, 0})
}
