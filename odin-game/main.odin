package entry

import "core:fmt"
import "core:dynlib"
import "core:c/libc"
import "core:log"

running := false

Game :: struct {
    // callbacks that the game dll can use to commnicate to here
    // callback orders:
    // alloc_state
    // terminate
    init_callbacks: proc "c" (proc "c" (uint) -> rawptr, proc "c" ()),
    run_game: proc "c" (),
    __handle: dynlib.Library,
}

alloc_state :: proc "c" (size: uint) -> rawptr {
    ptr := libc.malloc(size)
    if(ptr == nil) {
        libc.printf("could not allocate game state... exiting")
        libc.exit(-1)
    }

    return libc.memset(ptr, 0, size)
}

terminate :: proc "c" () {
    running = false
}

load_game :: proc() -> (Game, bool) {
    game: Game
    path :: "game.dll"

    count, ok := dynlib.initialize_symbols(&game, path)
    if !ok {
        error := dynlib.last_error()
        fmt.printf("%v", error)
        return game, false
    }

    fmt.printf("(Initial DLL Load) %v symbols loaded from " + path + " (%p).\n", count, game.__handle)

    return game, ok 
}

main :: proc() {
    game, ok := load_game()
    if !ok {
        fmt.println("failed to load library.. quiting")
        return
    }

    defer dynlib.unload_library(game.__handle)

    game.init_callbacks(alloc_state, terminate)
    
    game.run_game()
}
















