package src

import "core:fmt"
import "core:log"
import "core:strings"
import "base:runtime"
import sa "core:container/small_array"

import sapp "sokol/app"

MAX_BEAM_POSITION_CHECKS :: 50

// @setup
setup_game :: proc() {
    ok := load_level(state.current_level)
    if !ok {
        log.fatal("error loading level..")
        return
    }

    // when a new level is loaded, a new save point is created
    // this ensure the state of the level at the start is saved
    // and that it can be reverted to
    create_save_point()
}

restart :: proc() {
    state.entity_count = 0
    free_all(level_allocator)

    setup_game()
}

// @update
update :: proc() {
    { // global controls
        if state.key_inputs[.ESCAPE] == .DOWN {
            sapp.quit()
            return
        }

        // once the level is comlete nothing gets updated and most of the global controls
        // get disabled here aswell
        if state.level.complete {
            state.display_controls = false

            if state.key_inputs[.SPACE] == .DOWN {
                if REPEAT_LEVEL {
                    restart()
                } else {
                    next_level()
                } 
            }

            return
        }

        if state.key_inputs[.TAB] == .DOWN {
            state.display_controls = !state.display_controls
        }

        if state.key_inputs[.T] == .DOWN {
            restart()
            return
        } 

        if state.key_inputs[.U] == .DOWN {
            load_save_point()
            return
        }

        if state.key_inputs[.RIGHT] == .DOWN {
            change_player(true)
        }

        if state.key_inputs[.LEFT] == .DOWN {
            change_player(false)
        }

        when ODIN_DEBUG {
            if state.key_inputs[.N] == .DOWN {
                next_level()
                return
            }
    
            if state.key_inputs[.P] == .DOWN {
                previous_level()
                return
            }
        }
    } 

    // entity pass
    for entity_index in 0..<state.entity_count {
        entity := &state.entities[entity_index]

        if .DELETED in entity.flags {
            continue
        }

        player_update: {
            if !(.PLAYER in entity.flags) {
                break player_update
            }

            if !(.ACTIVE_PLAYER in entity.flags) {
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

                if movement_direction == .NONE {
                    break player_movement
                }

                player_grab: {
                    if state.key_inputs[.LEFT_SHIFT] != .PRESSING {
                        break player_grab
                    }

                    looking_direction   := entity.direction
                    behind_direction    := oppisite(entity.direction)

                    // can only grab and move backwards
                    if movement_direction != behind_direction {
                        break player_grab
                    }

                    looking_position := entity.grid_position + direction_grid_offset(looking_direction)

                    grabbed_entity := get_entity_at_position_with_flags(looking_position, {.MOVEABLE})
                    if grabbed_entity == nil {
                        break player_grab 
                    }
                    
                    move_entity(entity, behind_direction)
                    move_entity(grabbed_entity, behind_direction)

                    break player_movement
                }
                    
                // if the direction the player is facing is not the same
                // as the current direction then first check if there
                // is any entities that are rotatable/pushable in that tile if there is 
                // then only change the direction of the player, this allows
                // the player to rotate without push entities by accident
                // - 05/01/25
                move := true
                if movement_direction != entity.direction {
                    moving_to_position := entity.grid_position + direction_grid_offset(movement_direction)

                    other := get_entity_at_position_with_flags(moving_to_position, {.ROTATABLE, .MOVEABLE})
                    if other != nil {
                        move = false 
                    }
                }
 
                entity.direction = movement_direction
 
                if move {
                    move_entity(entity, movement_direction)
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
                        rotate_entity(other)
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
                        entity.flags += {.DELETED}
                        break
                    }
                }
            }

            player_win: {
                if entity.player_type != .PRIMARY {
                    break player_win
                }

                if entity.grid_position == state.level.end {
                    state.level.complete = true
                }
            }
        }

        button_update: {
            if !(.BUTTON in entity.flags) {
                break button_update
            }

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

        key_door_update: {
            if !(.KEY_DOOR in entity.flags) {
                break key_door_update
            }

            activated := .ACTIVATED in entity.flags

            if activated {
                entity.flags += {.NON_BLOCKING}    
            }
            else {
                entity.flags -= {.NON_BLOCKING}    
            }
        }

        door_update: {
            if !(.DOOR in entity.flags) {
                break door_update
            }

            should_be_open := true

            open_check: {
                for watching_id in sa.slice(&entity.watching) {
                    watching_entity := get_entity_with_id(watching_id)
                    if watching_entity == nil {
                        log.errorf("door %v couldn't font entity to watch", entity.grid_position)
                        break door_update
                    }
    
                    if !(.ACTIVATED in watching_entity.flags) {
                        should_be_open = false
                        break
                    }
                }


                if get_entity_at_position_without_flags(entity.grid_position, {.DOOR}) != nil {
                    should_be_open = true
                }
            }

            if should_be_open {
                entity.flags += {.NON_BLOCKING}
            }
            else {
                entity.flags -= {.NON_BLOCKING}
            }
        }

        light_detector_update: {
            if !(.LIGHT_DETECTOR in entity.flags) {
                break light_detector_update
            }

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

    // HACK: this is here because you need to garuntee all entities are launched
    // off of launch pads before the end of the tick because if another entity 
    // move onto a launch pad without it being moved because that happened after
    // the launch pad checked to see if it was there then a save will happen
    // with that entity on the jump pad meaning it will forever go back and forth
    // on it - 09/01/25

    // HACK: same thing needs to be done for keys because they need to be alligned with
    // the player by the end of the update or else they will weirdly teleport every
    // time the player moves with the key in hand

    // there are two ways to fix this
    // - have different entity types, each in their own buffer so the order at which
    //   they are updated is not related to when they are created, but how we choose
    // - don't undo a save point at a time but continue to roll back saves as the player
    //   wants while they hold the undo button, this allows to not force an update while 
    //   it is on the launch pad unless the player wants that to happen
    for entity_index in 0..<state.entity_count {
        entity := &state.entities[entity_index]

        if .DELETED in entity.flags {
            continue
        }

        if !(.LAUNCH_PAD in entity.flags) {
            continue
        }

        iter := new_grid_position_iterator(entity.grid_position)
        for {
            other := next(&iter)
            if other == nil {
                break
            }

            if other == entity {
                continue
            }

            if !(.MOVEABLE in other.flags) {
                continue
            }

            launch_entity(other, entity.jump_pad_target)
        }
    }

    for entity_index in 0..<state.entity_count {
        entity := &state.entities[entity_index]

        if !(.KEY in entity.flags) {
            continue
        }

        if .KEY_USED in entity.flags {
            continue
        }

        pickup_check: {
            if .IN_PLAYER_HAND in entity.flags {
                break pickup_check
            }

            iter := new_grid_position_iterator(entity.grid_position)
            for {
                other := next(&iter)
                if other == nil {
                    break pickup_check
                }
                    
                if !(.PLAYER in other.flags) {
                    continue 
                }

                if other.grid_position == entity.grid_position {
                    entity.attached_player = other.id
                    entity.flags += {.IN_PLAYER_HAND}
                    break pickup_check
                }
            }
        }

        in_hand: {
            if !(.IN_PLAYER_HAND in entity.flags) {
                break in_hand
            }

            player := get_entity_with_id(entity.attached_player)
            assert(player != nil)

            entity.position = player.position - Vector2{GRID_TILE_SIZE * 0.3, GRID_TILE_SIZE * 0.3}
            entity.grid_position = player.grid_position

            // check all adjacent tiles and check if there is a door
            // if it is then activated it
            for direction in Direction {
                if direction == .NONE {
                    continue
                }
                    
                checking_position := entity.grid_position + direction_grid_offset(direction)

                other := get_entity_at_position_with_flags(checking_position, {.KEY_DOOR})
                if other == nil {
                    continue
                }

                if !(.ACTIVATED in other.flags) {
                    other.flags += {.ACTIVATED}

                    entity.flags += {.KEY_USED}
                    entity.flags -= {.IN_PLAYER_HAND}
                }
            }
        }
    }
    
    if state.save_this_tick {
        create_save_point()
        state.save_this_tick = false
    }
}

// TODO: find a better way to do this
dot_drawing_offset : f32 = 0

// @draw
draw :: proc(delta_time: f32) {
    fit_and_centre_camera()

    dot_drawing_offset += delta_time

    if dot_drawing_offset > 1 {
        dot_drawing_offset = 0
    }

    { // draw grid
        for y in 0..<state.level.height {
            for x in 0..<state.level.width {
                grid_position := Vector2i{x, y}
                tile_active := is_tile_active(grid_position)

                if !tile_active {
                    continue
                }

                position := grid_position_to_world({x, y})
                inner_rect_size := GRID_TILE_SIZE * f32(0.97)

                tile_colour := WHITE
                if grid_position == state.level.end {
                    tile_colour = YELLOW
                }

                draw_rectangle(position, {GRID_TILE_SIZE, GRID_TILE_SIZE}, brightness(tile_colour, 0.9), .THREE)
                draw_rectangle(position, {inner_rect_size, inner_rect_size}, tile_colour, .THREE)
            }
        }
    }

    for &entity in state.entities[0:state.entity_count] {
        if .DELETED in entity.flags {
            continue
        }

        draw_colour := entity.colour 

        draw_player: {
            if !(.PLAYER in entity.flags) {
                break draw_player
            }

            if !(.ACTIVE_PLAYER in entity.flags) {
                draw_colour = greyscale(draw_colour)
            }

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

            draw_circle(entity.position + right_eye_position, 5, BLACK, .ONE)
            draw_circle(entity.position + left_eye_position, 5, BLACK, .ONE)
        }

        draw_button: {
            if !(.BUTTON in entity.flags) {
                break draw_button
            }

            if .ACTIVATED in entity.flags {
                draw_colour = brightness(draw_colour, 0.5)
            }
        }

        draw_key: {
            if !(.KEY in entity.flags) {
                break draw_key
            }

            if .KEY_USED in entity.flags {
                draw_colour = brightness(draw_colour, 0.5)
            }
        }

        draw_key_door: {
            if !(.KEY_DOOR in entity.flags) {
                break draw_key_door
            }

            if .ACTIVATED in entity.flags {
                draw_colour = alpha(draw_colour, 0.2)
            }
        }

        draw_door: {
            if !(.DOOR in entity.flags) {
                break draw_door
            }

            if .NON_BLOCKING in entity.flags {
                draw_colour = alpha(draw_colour, 0.2)
            }
        }

        draw_light_detector: {
            if !(.LIGHT_DETECTOR in entity.flags) {
                break draw_light_detector
            }

            if .ACTIVATED in entity.flags {
                draw_colour = brightness(draw_colour, 2)
                draw_circle(entity.position, GRID_TILE_SIZE * 0.5, alpha(draw_colour, 0.2), .ONE)
            }
        }

        draw_lamp: {
            if !(.LAMP in entity.flags) {
                break draw_lamp
            }

            light_colour := alpha(entity.colour, 0.2)
            draw_circle(entity.position, GRID_TILE_SIZE * 0.5, light_colour, .ONE)
            draw_beam_from_position(entity.grid_position, entity.direction, light_colour) 
        } 

        draw_mirror: {
            if !(.MIRROR in entity.flags) {
                break draw_mirror
            }

            // TODO: just set the rotation in the entity
            // that is what it is for
            // special drawing for a mirror
            angle: f32
            #partial switch entity.direction {
                case .RIGHT, .LEFT: angle = -45
                case .UP, .DOWN: angle = 45
                case: unreachable()
            }

            draw_rectangle(entity.position, {GRID_TILE_SIZE, 5}, draw_colour, entity.layer, rotation = angle)
            continue
        }

        draw_watching: {
            if !(.WATCHING in entity.flags) {
                break draw_watching
            }

            for watching_id in sa.slice(&entity.watching) {
                watching_entity := get_entity_with_id(watching_id)
                if watching_entity == nil {
                    break draw_watching
                }
    
                distance := length(watching_entity.position - entity.position)
                dot_count := i64(distance / (GRID_TILE_SIZE * 0.5)) // dot every half tile
    
                if dot_count < 5 {
                    dot_count = 5
                }
    
                draw_dotted_line(
                    watching_entity.position,
                    entity.position,
                    dot_count,
                    2,
                    alpha(BLUE, 0.2),
                    .ZERO,
                    dot_drawing_offset
                )
            }
        }

        draw_launch_pad: {
            if !(.LAUNCH_PAD in entity.flags) {
                break draw_launch_pad
            }

            start_position := entity.position
            end_position := grid_position_to_world(entity.jump_pad_target)

            distance := length(end_position - start_position)
            dot_count := i64(distance / (GRID_TILE_SIZE * 0.5)) // dot every half tile
 
            if dot_count < 5 {
                dot_count = 5
            }
 
            draw_dotted_line(
                start_position,
                end_position,
                dot_count,
                2,
                alpha(RED, 0.3),
                .ONE,
                dot_drawing_offset
            )
        }

        draw_no_undo: {
            if !(.NO_UNDO in entity.flags) {
                break draw_no_undo
            }

            highlight_alpha := 0.5 * draw_colour.a
            highlight_colour := alpha(RED, highlight_alpha)

            switch entity.shape {
                case .RECTANGLE: {
                    // this makes the outline around the rectangle the same size
                    // for rectangles that are not equal width and height
                    scaled_size := entity.size * 0.20
                    highlight_size := entity.size + max(scaled_size.x, scaled_size.y) 

                    draw_rectangle(entity.position, highlight_size, highlight_colour, .TWO, rotation = entity.rotation)	
                }
                case .CIRCLE: {
                    highlight_radius := (entity.size.x * 0.5) * 1.15
                    draw_circle(entity.position, highlight_radius, highlight_colour, .TWO)
                }
        }   
        }

        switch entity.shape {
            case .RECTANGLE: {
                draw_rectangle(entity.position, entity.size, draw_colour, entity.layer, rotation = entity.rotation)	
            }
            case .CIRCLE: {
                draw_circle(entity.position, entity.size.x * 0.5, draw_colour, entity.layer)
            }
        }
    }

    in_screen_space = true

    { // UI
        if state.level.complete {
            draw_rectangle({state.screen_width * 0.5, state.screen_height * 0.5}, {state.screen_width, 100}, BLACK, .UI_ONE)
            draw_text("Complete - Press Space", {state.screen_width * 0.5, state.screen_height * 0.5}, YELLOW, 30, .UI_ZERO)
        }

        if state.display_controls {
            centre := Vector2{state.screen_width * 0.5, state.screen_height * 0.5}
    
            draw_rectangle(centre, {800, 500}, BLACK, .UI_ONE)
            draw_text("move: WASD", centre + {0, 230}, YELLOW, 20, .UI_ZERO)
            draw_text("rotate: R", centre + {0, 190}, YELLOW, 20, .UI_ZERO)
            draw_text("grab: shift", centre + {0, 150}, YELLOW, 20, .UI_ZERO)
    
            draw_text("undo: U", centre + {0, 50}, YELLOW, 20, .UI_ZERO)
            draw_text("restart: T", centre + {0, 10}, YELLOW, 20, .UI_ZERO)
    
            draw_text("next player: right", centre + {0, -90}, YELLOW, 20, .UI_ZERO)
            draw_text("prev player: left", centre + {0, -130}, YELLOW, 20, .UI_ZERO)
    
            draw_text("controls: tab", centre + {0, -230}, YELLOW, 20, .UI_ZERO)
        } 

        { // level name
            level_display_name: string

            {
                name := level_name(state.current_level)
                seperated := strings.split(name, "/", context.temp_allocator)
                
                if len(seperated) == 1 {
                    name = seperated[0]
                } else {
                    name = seperated[1]
                }

                level_display_name, _ = strings.replace_all(name, "_", "  ", context.temp_allocator)
            }

            draw_text(level_display_name, {state.screen_width * 0.5, state.screen_height - 20}, BLACK, 30, .UI_ZERO)
        }

        { // note
            if len(state.level.note) > 0 {
                position := Vector2{state.screen_width * 0.5, 25}
                background_width := f32(len(state.level.note) * 25)

                draw_rectangle(position, {background_width, 50}, BLACK, .UI_ONE)
                draw_text(state.level.note, position , YELLOW, 30, .UI_ZERO)
            }


        }
    
        { // fps counter
            text := fmt.tprintf("FPS: %v", int(1 / delta_time))
            draw_text(text, {50, 25}, RED, 15, .UI_ZERO)
        }
    
        { // entity counter
            text := fmt.tprintf("E: %v/%v", state.entity_count, MAX_ENTITIES)
            draw_text(text, {175, 25}, GREEN, 15, .UI_ZERO)
        }
    
        { // quad counter
            text := fmt.tprintf("Q: %v/%v", state.quad_count, MAX_QUADS)
            draw_text(text, {300, 25}, BLUE, 15, .UI_ZERO)
        }
    } 
}

create_player :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.PLAYER, .ACTIVE_PLAYER, .MOVEABLE},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.7,
        colour = brightness(BLUE, 0.75),
        grid_position = grid_position,
        direction = .UP,
        player_type = .PRIMARY,
        layer = .ONE,
    })
}

