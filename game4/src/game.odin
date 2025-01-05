package src

import "core:fmt"
import "core:log"
import "base:runtime"

import sapp "sokol/app"

// @setup
setup_game :: proc() {
    level, ok := load_level(.TEST)
    if !ok {
        log.fatal("error loading level..")
        return
    }

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

    { // global controls
        if state.key_inputs[.R] == .DOWN {
            restart()
            return
        }

        if state.key_inputs[.L] == .DOWN {
            for entity_index in 0..<state.entity_count {
                entity := &state.entities[entity_index]

                if .LAMP in entity.flags {
                    entity.direction = clockwise(entity.direction)
                }
            }
        }

        if state.key_inputs[.M] == .DOWN {
            for entity_index in 0..<state.entity_count {
                entity := &state.entities[entity_index]

                if .MIRROR in entity.flags {
                    entity.direction = oppisite(entity.direction)
                }
            }
        }
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

        if .BUTTON in entity.flags {
            being_pressed := false

            iter := new_grid_position_iterator(entity.grid_position)
            other: ^Entity = next(&iter)
            for other != nil {
                being_pressed = true
                other = next(&iter)
            }

            if being_pressed {
                entity.flags += {.ACTIVATED}
            }
            else {
                entity.flags -= {.ACTIVATED}
            }
        }

        if .DOOR in entity.flags {
            watching_entity := get_entity_with_id(entity.watching_entity)
            assert(watching_entity != nil)

            if .ACTIVATED in watching_entity.flags {
                entity.flags += {.NON_BLOCKING}
            }
            else {
                entity.flags -= {.NON_BLOCKING}
            }
        }

        if .LIGHT_RECEIVER in entity.flags {
            found_light := false

            for direction in Direction {
                if direction == .NONE {
                    continue
                }

                if line_of_sight_to_lamp(entity.grid_position, direction) {
                    found_light = true
                    break
                }
            }

            if found_light {
                entity.flags += {.ACTIVATED}
            }
            else {
                entity.flags -= {.ACTIVATED}
            }
        }
    }

    // delete pass
    i := 0
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
        draw_colour := entity.colour

        if .BUTTON in entity.flags {
            if .ACTIVATED in entity.flags {
                draw_colour = brightness(draw_colour, 0.5)
            }
        }

        if .DOOR in entity.flags {
            if .NON_BLOCKING in entity.flags {
                draw_colour = alpha(draw_colour, 0.2)
            }
        }

        if .LIGHT_RECEIVER in entity.flags {
            if .ACTIVATED in entity.flags {
                draw_colour = brightness(draw_colour, 1.5)
                draw_circle(entity.position, GRID_TILE_SIZE * 0.5, alpha(ORANGE, 0.1))
            }
        }

        if .LAMP in entity.flags {
            draw_beam_from_position(entity.grid_position, entity.direction, ORANGE) 
        }

        if .MIRROR in entity.flags {
            // special drawing for a mirror
            angle: f32
            #partial switch entity.direction {
                case .RIGHT: angle = -45
                case .LEFT: angle = 45
                case: unreachable()
            }

            draw_rectangle(entity.position, {GRID_TILE_SIZE, 5}, draw_colour, angle)
            continue
        }

        switch entity.shape {
        case .RECTANGLE:
            draw_rectangle(entity.position, entity.size, draw_colour, entity.rotation)	
        case .CIRCLE:
            draw_circle(entity.position, entity.size.x * 0.5, draw_colour)
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

create_player :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.PLAYER},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.7,
        colour = brightness(RED, 0.75),
        grid_position = grid_position
    })
}

create_button :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.BUTTON, .NON_BLOCKING},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.5,
        colour = SKY_BLUE,
        grid_position = grid_position,
        shape = .CIRCLE
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

create_door :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.DOOR},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE},
        colour = PINK,
        grid_position = grid_position
    })  
}

