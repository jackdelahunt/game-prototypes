package src

import "core:c"
import "core:log"

import "vendor:glfw"
import gl "vendor:OpenGL"

GL_MAJOR :: 4
GL_MINOR :: 6

create_window :: proc(width: int, height: int, title: cstring) -> (glfw.WindowHandle, bool) {
    ok := glfw.Init();
    if !ok {
        return nil, false
    }

    glfw.WindowHint(glfw.CONTEXT_VERSION_MAJOR, GL_MAJOR)
    glfw.WindowHint(glfw.CONTEXT_VERSION_MINOR, GL_MINOR)
    glfw.WindowHint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)
    // glfw.WindowHint(glfw.OPENGL_FORWARD_COMPAT, gl.TRUE) macos

    window := glfw.CreateWindow(i32(width), i32(height), title, nil, nil)
    if window == nil {
        glfw.Terminate()
        return nil, false
    }

    glfw.MakeContextCurrent(window)

    glfw.SwapInterval(1)
    glfw.SetErrorCallback(error_callback)
    glfw.SetKeyCallback(window, key_callback)
    glfw.SetFramebufferSizeCallback(window, size_callback)

    return window, true
}

error_callback :: proc "c" (error: c.int, description: cstring) {
    context = custom_context()
    log.errorf("glfw window error: [%v] %v", error, description)
}

key_callback :: proc "c" (window: glfw.WindowHandle, key: c.int, scancode: c.int, action: c.int, mods: c.int) {
    switch action {
        case glfw.PRESS: fallthrough
        case glfw.REPEAT:
            state.keys[key] = .down
        case glfw.RELEASE: {
            state.keys[key] = .up
        }
    }
}

size_callback :: proc "c" (window: glfw.WindowHandle, width: c.int, height: c.int) {
    gl.Viewport(0, 0, width, height)
    state.width = int(width)
    state.height = int(height)
}


init_gl :: proc() {
    gl.load_up_to(GL_MAJOR, GL_MINOR, glfw.gl_set_proc_address)
    gl.ClearColor(0.3, 0.6, 0.9, 1)
}