create_secondary_player :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.PLAYER, .MOVEABLE},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.7,
        colour = brightness(GREEN, 0.75),
        grid_position = grid_position,
        direction = .UP,
        player_type = .SECONDARY,
        layer = .ONE,
    })
}

create_button :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.BUTTON, .NON_BLOCKING},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.5,
        colour = SKY_BLUE,
        grid_position = grid_position,
        shape = .CIRCLE,
        layer = .TWO
    })  
}

create_key :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.KEY, .NON_BLOCKING},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE * 0.5, GRID_TILE_SIZE * 0.18},
        colour = YELLOW,
        grid_position = grid_position,
        layer = .ZERO,
    })  
}

create_key_door :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.KEY_DOOR},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE},
        colour = brightness(YELLOW, 0.6),
        grid_position = grid_position,
        layer = .ONE,
    })  
}

create_rock :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.MOVEABLE},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.85,
        colour = BROWN,
        grid_position = grid_position,
        shape = .CIRCLE,
        layer = .ONE,
    })  
}

create_wall :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.NONE},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE},
        colour = BLACK,
        grid_position = grid_position,
        layer = .ONE,
    })  
}

create_door :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.DOOR, .WATCHING},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE},
        colour = DARK_GRAY,
        grid_position = grid_position,
        layer = .ONE,
    })  
}

