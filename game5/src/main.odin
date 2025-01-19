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

import "vendor:glfw"
import gl "vendor:OpenGL"
import stbi "vendor:stb/image"
import stbtt "vendor:stb/truetype"
import stbrp "vendor:stb/rect_pack"

// TODO: render tech todo
// - port same text rendering method
// - better font and fix issues
// - port screen space rendering
// - screen independent screen space rendering
// - animated textures ?? maybe

// dev settings
LOG_COLOURS         :: false
OPENGL_MESSAGES     :: false
WRITE_DEBUG_IMAGES  :: true

// -------------------------- @game ---------------------------
state: State

State :: struct {
    width: int,
    height: int,
    window: glfw.WindowHandle,
    keys: [348]InputState,
    texture_atlas: Atlas,
    textures: [Texture]TextureInfo,
    font: Font,
    camera: struct {
        position: v2,
        // length in world units from camera centre to top edge of camera view
        // length of camera centre to side edge is this * aspect ratio
        orthographic_size: f32,
        near_plane: f32,
        far_plane: f32
    },
    renderer: struct {
        quads: []Quad,
        quad_count: int,
       
        vertex_array_id: u32,
        vertex_buffer_id: u32,
        index_buffer_id: u32,
        shader_program_id: u32,
        atlas_texture_id: u32,
        font_texture_id: u32,
    }
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
            orthographic_size = 100,
            near_plane = 0.01,
            far_plane = 100
        },
        renderer = {
            quads = make([]Quad, MAX_QUADS)
        },
    }

    { // initialise everything
        ok: bool

        ok = load_textures()
        if !ok {
            log.fatal("error when loading textures")
            return
        }
    
        ok = build_texture_atlas()
        if !ok {
            log.fatal("error when building texture atlas")
            return
        }

        ok = load_fonts()
        if !ok {
            log.fatal("error when loading fonts")
            return
        }

        state.window, ok = create_window(state.width, state.height, "game5")
        if !ok {
            log.fatal("error trying to init window")
            return
        }
     
        ok = init_renderer()
        if !ok {
            log.fatal("error when initialising the renderer")
            return
        }
    }

    for !glfw.WindowShouldClose(state.window) {
        if state.keys[glfw.KEY_ESCAPE] == .down {
            glfw.SetWindowShouldClose(state.window, true)
        }

        update()
        draw()

        gl.Clear(gl.COLOR_BUFFER_BIT)

        // update vertex buffer with current quad data
        gl.BindBuffer(gl.ARRAY_BUFFER, state.renderer.vertex_buffer_id)
        gl.BufferSubData(gl.ARRAY_BUFFER, 0, size_of(Quad) * state.renderer.quad_count, &state.renderer.quads[0])

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
        glfw.PollEvents()
    }

    glfw.DestroyWindow(state.window)
    glfw.Terminate()
}

update :: proc() {
    SPEED :: 1

    if state.keys[glfw.KEY_A] == .down {
        state.camera.position.x -= SPEED
    }

    if state.keys[glfw.KEY_D] == .down {
        state.camera.position.x += SPEED
    }

    if state.keys[glfw.KEY_W] == .down {
        state.camera.position.y += SPEED
    }

    if state.keys[glfw.KEY_S] == .down {
        state.camera.position.y -= SPEED
    }
}

draw :: proc() {
    // draw_texture(.face, {0, 0}, {50, 50}, WHITE)
    // draw_texture(.sad_face, {50, 50}, {50, 50}, WHITE)
    // draw_texture(.blue_face, {-50, -50}, {50, 50}, WHITE)
    // draw_texture(.wide_face, {-25, 50}, {100, 50}, WHITE)

    draw_rectangle({0, 0}, {110, 20}, BLACK)
    draw_text("jj aa jj aa", {0, 0}, 20, WHITE)
}

// -------------------------- @renderer -----------------------
v2 :: [2]f32
v3 :: [3]f32
v4 :: [4]f32
Mat4 :: linalg.Matrix4f32

GL_MAJOR :: 4
GL_MINOR :: 6
MAX_QUADS :: 32

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

Texture :: enum {
    face,
    sad_face,
    blue_face,
    wide_face
}

