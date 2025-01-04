package src

import "core:fmt"
import "core:log"
import "base:runtime"

import sapp "sokol/app"

create_player :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.PLAYER},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.7,
        colour = brightness(RED, 0.75),
        grid_position = grid_position
    })
}

create_rock :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.PUSHABLE},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.85,
        colour = BROWN,
        grid_position = grid_position,
        shape = .CIRCLE,
    })  
}

create_wall :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.NONE},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE},
        colour = BLACK,
        grid_position = grid_position
    })  
}

move_entity :: proc(entity: ^Entity, direction: Direction) -> bool {
    new_position := entity.grid_position + direction_grid_offset(direction)

    if !valid(new_position) {
        return false
    }

    tile := get_tile(new_position)

    if !tile.is_floor {
        return false
    }

    if tile.entity_id != nil {
        other := get_entity_with_id(tile.entity_id.?)

        // this is recursive, maybe this will cause issues in the future
        // but for right now it works - 04/01/25
        pushed := push_entity(other, direction)

        if !pushed {
            return false
        }
    }

    unset_entity_in_grid(entity.grid_position)
    entity.grid_position = new_position
    set_entity_in_grid(entity, new_position)
    entity.position = grid_position_to_world(entity.grid_position)

    return true
}

push_entity :: proc(entity: ^Entity, direction: Direction) -> bool {
    if !(.PUSHABLE in entity.flags) {
        return false
    }

    return move_entity(entity, direction)
}

// @setup
setup_game :: proc() {
    level, ok := load_level(.TEST)
    if !ok {
        log.fatal("error loading level..")
        return
    }

    state.level = level

    total_width := f32(GRID_TILE_SIZE * state.level.width)
    total_height := f32(GRID_TILE_SIZE * state.level.height)

    state.camera_position = {total_width, total_height} * 0.5
    state.camera_position -= {GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.5
}

restart :: proc() {
    state.level_complete = false 
    state.entity_count = 0
    
    setup_game()
}

// @update
update :: proc() {
    if state.key_inputs[.ESCAPE] == .DOWN {
        sapp.quit()
    }

    if state.level_complete {
        restart()
        return
    }

    if state.key_inputs[.R] == .DOWN {
        restart()
        return
    }

    // update pass
    for entity_index in 0..<state.entity_count {
        entity := &state.entities[entity_index]

        if entity.inactive {
            continue
        }

        if .PLAYER in entity.flags {
            movement_direction := Direction.NONE

            if state.key_inputs[.W] == .DOWN {
                movement_direction = .UP
            }
            else if state.key_inputs[.S] == .DOWN {
                movement_direction = .DOWN
            }
            else if state.key_inputs[.D] == .DOWN {
                movement_direction = .RIGHT
            }
            else if state.key_inputs[.A] == .DOWN {
                movement_direction = .LEFT
            }

            if movement_direction != .NONE {
                move_entity(entity, movement_direction)

                if entity.grid_position == state.level.end {
                    state.level_complete = true 
                }
            }
        }
    }

    // delete pass
    i : uint = 0
    for i < state.entity_count {
        entity := &state.entities[i]
    
        if .DELETE in entity.flags {
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

// @draw
draw :: proc(delta_time: f32) {
    { // draw grid
        for y in 0..<state.level.height {
            for x in 0..<state.level.width {
                tile := &state.level.grid[y][x]

                if !tile.is_floor {
                    continue
                }

                position := grid_position_to_world({x, y})
                grid_position := Vector2i{x, y}
                inner_rect_size := GRID_TILE_SIZE * f32(0.97)

                tile_colour := WHITE
                if grid_position == state.level.end {
                    tile_colour = YELLOW
                }

                draw_rectangle(position, {GRID_TILE_SIZE, GRID_TILE_SIZE}, brightness(tile_colour, 0.9))
                draw_rectangle(position, {inner_rect_size, inner_rect_size}, tile_colour)
            }
        }
    }

    for &entity in state.entities[0:state.entity_count] {
        switch entity.shape {
        case .RECTANGLE:
            draw_rectangle(entity.position, entity.size, entity.colour, entity.rotation)	
        case .CIRCLE:
            draw_circle(entity.position, entity.size.x * 0.5, entity.colour)
        }
    }

    in_screen_space = true

    if state.level_complete {
        centre := Vector2{state.screen_width * 0.5, state.screen_height * 0.5}
        draw_rectangle(centre, {state.screen_width, 100}, alpha(BLACK, 0.7))
        draw_text("You win", centre, YELLOW, 60)
    }

    { // fps counter
        text := fmt.tprintf("FPS: %v", int(1 / delta_time))
        draw_text(text, {50, 25}, RED, 15)
    }

    { // entity counter
        text := fmt.tprintf("E: %v/%v", state.entity_count, MAX_ENTITIES)
        draw_text(text, {175, 25}, GREEN, 15)
    }

    { // quad counter
        text := fmt.tprintf("Q: %v/%v", state.quad_count, MAX_QUADS)
        draw_text(text, {300, 25}, BLUE, 15)
    }
}
