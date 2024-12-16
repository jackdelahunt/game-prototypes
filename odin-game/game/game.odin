package game

import "core:fmt"
import "core:log"
import "base:runtime"
import "vendor:glfw"

import sapp "sokol/app"
import sg "sokol/gfx"
import slog "sokol/log"
import sglue "sokol/glue"

SCREEN_WIDTH :: 640
SCREEN_HEIGHT :: 400

// callbacks to main file
alloc_state: proc "c" (uint) -> rawptr = nil
terminate: proc "c" () = nil

//////////////// @state /////////////////
GameState :: struct {
    window: glfw.WindowHandle
}

gs: ^GameState = nil

@(export)
init_callbacks :: proc "c" (alloc_state_callback: proc "c" (uint) -> rawptr, terminate_callback: proc "c" ()) {
    alloc_state = alloc_state_callback
    terminate = terminate_callback
}

@(export)
run_game :: proc "c" () {
    run()
}

run :: proc "contextless" () {
    ptr := alloc_state(size_of(GameState))
    gs = cast(^GameState)ptr

    context = runtime.default_context()

    // get glfw set up
    if !glfw.Init() {
	log.error("unable to init glfw")
	return
    }

    gs.window = glfw.CreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Window", nil, nil)
    if gs.window == nil {
	log.error("unable to create glfw window")
	glfw.Terminate()
	return
    }

    // get sokol setup
    sg.setup({
	environment = sglue.environment(),
	logger = { func = slog.func },
    })

    for	!glfw.WindowShouldClose(gs.window) {
	glfw.SwapBuffers(gs.window)
	glfw.PollEvents()
    }

    glfw.Terminate()
}