TextureInfo :: struct {
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


FONT_BITMAP_WIDTH   :: 256 
FONT_BITMAP_HEIGHT  :: 256
FONT_HEIGHT         :: 15
FONT_CHAR_COUNT     :: 96

Font :: struct {
    characters: [FONT_CHAR_COUNT]stbtt.bakedchar,
    bitmap: [FONT_BITMAP_WIDTH * FONT_BITMAP_HEIGHT]byte
}

RED     :: v4{1, 0, 0, 1}
GREEN   :: v4{0, 1, 0, 1}
BLUE    :: v4{0, 0, 1, 1}
WHITE   :: v4{1, 1, 1, 1}
BLACK   :: v4{0, 0, 0, 1}

init_renderer :: proc() -> bool {
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

        state.renderer.shader_program_id = shader_program
    }

    { // vertex array
        vertex_array: u32
        gl.GenVertexArrays(1, &vertex_array)
        gl.BindVertexArray(vertex_array)

        state.renderer.vertex_array_id = vertex_array
    }

    { // vertex buffer
        vertex_buffer: u32
        gl.GenBuffers(1, &vertex_buffer)
            
        gl.BindBuffer(gl.ARRAY_BUFFER, vertex_buffer)
        gl.BufferData(gl.ARRAY_BUFFER, size_of(Quad) * len(state.renderer.quads), &state.renderer.quads[0], gl.DYNAMIC_DRAW)

        state.renderer.vertex_buffer_id = vertex_buffer
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

        state.renderer.index_buffer_id = index_buffer
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
    
            atlas := &state.texture_atlas
    
            gl.TexImage2D(gl.TEXTURE_2D, 0, gl.RGBA, atlas.width, atlas.height, 0, gl.RGBA, gl.UNSIGNED_BYTE, atlas.data)
    
            state.renderer.atlas_texture_id = atlas_texture
        }

        { // upload font txture
            font_texture: u32
            gl.GenTextures(1, &font_texture)
    
            gl.BindTexture(gl.TEXTURE_2D, font_texture)
    
            gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT) // s is x wrap
            gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.REPEAT) // t is y wrap
            gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST)
            gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST)
    
            font := &state.font
   
            // font data is monochrome with a single channel so need to load as just red and store it as that - 19/01/25
            gl.TexImage2D(gl.TEXTURE_2D, 0, gl.RED, FONT_BITMAP_WIDTH, FONT_BITMAP_HEIGHT, 0, gl.RED, gl.UNSIGNED_BYTE, &font.bitmap)
    
            state.renderer.font_texture_id = font_texture
        }
    }

    return true
}

draw_rectangle :: proc(position: v2, size: v2, colour: v4) {
    draw_quad(position, size, colour, DEFAULT_UV, .rectangle)
}

draw_texture :: proc(texture: Texture, position: v2, size: v2, colour: v4) {
    draw_quad(position, size, colour, state.textures[texture].uv, .texture)
}

draw_circle :: proc(position: v2, radius: f32, colour: v4) {
    draw_quad(position, {radius * 2, radius * 2}, colour, DEFAULT_UV, .circle)
}

draw_text :: proc(text: string, position: v2, font_size: f32, colour: v4) {
    if len(text) == 0 {
        return
    }

    GlyphRenderInfo :: struct {
        relative_x: f32,
        relative_y: f32,
        width: f32,
        height: f32,
        uvs: [4]v2,
    }

    glyphs := make([]GlyphRenderInfo, len(text), context.temp_allocator)

    total_x:    f32
    total_y:    f32
    max_height: f32

    for i in 0..<len(text) {
        c := text[i]

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
        stbtt.GetBakedQuad(&state.font.characters[0], FONT_BITMAP_WIDTH, FONT_BITMAP_HEIGHT, (cast(i32)c) - 32, &advanced_x, &advanced_y, &alligned_quad, false)

        width := alligned_quad.x1 - alligned_quad.x0
        height := alligned_quad.y1 - alligned_quad.y0

        if height > max_height {
            max_height = height
        }

        top_left_uv     := v2{alligned_quad.s0, alligned_quad.t0}
        top_right_uv    := v2{alligned_quad.s1, alligned_quad.t0}
        bottom_right_uv := v2{alligned_quad.s1, alligned_quad.t1}
        bottom_left_uv  := v2{alligned_quad.s0, alligned_quad.t1}

        glyphs[i] = {
            relative_x = total_x,
            relative_y = total_y,
            width = width,
            height = height,
            uvs = {
                top_left_uv,
                top_right_uv,
                bottom_right_uv,
                bottom_left_uv
            }
        }
            
        total_x += advanced_x
        total_y += advanced_y
    }

    
    scale_factor := 1.0 / max_height
    height_scale_factor := scale_factor * font_size // font size is in world units
    total_scaled_width := total_x * scale_factor * font_size

    for &info in glyphs {
        width_proportion := info.width / total_x
        x_proportion := info.relative_x / total_x

        width := width_proportion * total_scaled_width
        x := x_proportion * total_scaled_width

        // TODO: right now the text is just centred, could add an origin
        // vec2 which dictates where the centre of the text should be
        size := v2{width, info.height * height_scale_factor}
        offset_position := v2{x - (total_scaled_width * 0.5), info.relative_y}

        draw_quad(position + offset_position, size, colour, info.uvs, .font);
    }
}

