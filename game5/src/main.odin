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

// TODO: render tech todo
// - screen independent screen space rendering
// - animated textures ?? maybe

// indev settings
LOG_COLOURS         :: false
OPENGL_MESSAGES     :: false
WRITE_DEBUG_IMAGES  :: true
V_SYNC              :: true

// internal settings
MAX_ENTITIES :: 128

// gameplay settings
PLAYER_SPEED            :: 400
PLAYER_SHOOT_COOLDOWN   :: 0.05
BULLET_SPEED            :: 800
AI_SPEED                :: 200

// player settings
GAMEPAD_STICK_DEADZONE      :: 0.15
GAMEPAD_TRIGGER_DEADZONE    :: 0.2

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
    down
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
            orthographic_size = 500,
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
    texture: TextureHandle,

    // player
    shooting_cooldown: f32,
    aim_direction: v2
}

EntityFlag :: enum {
    player,
    ai,
    projectile,
    to_be_deleted
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
        flags = {.player},
        position = position,
        size = {50, 50},
        texture = .face,
        aim_direction = {0, 1}
    })
}

create_ai :: proc(position: v2) -> ^Entity {
    return create_entity({
        flags = {.ai},
        position = position,
        size = {30, 30},
        texture = .sad_face,
    })
}

create_bullet :: proc(position: v2, velocity: v2) -> ^Entity {
    return create_entity({
        flags = {.projectile},
        position = position,
        velocity = velocity,
        size = {5, 5},
        texture = .face
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

start :: proc() {
    create_player({0, 0})
    create_ai({-100, 150})
}

input :: proc() {
    glfw.PollEvents()

    // TODO: handle disconnect ??
    if glfw.GetGamepadState(glfw.JOYSTICK_1, &state.gamepad) == 0 {
        log.error("no gamepad detected")
    }
}

update :: proc(delta_time: f32) {
    for &entity in state.entities[0:state.entity_count] {
        { // reduce cooldowns
            entity.shooting_cooldown -= delta_time
            if entity.shooting_cooldown < 0 {
                entity.shooting_cooldown = 0
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

                if entity.shooting_cooldown != 0 {
                    break shooting
                }
                   
                entity.shooting_cooldown = PLAYER_SHOOT_COOLDOWN
                create_bullet(entity.position, entity.aim_direction * BULLET_SPEED)
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
           
            direction := linalg.normalize(player.position - entity.position)
            entity.velocity = direction * AI_SPEED    
        }



        projectile_update: {
            if !(.projectile in entity.flags) {
                break projectile_update
            }

            if state.time - entity.created_time > 3 {
                entity.flags += {.to_be_deleted} 
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
        entity.position += entity.velocity * delta_time
    }
}

draw :: proc(delta_time: f32) {
    for &entity in state.entities[0:state.entity_count] {
        draw_texture(entity.texture, entity.position, entity.size, WHITE)
    }

    in_screen_space = true

    { // entity count
        text := fmt.tprintf("E: %v", state.entity_count)
        draw_text(text, {10, 10}, 30, BLACK, .bottom_left)
    }

    { // fps
        fps := math.trunc(1 / delta_time)
        text := fmt.tprintf("%v", fps)
        draw_text(text, {10, state.height - 35}, 30, BLACK, .bottom_left)
    }
}

// -------------------------- @renderer -----------------------
v2 :: [2]f32
v3 :: [3]f32
v4 :: [4]f32
Mat4 :: linalg.Matrix4f32

GL_MAJOR :: 4
GL_MINOR :: 6
MAX_QUADS :: 1024

Vertex :: struct {
    position: v3,
    colour: v4,
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
    face,
    sad_face,
    blue_face,
    wide_face
}

Texture :: struct {
    width: i32,
    height: i32,
    uv: [4]v2,
    data: [^]byte
}

Atlas :: struct {
    width: i32,
    height: i32,
    data: [^]byte
}

FontHandle :: enum {
    alagard,
    baskerville
}

Font :: struct {
    characters: []stbtt.bakedchar,
    bitmap: []byte,
    bitmap_width: int,
    bitmap_height: int,
    filter: FontFilter,
}

FontFilter :: enum {
    nearest,
    linear
}

TextAllignment :: enum {
    center,
    bottom_left
}

RED     :: v4{1, 0, 0, 1}
GREEN   :: v4{0, 1, 0, 1}
BLUE    :: v4{0, 0, 1, 1}
WHITE   :: v4{1, 1, 1, 1}
BLACK   :: v4{0, 0, 0, 1}

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
    
        V :: 0.6
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
        gl.VertexAttribPointer(0, 3, gl.FLOAT, gl.FALSE, size_of(Vertex), 0)                                    // position
        gl.VertexAttribPointer(1, 4, gl.FLOAT, gl.FALSE, size_of(Vertex), 3 * size_of(f32))                     // colour
        gl.VertexAttribPointer(2, 2, gl.FLOAT, gl.FALSE, size_of(Vertex), (3 + 4) * size_of(f32))               // uv
        gl.VertexAttribIPointer(3, 1, gl.INT, size_of(Vertex), (3 + 4 + 2) * size_of(f32))                      // draw type

        gl.EnableVertexAttribArray(0)
        gl.EnableVertexAttribArray(1)
        gl.EnableVertexAttribArray(2)
        gl.EnableVertexAttribArray(3)
    }
    
    { // textures
        { // upload atlas texture
            atlas_texture: u32
            gl.GenTextures(1, &atlas_texture)
    
            gl.BindTexture(gl.TEXTURE_2D, atlas_texture)
    
            gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT) // s is x wrap
            gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.REPEAT) // t is y wrap
            gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST)
            gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST)
    
            atlas := &renderer.texture_atlas
    
            gl.TexImage2D(gl.TEXTURE_2D, 0, gl.RGBA, atlas.width, atlas.height, 0, gl.RGBA, gl.UNSIGNED_BYTE, atlas.data)
    
            renderer.atlas_texture_id = atlas_texture
        }

        { // upload font txture
            font_texture: u32
            gl.GenTextures(1, &font_texture)
    
            gl.BindTexture(gl.TEXTURE_2D, font_texture)
    
            gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT) // s is x wrap
            gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.REPEAT) // t is y wrap

            filter: i32

            switch renderer.font.filter {
                case .nearest:
                    filter = gl.NEAREST
                case .linear:
                    filter = gl.LINEAR
            }

            gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, filter)
            gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, filter)
    
            font := &renderer.font
   
            // font data is monochrome with a single channel so need to load as just red and store it as that - 19/01/25
            gl.TexImage2D(gl.TEXTURE_2D, 0, gl.RED, i32(font.bitmap_width), i32(font.bitmap_height), 0, gl.RED, gl.UNSIGNED_BYTE, slice.as_ptr(font.bitmap))
    
            renderer.font_texture_id = font_texture
        }
    }

    return true
}

