package src

import "core:fmt"
import "core:log"
import "base:runtime"

import sapp "sokol/app"

MAX_BEAM_POSITION_CHECKS :: 50

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
        if state.key_inputs[.T] == .DOWN {
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

        player_update: {
            if !(.PLAYER in entity.flags) {
                break player_update
            }

            player_movement: {
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
                    // if the firection the player is facing is not the same
                    // as the current direction then first check if there
                    // is any entities that are rotatable/pushable in that tile if there is 
                    // then only change the direction of the player, this allows
                    // the player to rotate without push entities by accident
                    // - 05/01/25
                    move := true
                    if movement_direction != entity.direction {
                        moving_to_position := entity.grid_position + direction_grid_offset(movement_direction)
    
                        iter := new_grid_position_iterator(moving_to_position)
                        for {
                            neighbour := next(&iter)
                            if neighbour == nil {
                                break
                            }
    
                            if .ROTATABLE in neighbour.flags || .PUSHABLE in neighbour.flags {
                                move = false 
                                break
                            }
                        }
                    }
    
                    entity.direction = movement_direction
    
                    if move {
                        move_entity(entity, movement_direction)
                    }
    
                    if entity.grid_position == state.level.end {
                        state.level_complete = true 
                    }
                }
            }

            player_rotate: {
                facing_position := entity.grid_position + direction_grid_offset(entity.direction)

                if state.key_inputs[.R] != .DOWN || !valid(facing_position) {
                    break player_rotate
                }

                iter := new_grid_position_iterator(facing_position)
                for {
                    other := next(&iter) 
                    if other == nil {
                        break
                    }

                    if .ROTATABLE in other.flags {
                        other.direction = clockwise(other.direction) 
                    }
                }
            }

            player_death_beam: {
                // this is probably not the best way to do this but ohh well
                for direction in Direction {
                    if direction == .NONE {
                        continue
                    }
    
                    if line_of_sight_to_lamp(entity.grid_position, .DEATH, direction) {
                        entity.inactive = true
                        entity.colour = RED
                        break
                    }
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

        door_update: {
            if !(.DOOR in entity.flags) {
                break door_update
            }

            watching_entity := get_entity_with_id(entity.watching_entity)
            if watching_entity == nil {
                log.errorf("door %v couldn't font entity to watch", entity.grid_position)
                break door_update
            }
 
            if .ACTIVATED in watching_entity.flags {
                entity.flags += {.NON_BLOCKING}
            }
            else {
                entity.flags -= {.NON_BLOCKING}
            }
        }

        if .LIGHT_DETECTOR in entity.flags {
            found_light := false

            for direction in Direction {
                if direction == .NONE {
                    continue
                }

                if line_of_sight_to_lamp(entity.grid_position, entity.lamp_type, direction) {
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

        if .PLAYER in entity.flags {
            eyes_centre_position: Vector2

            eye_offset := entity.size * 0.5

            switch entity.direction {
                case .NONE: unreachable()
                case .UP:
                    eyes_centre_position.y = eye_offset.y
                case .DOWN:
                    eyes_centre_position.y = -eye_offset.y
                case .LEFT:
                    eyes_centre_position.x = -eye_offset.x
                case .RIGHT:
                    eyes_centre_position.x = eye_offset.x
            }

            EYE_GAP :: f32(GRID_TILE_SIZE * 0.25)

            left_eye_position := eyes_centre_position
            right_eye_position := eyes_centre_position

            switch entity.direction {
                case .NONE: unreachable()

                case .UP: fallthrough;
                case .DOWN: {
                    left_eye_position.x -= EYE_GAP * 0.5
                    right_eye_position.x += EYE_GAP * 0.5
                }
                case .LEFT: fallthrough;
                case .RIGHT: {
                    left_eye_position.y -= EYE_GAP * 0.5
                    right_eye_position.y += EYE_GAP * 0.5
                }
            }

            draw_circle(entity.position + right_eye_position, 5, BLACK)
            draw_circle(entity.position + left_eye_position, 5, BLACK)
        }

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

        if .LIGHT_DETECTOR in entity.flags {
            if .ACTIVATED in entity.flags {
                draw_colour = brightness(draw_colour, 2)
                draw_circle(entity.position, GRID_TILE_SIZE * 0.5, alpha(draw_colour, 0.2))
            }
        }

        if .LAMP in entity.flags {
            light_colour := alpha(entity.colour, 0.2)
            draw_circle(entity.position, GRID_TILE_SIZE * 0.5, light_colour)
            draw_beam_from_position(entity.grid_position, entity.direction, light_colour) 
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
        colour = brightness(BLUE, 0.75),
        grid_position = grid_position,
        direction = .RIGHT
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

create_lamp :: proc(grid_position: Vector2i, type: LampType) -> ^Entity {
    return create_entity(Entity{
        flags = {.LAMP, .PUSHABLE, .ROTATABLE},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.5,
        colour = lamp_colour(type),
        grid_position = grid_position,
        direction = .UP,
        lamp_type = type,
    })  
}

create_light_detector :: proc(grid_position: Vector2i, type: LampType) -> ^Entity {
    return create_entity(Entity{
        flags = {.LIGHT_DETECTOR},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.5,
        colour = brightness(lamp_colour(type), 0.5),
        grid_position = grid_position,
        shape = .CIRCLE,
        lamp_type = type
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
line_of_sight_to_lamp :: proc(start_position: Vector2i, type: LampType, direction: Direction) -> bool {
    assert(direction != .NONE)

    current_direction := direction
    current_position := start_position + direction_grid_offset(direction)

    position_checked := 0

    check_current_position:
    for valid(current_position) {
        position_checked += 1
        if position_checked > MAX_BEAM_POSITION_CHECKS {
            return false
        }

        iter := new_grid_position_iterator(current_position)

        for {
            entity := next(&iter)
            if entity == nil {
                current_position += direction_grid_offset(current_direction)
                break
            }
            else if .LAMP in entity.flags {
                // only a valid lamp if it is the same type and it is pointing
                // in the direction we are coming from
                if entity.lamp_type == type && entity.direction == oppisite(current_direction) {
                    return true
                }

                current_position += direction_grid_offset(current_direction)
            }
            else if .MIRROR in entity.flags {
                incoming_direction_for_mirror := oppisite(current_direction)

                current_direction = mirror_reflection(entity.direction, incoming_direction_for_mirror) 
                current_position += direction_grid_offset(current_direction)
                break
            }
            else if !(.NON_BLOCKING in entity.flags) && !(.LIGHT_DETECTOR in entity.flags) {
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

    // starting position is skipped
    current_direction := direction
    current_position := position + direction_grid_offset(current_direction)

    positions_checked := 0

    current_position_check:
    for valid(current_position) {
        positions_checked += 1
        if positions_checked > MAX_BEAM_POSITION_CHECKS {
            return
        }

        iter := new_grid_position_iterator(current_position)
        for {
            other := next(&iter)
            if other == nil {
                break
            }

            if .MIRROR in other.flags {
                { // draw mirror highlight
                    world_position := grid_position_to_world(current_position)
                    draw_circle(world_position, GRID_TILE_SIZE * 0.5, colour)
                }

                { // reflect beam direction
                    incoming_direction_to_mirror := oppisite(current_direction)
                    current_direction = mirror_reflection(other.direction, incoming_direction_to_mirror)

                    current_position += direction_grid_offset(current_direction)
                    continue current_position_check
                }
            }
            else if !(.NON_BLOCKING in other.flags) && !(.LIGHT_DETECTOR in other.flags) {
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
        draw_rectangle(world_position, size, colour)
        current_position += direction_grid_offset(current_direction)
    }
}