create_lamp :: proc(grid_position: Vector2i, type: LampType) -> ^Entity {
    if type == .LIGHT {
        return create_entity(Entity{
            flags = {.LAMP, .MOVEABLE, .ROTATABLE},
            position = grid_position_to_world(grid_position),
            size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.5,
            colour = lamp_colour(type),
            grid_position = grid_position,
            direction = .UP,
            lamp_type = type,
            layer = .ONE,
        })
    }

    if type == .DEATH {
        return create_entity(Entity{
            flags = {.LAMP, .MOVEABLE, .ROTATABLE, .NO_UNDO},
            position = grid_position_to_world(grid_position),
            size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.5,
            colour = lamp_colour(type),
            grid_position = grid_position,
            direction = .UP,
            lamp_type = type,
        })
    }
     
    unreachable()
}

create_light_detector :: proc(grid_position: Vector2i, type: LampType) -> ^Entity {
    return create_entity(Entity{
        flags = {.LIGHT_DETECTOR, .NON_BLOCKING},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.5,
        colour = brightness(lamp_colour(type), 0.5),
        grid_position = grid_position,
        shape = .CIRCLE,
        lamp_type = type,
        layer = .ONE
    })  
}

create_mirror :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.MIRROR, .MOVEABLE, .ROTATABLE},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE},
        colour = SKY_BLUE,
        grid_position = grid_position,
        direction = .RIGHT,
        layer = .ONE,
    })  
}

