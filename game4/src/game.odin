package src

import "core:fmt"
import "core:log"
import "base:runtime"

import sapp "sokol/app"

create_player :: proc(grid_position: Vector2i) -> ^Entity {
    entity := create_entity(Entity{
        flags = {.PLAYER},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.7,
        colour = brightness(RED, 0.75),
        grid_position = grid_position
    })

    set_entity_in_grid(entity, entity.grid_position)
    return entity
}

create_rock :: proc(grid_position: Vector2i) -> ^Entity {
    entity := create_entity(Entity{
        flags = {.PUSHABLE},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.85,
        colour = BROWN,
        grid_position = grid_position,
        shape = .CIRCLE,
    })  
    
    set_entity_in_grid(entity, entity.grid_position)
    return entity
}

create_wall :: proc(grid_position: Vector2i) -> ^Entity {
    entity := create_entity(Entity{
        flags = {.NONE},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE},
        colour = BLACK,
        grid_position = grid_position
    })  
    
    set_entity_in_grid(entity, entity.grid_position)
    return entity
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

// @update
update :: proc() {
    if state.key_inputs[.ESCAPE] == .DOWN {
        sapp.quit()
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

on_trigger_collision :: proc(trigger: ^Entity, other: ^Entity) {
}

on_solid_collision :: proc(entity: ^Entity, other: ^Entity) {
}

// @draw
draw :: proc(delta_time: f32) {
    { // draw grid
        for y in 0..<GRID_HEIGHT {
            for x in 0..<GRID_WIDTH {
                tile := &state.grid[y][x]

                if !tile.is_floor {
                    continue
                }

                position := grid_position_to_world({x, y})
                inner_rect_size := GRID_TILE_SIZE * f32(0.97)

                draw_rectangle(position, {GRID_TILE_SIZE, GRID_TILE_SIZE}, LIGHT_GRAY)
                draw_rectangle(position, {inner_rect_size, inner_rect_size}, WHITE)
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

    { // fps counter
        text := fmt.tprintf("FPS: %v", int(1 / delta_time))
        draw_text(text, {50, 25}, RED, 12)
    }

    { // entity counter
        text := fmt.tprintf("E: %v/%v", state.entity_count, MAX_ENTITIES)
        draw_text(text, {175, 25}, GREEN, 12)
    }

    { // quad counter
        text := fmt.tprintf("Q: %v/%v", state.quad_count, MAX_QUADS)
        draw_text(text, {300, 25}, BLUE, 12)
    }
}