draw_quad :: proc(position: v2, size: v2, colour: v4, uv: [4]v2, draw_type: DrawType) {
    transformation_matrix: Mat4

    // model matrix
    transformation_matrix = linalg.matrix4_translate(v3{position.x, position.y, 10}) * linalg.matrix4_scale(v3{size.x, size.y, 1})

    // model view matrix
    transformation_matrix = get_view_matrix() * transformation_matrix

    // model view projection
    transformation_matrix = get_projection_matrix() * transformation_matrix

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

load_textures :: proc() -> bool {
    RESOURCE_DIR :: "resources/textures/"
    DESIRED_CHANNELS :: 4

    for texture in Texture {
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

        state.textures[texture] = {
            width = width,
            height = height,
            data = data
        }
    }

    return true
}

build_texture_atlas :: proc() -> bool {
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
        RECT_COUNT :: len(Texture)
        
        rp_context: stbrp.Context
        nodes:      [ATLAS_WIDTH]stbrp.Node
        rects:      [RECT_COUNT]stbrp.Rect

        stbrp.init_target(&rp_context, ATLAS_HEIGHT, ATLAS_HEIGHT, &nodes[0], ATLAS_WIDTH)

        for texture, i in Texture {
            info := &state.textures[texture]

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
            texture_info := &state.textures[Texture(rect.id)]

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

    state.texture_atlas = Atlas {
        width = ATLAS_WIDTH,
        height = ATLAS_HEIGHT,
        data = atlas_data
    }

    log.infof("built texture atlas [%v x %v %v bytes uncompressed]", ATLAS_WIDTH, ATLAS_HEIGHT, ATLAS_BYTE_SIZE)

    return true
}

get_texture_name :: proc(texture: Texture) -> string {
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

load_fonts :: proc() -> bool {
    RESOURCE_DIR :: "resources/fonts/"

    path := fmt.tprint(RESOURCE_DIR, "alagard.ttf", sep="")

    font_data, ok := os.read_entire_file(path)
    if !ok {
        log.errorf("error loading font file \"%v\"", path)
        return false
    }

    bake_result := stbtt.BakeFontBitmap(
        raw_data(font_data), 
        0, 
        FONT_HEIGHT, 
        auto_cast &state.font.bitmap,
        FONT_BITMAP_WIDTH, 
        FONT_BITMAP_HEIGHT, 
        32, 
        FONT_CHAR_COUNT, 
        auto_cast &state.font.characters
    )

    if !(bake_result > 0) {
        log.errorf("not enough space in bitmap buffer for font %v", path)
        return false
    }

    when WRITE_DEBUG_IMAGES {
        output_path :: "build/alagard.png"

        stbi.flip_vertically_on_write(false)

        write_result := stbi.write_png(output_path, FONT_BITMAP_WIDTH, FONT_BITMAP_HEIGHT, 1, &state.font.bitmap[0], FONT_BITMAP_WIDTH)	
        if write_result == 0 {
            log.error("could not write font \"%v\" to output image \"%v\"", path, output_path)
            return false
        }

        log.infof("wrote font image to \"%v\" [%v x %v]", output_path, FONT_BITMAP_WIDTH, FONT_BITMAP_HEIGHT)
    }

    log.infof("loaded font \"%v\" [%v x %v %v bytes uncompressed]", path, FONT_BITMAP_WIDTH, FONT_BITMAP_HEIGHT, len(font_data))
   
    return true
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
create_window :: proc(width: int, height: int, title: cstring) -> (glfw.WindowHandle, bool) {
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

    glfw.SwapInterval(1)
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
    state.width = int(width)
    state.height = int(height)
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