create_launch_pad :: proc(grid_position: Vector2i) -> ^Entity {
    return create_entity(Entity{
        flags = {.LAUNCH_PAD, .NON_BLOCKING},
        position = grid_position_to_world(grid_position),
        size = Vector2{GRID_TILE_SIZE, GRID_TILE_SIZE} * 0.8,
        colour = GREEN,
        grid_position = grid_position,
        layer = .TWO
    })  
}

change_player :: proc(look_right: bool) {
    // change player works by first looking for the current player that has the
    // active flag. It then finds the first entity that has a player flag starting
    // one index to the next of the active player (-1 if looking left, 1 for right)
    // once another player is found then the flags are switched. When the searching
    // index reaches the end or begining of the entity array it is not ensured to have
    // checked every entity so the search index wraps around based on the direction. 
    // Because of the wrapping we know if the search index is eventually eqaul to the
    // active player index then no other player is in the game and the search is canceled
    // with no changes - 10/01/25
    active_player: ^Entity
    active_player_index: int

    for &entity, i in state.entities[0 : state.entity_count] {
        if .ACTIVE_PLAYER in entity.flags {
            active_player = &entity
            active_player_index = i

            assert(.PLAYER in entity.flags)

            break
        }
    }

    assert(active_player != nil, "no active players found in game ??")

    search_index: int

    if look_right {
        search_index = active_player_index + 1
    } else {
        search_index = active_player_index - 1
    }

    if search_index >= state.entity_count {
            search_index = 0
    } else if search_index < 0 {
            search_index = state.entity_count - 1
    }

    for {
        if search_index == active_player_index {
            log.info("tried to change player but not other players found.. doing nothing")
            return
        }

        entity := &state.entities[search_index]

        if .PLAYER in entity.flags {
            entity.flags += {.ACTIVE_PLAYER}
            active_player.flags -= {.ACTIVE_PLAYER}

            log.info("updated active player")
            return
        }

        // update search index based on direction and wrap if needed
        if look_right {
            if search_index == state.entity_count - 1 {
                search_index = 0
            }
            else {
                search_index += 1
            }
        } else {
            if search_index == 0 {
                search_index = state.entity_count - 1
            }
            else {
                search_index -= 1
            }
        }
    }
}