create_lamp :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.LAMP, .PUSHABLE},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.5,
        colour = ORANGE,
        grid_position = grid_position,
        direction = .UP,
    })  
}

create_light_receiver :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.LIGHT_RECEIVER},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.5,
        colour = brightness(ORANGE, 0.6),
        grid_position = grid_position,
        shape = .CIRCLE
    })  
}

create_mirror :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.MIRROR, .PUSHABLE},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE},
        colour = SKY_BLUE,
        grid_position = grid_position,
        direction = .RIGHT,
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

    iter := new_grid_position_iterator(new_position)
    other: ^Entity = next(&iter)
    for other != nil {
        // this is recursive, maybe this will cause issues in the future
        // but for right now it works - 04/01/25
        pushed := push_entity(other, direction)

        other = next(&iter)

        if !pushed {
            return false
        }
    }

    entity.grid_position = new_position
    entity.position = grid_position_to_world(entity.grid_position)

    return true
}

push_entity :: proc(entity: ^Entity, direction: Direction) -> bool {
    if !(.PUSHABLE in entity.flags) {
        return false
    }

    return move_entity(entity, direction)
}

// takes into account reflections from mirrors, start position
// is assumed to be an entity, so we skip that position
// maybe this assumption will need to chang ebut right now
// only light receivers are checking for lies of sight to lamps
// and they always do it from their location - 05/01/25
line_of_sight_to_lamp :: proc(start_position: Vector2i, direction: Direction) -> bool {
    assert(direction != .NONE)

    current_direction := direction
    current_position := start_position + direction_grid_offset(direction)

    check_current_position:
    for valid(current_position) {
        iter := new_grid_position_iterator(current_position)

        for {
            entity := next(&iter)
            if entity == nil {
                current_position += direction_grid_offset(current_direction)
                break
            }
            else if .LAMP in entity.flags {
                return true
            }
            else if .MIRROR in entity.flags {
                incoming_direction_for_mirror := oppisite(current_direction)

                current_direction = mirror_reflection(entity.direction, incoming_direction_for_mirror) 
                current_position += direction_grid_offset(current_direction)
                break
            }
            else if !(.NON_BLOCKING in entity.flags) && !(.LIGHT_RECEIVER in entity.flags) {
                return false
            } else {
                current_position += direction_grid_offset(current_direction)
            }
        }
    }


    return false
}

// position is assumed to be a lamp location so it is skipped
// when checking to draw, the first beam to be draw will be
// at the grid position adjacent to the starting position 
// at the direction passed in, this assumption might change - 05/01/25
draw_beam_from_position :: proc(position: Vector2i, direction: Direction, colour: Colour) {
    assert(direction != .NONE)
    assert(valid(position))

    beam_colour := alpha(colour, 0.2)

    // starting position is skipped
    current_direction := direction
    current_position := position + direction_grid_offset(current_direction)

    current_position_check:
    for valid(current_position) {
        iter := new_grid_position_iterator(current_position)
        for {
            other := next(&iter)
            if other == nil {
                break
            }

            if .MIRROR in other.flags {
                { // draw mirror highlight
                    world_position := grid_position_to_world(current_position)
                    draw_circle(world_position, GRID_TILE_SIZE * 0.5, beam_colour)
                }

                { // reflect beam direction
                    incoming_direction_to_mirror := oppisite(current_direction)
                    current_direction = mirror_reflection(other.direction, incoming_direction_to_mirror)

                    current_position += direction_grid_offset(current_direction)
                    continue current_position_check
                }
            }
            else if !(.NON_BLOCKING in other.flags) && !(.LIGHT_RECEIVER in other.flags) {
                break current_position_check
            }
        }

        size := Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE}
        if current_direction == .UP || current_direction == .DOWN {
            size *= {0.3, 1}
        } else {
            size *= {1, 0.3}
        }
         
        world_position := grid_position_to_world(current_position)
        draw_rectangle(world_position, size, beam_colour)
        current_position += direction_grid_offset(current_direction)
    }
}