draw_rectangle :: proc(position: v2, size: v2, colour: v4) {
    draw_quad(position, size, colour, DEFAULT_UV, .rectangle)
}

draw_texture :: proc(texture: TextureHandle, position: v2, size: v2, colour: v4) {
    draw_quad(position, size, colour, state.renderer.textures[texture].uv, .texture)
}

draw_circle :: proc(position: v2, radius: f32, colour: v4) {
    draw_quad(position, {radius * 2, radius * 2}, colour, DEFAULT_UV, .circle)
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
        draw_quad(translated_position + (scaled_size * 0.5), scaled_size, colour, glyph.uvs, .font);
    } 
}

draw_quad :: proc(position: v2, size: v2, colour: v4, uv: [4]v2, draw_type: DrawType) {
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
            width = width,
            height = height,
            data = data
        }
    }

    return true
}

build_texture_atlas :: proc(renderer: ^Renderer) -> bool {
    ATLAS_WIDTH     :: 32
    ATLAS_HEIGHT    :: 32
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
        case .face:
            return "face.png"
        case .sad_face:
            return "sad_face.png"
        case .blue_face:
            return "blue_face.png"
        case .wide_face:
            return "wide_face.png"
    }

    unreachable()
}

load_font :: proc(renderer: ^Renderer, font: FontHandle, bitmap_width: int, bitmap_height: int, font_height: f32, filter: FontFilter) -> bool {
    RESOURCE_DIR :: "resources/fonts/"
    CHAR_COUNT     :: 96

    font_info := Font {
        characters = make([]stbtt.bakedchar, CHAR_COUNT),
        bitmap = make([]byte, bitmap_width * bitmap_height),
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
        slice.as_ptr(font_info.bitmap),
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

        log.infof("wrote font image to \"%v\" [%v x %v %v bytes uncompressed]", output_path, bitmap_width, bitmap_height, len(font_info.bitmap))
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

alpha :: proc(colour: v4, alpha: f32) -> v4 {
    return {colour.r, colour.g, colour.b, alpha}
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
    switch action {
        case glfw.PRESS: fallthrough
        case glfw.REPEAT:
            state.keys[key] = .down
        case glfw.RELEASE: {
            state.keys[key] = .up
        }
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
