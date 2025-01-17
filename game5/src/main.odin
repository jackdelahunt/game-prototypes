package src

import "core:fmt"
import "base:runtime"
import "core:encoding/ansi"
import "core:log"
import "core:path/filepath"

import "vendor:glfw"
import gl "vendor:OpenGL"

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

	for !glfw.WindowShouldClose(state.window) {
		if state.keys[glfw.KEY_ESCAPE] == .down {
			glfw.SetWindowShouldClose(state.window, true)
		}

		gl.Clear(gl.COLOR_BUFFER_BIT)

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
