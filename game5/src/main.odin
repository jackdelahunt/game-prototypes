package src

import "core:fmt"
import "base:runtime"
import "core:encoding/ansi"
import "core:log"
import "core:path/filepath"
import "core:strings"

import "vendor:glfw"
import gl "vendor:OpenGL"

vertex_shader_source := #load("./shaders/vertex.shader", cstring)
fragment_shader_source := #load("./shaders/fragment.shader", cstring)

MAX_QUADS :: 32

// @state
State :: struct {
    width: int,
    height: int,
    window: glfw.WindowHandle,
    keys: [348]InputState,
    renderer: struct {
        quads: []Quad,
        quad_count: int,
       
        vertex_array_id: u32,
        vertex_buffer_id: u32,
        index_buffer_id: u32,
        shader_program_id: u32
    }
}

state: State

InputState :: enum {
    up,
    down
}

v3 :: [3]f32
v4 :: [4]f32

Vertex :: struct {
    position: v3,
    colour: v4
}

Quad :: struct {
    vertices: [4]Vertex
}

RED     :: v4{1, 0, 0, 1}
GREEN   :: v4{0, 1, 0, 1}
BLUE    :: v4{0, 0, 1, 1}
WHITE   :: v4{1, 1, 1, 1}

main :: proc() {
    context = custom_context()

    state = {
        width = 720,
        height = 480,
        renderer = {
            quads = make([]Quad, MAX_QUADS)
        }
    }

    { // create window
        window, ok := create_window(state.width, state.height, "game5")
        if !ok {
            log.fatal("error trying to init window")
            return
        }
    
        state.window = window
    }

    init_gl()
    init_renderer()

    for !glfw.WindowShouldClose(state.window) {
        if state.keys[glfw.KEY_ESCAPE] == .down {
            glfw.SetWindowShouldClose(state.window, true)
        }

        gl.Clear(gl.COLOR_BUFFER_BIT)

        gl.UseProgram(state.renderer.shader_program_id)
        gl.BindVertexArray(state.renderer.vertex_array_id)
        gl.DrawElements(gl.TRIANGLES, 6, gl.UNSIGNED_INT, nil)

        glfw.SwapBuffers(state.window)
        glfw.PollEvents()
    }

    glfw.DestroyWindow(state.window)
    glfw.Terminate()
}

init_renderer :: proc() {
    { // shaders 
        BUFFER_SIZE :: 512
        compile_status: i32
        error_buffer: [BUFFER_SIZE]u8

        vertex_shader := gl.CreateShader(gl.VERTEX_SHADER)
        defer gl.DeleteShader(vertex_shader)

        gl.ShaderSource(vertex_shader, 1, &vertex_shader_source, nil)
        gl.CompileShader(vertex_shader)

        gl.GetShaderiv(vertex_shader, gl.COMPILE_STATUS, &compile_status)
        if compile_status == 0 {
            gl.GetShaderInfoLog(vertex_shader, BUFFER_SIZE, nil, &error_buffer[0])
            log.fatalf("failed to compile vertex shader: %v", strings.string_from_ptr(&error_buffer[0], 512))
        }

        fragment_shader := gl.CreateShader(gl.FRAGMENT_SHADER)
        defer gl.DeleteShader(fragment_shader)

        gl.ShaderSource(fragment_shader, 1, &fragment_shader_source, nil)
        gl.CompileShader(fragment_shader)

        gl.GetShaderiv(fragment_shader, gl.COMPILE_STATUS, &compile_status)
        if compile_status == 0 {
            gl.GetShaderInfoLog(fragment_shader, BUFFER_SIZE, nil, &error_buffer[0])
            log.fatalf("failed to compile fragment shader: %v", strings.string_from_ptr(&error_buffer[0], 512))
        }

        shader_program := gl.CreateProgram()
        gl.AttachShader(shader_program, vertex_shader)
        gl.AttachShader(shader_program, fragment_shader)
        gl.LinkProgram(shader_program)
            
        gl.GetProgramiv(shader_program, gl.LINK_STATUS, &compile_status);
        if compile_status == 0 {
            gl.GetProgramInfoLog(shader_program, BUFFER_SIZE, nil, &error_buffer[0]);
        }

        gl.UseProgram(shader_program)

        state.renderer.shader_program_id = shader_program
    }

    { // vertex array
        vertex_array: u32
        gl.GenVertexArrays(1, &vertex_array)
        gl.BindVertexArray(vertex_array)

        state.renderer.vertex_array_id = vertex_array
    }

    { // vertex buffer
        quad := &state.renderer.quads[state.renderer.quad_count]
        state.renderer.quad_count += 1

        quad.vertices[0] = { position = {-0.5, 0.5, 0.0},    colour = RED}      // top left
        quad.vertices[1] = { position = {0.5, 0.5, 0.0},   colour = GREEN}      // top right
        quad.vertices[2] = { position = {0.5, -0.5, 0.0},  colour = BLUE}       // bottom right
        quad.vertices[3] = { position = {-0.5, -0.5, 0.0},   colour = WHITE}    // bottom right
   
        vertex_buffer: u32
        gl.GenBuffers(1, &vertex_buffer)
            
        gl.BindBuffer(gl.ARRAY_BUFFER, vertex_buffer)
        gl.BufferData(gl.ARRAY_BUFFER, size_of(Quad) * len(state.renderer.quads), &state.renderer.quads[0], gl.STATIC_DRAW)

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
        gl.VertexAttribPointer(0, 3, gl.FLOAT, gl.FALSE, size_of(Vertex), 0) // position
        gl.VertexAttribPointer(1, 4, gl.FLOAT, gl.FALSE, size_of(Vertex), 3 * size_of(f32)) // colour

        gl.EnableVertexAttribArray(0)
        gl.EnableVertexAttribArray(1)
    }
}

custom_context :: proc() -> runtime.Context {
    c := runtime.default_context()

    c.logger = {
        procedure = log_callback,
        lowest_level = .Debug when ODIN_DEBUG else .Warning
    }

    return c
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
