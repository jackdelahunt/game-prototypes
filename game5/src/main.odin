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

// @state
State :: struct {
    width: int,
    height: int,
    window: glfw.WindowHandle,
    keys: [348]InputState,
}

state: State

InputState :: enum {
    up,
    down
}

main :: proc() {
    context = custom_context()

    state = {
        width = 720,
        height = 480
    }

    { // create window and init open gl
        window, ok := create_window(state.width, state.height, "game5")
        if !ok {
            log.fatal("error trying to init window")
            return
        }
    
        state.window = window
    
        init_gl()
    }

    { // setup renderer
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
        }

        { // vertex array
            vertex_array: u32
            gl.GenVertexArrays(1, &vertex_array)
            gl.BindVertexArray(vertex_array)
        }

        { // vertex buffer
            vertices := [?]f32 {
                0.5,  0.5, 0.0,     // top right
                0.5, -0.5, 0.0,     // bottom right
                -0.5, -0.5, 0.0,    // bottom left
                -0.5,  0.5, 0.0     // top left 
            }
    
            vertex_buffer: u32
            gl.GenBuffers(1, &vertex_buffer)
            
            gl.BindBuffer(gl.ARRAY_BUFFER, vertex_buffer)
            gl.BufferData(gl.ARRAY_BUFFER, size_of(f32) * len(vertices), &vertices, gl.STATIC_DRAW)
        }

        { // index buffer
            indices := [?]u32 {
                0, 1, 3,
                1, 2, 3
            }

            index_buffer: u32
            gl.GenBuffers(1, &index_buffer)

            gl.BindBuffer(gl.ELEMENT_ARRAY_BUFFER, index_buffer)
            gl.BufferData(gl.ELEMENT_ARRAY_BUFFER, size_of(u32) * len(indices), &indices, gl.STATIC_DRAW)
        }

        { // attributes
            gl.VertexAttribPointer(0, 3, gl.FLOAT, gl.FALSE, 3 * size_of(f32), 0)
            gl.EnableVertexAttribArray(0)
        }
    }

    for !glfw.WindowShouldClose(state.window) {
        if state.keys[glfw.KEY_ESCAPE] == .down {
            glfw.SetWindowShouldClose(state.window, true)
        }

        gl.Clear(gl.COLOR_BUFFER_BIT)

        gl.DrawElements(gl.TRIANGLES, 6, gl.UNSIGNED_INT, nil)

        glfw.SwapBuffers(state.window)
        glfw.PollEvents()
    }

    glfw.DestroyWindow(state.window)
    glfw.Terminate()
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