move_entity :: proc(entity: ^Entity, direction: Direction) -> bool {
    new_position := entity.grid_position + direction_grid_offset(direction)

    if !valid(new_position) {
        return false
    }

    tile_active := is_tile_active(new_position)

    if !tile_active {
        return false
    }

    iter := new_grid_position_iterator(new_position)
    for {
        other := next(&iter)
        if other == nil {
            break
        }

        if .NON_BLOCKING in other.flags {
            continue
        }
        else if .MOVEABLE in other.flags {
            pushed := move_entity(other, direction)
            if !pushed {
                return false
            }
        } 
        else {
            return false
        }
    }

    entity.grid_position = new_position
    entity.position = grid_position_to_world(entity.grid_position)

    if !(.NO_UNDO in entity.flags) {
        save_tick()
    }

    return true
}

launch_entity :: proc(entity: ^Entity, new_position: Vector2i) {
    if !valid(new_position) {
        return 
    }

    tile_active := is_tile_active(new_position)

    if !tile_active {
        return
    }

    // when an entity is launched to a new position we need to get the
    // direction from the starting position, this is then used as the
    // direction that any entity at the new position needs to move with
    // if the entity at the new position cannot move then the launch is not
    // done - 09/01/25
    direction_to_new_position := direction_to_position(entity.grid_position, new_position)

    iter := new_grid_position_iterator(new_position)
    for {
        other := next(&iter)
        if other == nil {
            break
        }

        if .NON_BLOCKING in other.flags {
            continue
        }
        else if .MOVEABLE in other.flags {
            pushed := move_entity(other, direction_to_new_position)
            if !pushed {
                return
            }
        } 
        else {
            return
        }
    }

    entity.grid_position = new_position
    entity.position = grid_position_to_world(entity.grid_position)
    
    if !(.NO_UNDO in entity.flags) {
        save_tick()
    }
}

rotate_entity :: proc(entity: ^Entity) {
    assert(.ROTATABLE in entity.flags)

    entity.direction = clockwise(entity.direction)

    if !(.NO_UNDO in entity.flags) {
        save_tick()
    }
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
                    draw_circle(world_position, GRID_TILE_SIZE * 0.5, colour, .ONE)
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
        draw_rectangle(world_position, size, colour, .ONE)
        current_position += direction_grid_offset(current_direction)
    }
}













