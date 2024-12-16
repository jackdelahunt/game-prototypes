package entry

import "core:fmt"
import "core:log"
import "base:runtime"
import "core:c"

import sg "sokol/gfx"
import sapp "sokol/app"
import slog "sokol/log"
import sglue "sokol/glue"

SCREEN_WIDTH :: 640
SCREEN_HEIGHT :: 400

pass_action: sg.Pass_Action

red: f32 = 0
green: f32 = 0
blue: f32 = 0

main :: proc() {
    sapp.run({
	init_cb = init,
	frame_cb = frame,
	cleanup_cb = cleanup,
	width = SCREEN_WIDTH,
	height = SCREEN_HEIGHT,
	window_title = "sokol window",
	icon = { sokol_default = true },
	logger = { func = slog.func },
    })
}


init :: proc "c" () {
    context = runtime.default_context()
    sg.setup({
	environment = sglue.environment(),
	logger = { func = slog.func },
    })

    pass_action = {
        colors = {
	    { load_action = sg.Load_Action.CLEAR, clear_value = { red, green, blue, 1 } },
	    {},
	    {},	    
	    {}
        }
    }
}

frame :: proc "c" () {
    red += 0.01
    green += 0.02
    blue += 0.03

    if red > 1 {
	red = 0
    }

    if green > 1 {
	green = 0
    }

    if blue > 1 {
	blue = 0
    }

    pass_action.colors[0].clear_value = {red, green, blue, 1}

    sg.begin_pass({action = pass_action, swapchain = sglue.swapchain()})
    sg.end_pass()
    sg.commit()
}

cleanup :: proc "c" () {
    sg.shutdown()
}












