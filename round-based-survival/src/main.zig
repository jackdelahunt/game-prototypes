const std = @import("std");

const raylib = @cImport(@cInclude("raylib.h"));

const render = @import("render.zig");

const MAX_ENTITIES = 100;

const PLAYER_SPEED  = 200;
const ENEMY_SPEED   = 120;

const WINDOW_WIDTH  = 1200;
const WINDOW_HEIGHT = 900;

const TICKS_PER_SECONDS: f32    = 20;
const TICK_RATE: f32            = 1.0 / TICKS_PER_SECONDS;

const DEBUG_DRAW_CENTRE_POINTS          = false;
const DEBUG_DRAW_MOUSE_DIRECTION_ARROWS = false;
const DEBUG_DRAW_ENEMY_ATTACK_BOXES     = true;

/////////////////////////////////////////////////////////////////////////
///                         @state
/////////////////////////////////////////////////////////////////////////
const State = struct {
    const Self = @This();

    const InputState = enum {
        up,
        down, 
        pressing, 
        released,
    };

    keyboard: [348]InputState,
    mouse: [7]InputState,
    mouse_screen_position: V2f,
    mouse_world_position: V2f,
    time_since_start: f64,
    tick_timer: f64,
    update_time_nanoseconds: u64,
    physics_time_nanoseconds: u64,
    draw_time_nanoseconds: u64,
    camera: raylib.Camera2D,
    entities: std.BoundedArray(Entity, MAX_ENTITIES),
    level: struct {
        round: u64,
        enemies_left_to_spawn: u64,
        total_kills: u64,
        kills_this_round: u64,
        end_of_round: bool,
        time_to_next_round: f32,
        money: u64,
    },
    rng: std.rand.DefaultPrng,
};

fn new_state() State {
    var state = State{
        .keyboard = [_]State.InputState{.up} ** 348,
        .mouse = [_]State.InputState{.up} ** 7,
        .mouse_screen_position = vscaler(0),
        .mouse_world_position = vscaler(0),
        .time_since_start = 0,
        .tick_timer = 0,
        .update_time_nanoseconds = 0,
        .physics_time_nanoseconds = 0,
        .draw_time_nanoseconds = 0,
        .camera = .{
            .target = .{
                .x = 0,
                .y = 0,
            },
            .offset = .{.x = WINDOW_WIDTH * 0.5, .y = WINDOW_HEIGHT * 0.5},
            .rotation = 0,
            .zoom = 1,
        },
        .entities = std.BoundedArray(Entity, MAX_ENTITIES).init(0) catch unreachable,
        .level = .{
            .round = 1,
            .enemies_left_to_spawn = get_enemy_count_for_round(1),
            .total_kills = 0,
            .kills_this_round = 0,
            .end_of_round = false,
            .time_to_next_round = 0,
            .money = 0,
        },
        .rng = std.rand.DefaultPrng.init(123456),
    };

    // zero init all entities
    for(state.entities.slice()) |*entity| {
        entity.* = std.mem.zeroes(Entity);
    }

    return state;
}

fn run(state: *State) void {
    while (!raylib.WindowShouldClose()) {
        const delta_time = raylib.GetFrameTime();
        state.tick_timer += delta_time; 
        state.time_since_start += delta_time;

        if(state.tick_timer >= TICK_RATE) {
            input(state);

            const zoom_rate = 1;

            if(key(state, raylib.KEY_UP) == .down) {
                state.camera.zoom += zoom_rate;
            }

            if(key(state, raylib.KEY_DOWN) == .down) {
                state.camera.zoom -= zoom_rate;
            }

            state.camera.zoom = std.math.clamp(state.camera.zoom, 1, 10);

            var update_timer = std.time.Timer.start() catch unreachable;
            update_level(state);
            update_entites(state);
            state.update_time_nanoseconds = update_timer.read();

            state.tick_timer = 0;
        }


        var physics_timer = std.time.Timer.start() catch unreachable;
        physics(state, delta_time);
        state.physics_time_nanoseconds = physics_timer.read();

        var draw_timer = std.time.Timer.start() catch unreachable;
        draw(state, delta_time);
        state.draw_time_nanoseconds = draw_timer.read();
    }

    raylib.CloseWindow();
}

/////////////////////////////////////////////////////////////////////////
///                         @input
/////////////////////////////////////////////////////////////////////////
fn input(state: *State) void {
    for(&state.keyboard, 0..) |_, i| {
        if(raylib.IsKeyDown(@intCast(i))) {
            state.keyboard[i] = switch (state.keyboard[i]) {
                .up, .released => .down,
                .down, .pressing => .pressing
            };
        } else {
            state.keyboard[i] = switch (state.keyboard[i]) {
                .up, .released => .up,
                .down, .pressing => .released
            };
        }
    }

    for(&state.mouse, 0..) |_, i| {
        if(raylib.IsMouseButtonDown(@intCast(i))) {
            state.mouse[i] = switch (state.mouse[i]) {
                .up, .released => .down,
                .down, .pressing => .pressing
            };
        } else {
            state.mouse[i] = switch (state.mouse[i]) {
                .up, .released => .up,
                .down, .pressing => .released
            };
        }
    }

    const mouse_screen_position = raylib.GetMousePosition();
    const mouse_world_position = raylib.GetScreenToWorld2D(mouse_screen_position, state.camera);

    state.mouse_screen_position = V2f{mouse_screen_position.x, mouse_screen_position.y};
    state.mouse_world_position = V2f{mouse_world_position.x, mouse_world_position.y};
}

/////////////////////////////////////////////////////////////////////////
///                         @update
/////////////////////////////////////////////////////////////////////////
fn update_entites(state: *State) void {
    for(0..state.entities.len) |i| {
        var entity: *Entity = &state.entities.slice()[i];

        // tick any delays or timers that are going on in this entity
        if(entity.attacking_cooldown > 0) {
            entity.attacking_cooldown -= TICK_RATE;

            if(entity.attacking_cooldown <= 0) {
                entity.attacking_cooldown = 0;
            }
        }

        if(entity.health_regen_cooldown > 0) {
            entity.health_regen_cooldown -= TICK_RATE;

            if(entity.health_regen_cooldown <= 0) {
                entity.health_regen_cooldown = 0;
            }
        }

        if(entity.spawner_cooldown > 0) {
            entity.spawner_cooldown -= TICK_RATE;

            if(entity.spawner_cooldown <= 0) {
                entity.spawner_cooldown = 0;
            }
        }

        player: {
            if(!entiity_has_flag(entity, flag_player)) {
                break :player;
            } 

            if(key(state, raylib.KEY_W) == .pressing) {
                entity.velocity[1] = -PLAYER_SPEED;
            }
 
            if(key(state, raylib.KEY_A) == .pressing) {
                entity.velocity[0] = -PLAYER_SPEED;
            }
     
            if(key(state, raylib.KEY_S) == .pressing) {
                entity.velocity[1] = PLAYER_SPEED;
            }
     
            if(key(state, raylib.KEY_D) == .pressing) {
                entity.velocity[0] = PLAYER_SPEED;
            }

            if(key(state, raylib.KEY_R) == .down) {
                if(entity.magazine_ammo != entity.weapon.magazine_size()) {
                    entiity_set_flag(entity, flag_is_reloading);
                    entity.reload_cooldown = entity.weapon.reload_cooldown();
                }
            }

            if(
                key(state, raylib.KEY_SPACE) == .pressing and 
                entity.attacking_cooldown == 0 and
                entity.magazine_ammo > 0 and
                !entiity_has_flag(entity, flag_is_reloading)
            ) {
                entity.attacking_cooldown = entity.weapon.firing_cooldown();
                entity.magazine_ammo -= 1;

                const projectile_speed = vscaler(800);
                var direction_vector =  state.mouse_world_position - entity.position;
                direction_vector = vnormalise(direction_vector) * projectile_speed;

                _ = create_entity(state, Entity{
                    .flags = flag_projectile | flag_has_trigger_hitbox,
                    .position = entity.position,
                    .velocity = direction_vector,
                    .size = .{10, 10},
                    .texture= .black,
                });
            }

            if(key(state, raylib.KEY_E) == .down) {
                const interact_shape = entity.size + vscaler(40);
                var iter = new_box_collision_iterator(entity.position, interact_shape);
    
                while(iter.next(state)) |other| {
                    if(entity.id == other.id) {
                        continue;
                    }

                    if(entiity_has_flag(other, flag_is_ammo_crate) and state.level.money >= 1000) {
                        entity.magazine_ammo = entity.weapon.magazine_size();
                        entity.reserve_ammo = entity.magazine_ammo * entity.weapon.magazine_count();
                        state.level.money -= 1000;
                    }
                }
            }
        }

        ai: {
            if(!entiity_has_flag(entity, flag_ai)) {
                break :ai;
            }

            if(entity.target == null) {
                if(get_entity_with_flag(state, flag_player)) |player| {
                    entity.target = player.id;
                }
            }

            // maybe the target was deleted
            if(entity.target == null) {
                break :ai;
            }

            const target_entity = get_entity_with_flag(state, flag_player) orelse break :ai;
            const delta_vector = target_entity.position - entity.position;

            if(vlength(delta_vector) > 55) {
                entity.velocity = 
                    vnormalise(delta_vector) * 
                    vscaler(ENEMY_SPEED) * 
                    vscaler(get_enemy_speed_multiplier_for_round(state.level.round));

                break :ai;
            }

            // attack target
            if(entity.attacking_cooldown > 0) {
                break :ai; 
            }

            // ai needs to slow down to attack
            if(vlength(entity.velocity) > 35) {
                break :ai; 
            }

            entity.attacking_cooldown = 2;

            const attack_shape = get_enemy_attack_box_size(entity);
            var iter = new_box_collision_iterator(entity.position, attack_shape);

            while(iter.next(state)) |other| {
                if(entity.id == other.id or other.id != target_entity.id) {
                    continue;
                }

                // maybe this will change but right now assume it is true
                std.debug.assert(entiity_has_flag(other, flag_has_health));
                
                entity_take_damage(other, 35);
            }
        }

        projectile: {
            if(!entiity_has_flag(entity, flag_projectile)) {
                break :projectile;
            }

            if(state.time_since_start - entity.time_created > 2) {
                entiity_set_flag(entity, flag_to_be_deleted);
            } 

            if(entity.penetration_count >= 3) {
                entiity_set_flag(entity, flag_to_be_deleted);
            }
        }

        health: {
            if (!entiity_has_flag(entity, flag_has_health)) {
                break :health;
            }

            std.debug.assert(entity.health <= entity.max_health);

            if(entity.health == 0) {
                entiity_set_flag(entity, flag_to_be_deleted);
            }

            // health regen
            if(entity.health_regen_rate > 0 and entity.health_regen_cooldown == 0) {
                entity.health += @intFromFloat(TICK_RATE * @as(f32, @floatFromInt(entity.health_regen_rate)));
                
                if(entity.health >= entity.max_health) {
                    entity.health = entity.max_health;
                }
            }
        } 

        spawner: {
            if(!entiity_has_flag(entity, flag_spawner)) {
                break :spawner;
            }

            if(entity.spawner_cooldown == 0 and state.level.enemies_left_to_spawn > 0) {
                const f = state.rng.random().float(f64);

                if(f < 0.015) {
                    _ = create_basic_enemy(state, entity.position);
                    entity.spawner_cooldown = get_spawner_cooldown_for_round(state.level.round);
                    state.level.enemies_left_to_spawn -= 1;
                }
            }
        }

        weapon: {
            if(!entiity_has_flag(entity, flag_has_weapon)) {
                break :weapon;
            }

            if(entity.magazine_ammo == 0 and entity.reserve_ammo > 0 and !entiity_has_flag(entity, flag_is_reloading)) {
                entiity_set_flag(entity, flag_is_reloading);
                entity.reload_cooldown = entity.weapon.reload_cooldown();
            }

            if(entity.reload_cooldown > 0) {
                entity.reload_cooldown -= TICK_RATE;
    
                if(entity.reload_cooldown <= 0) {
                    entity.reload_cooldown = 0;

                    const space_left_in_mag = entity.weapon.magazine_size() - entity.magazine_ammo;
                    const ammo_added_to_mag = if (space_left_in_mag > entity.reserve_ammo) entity.reserve_ammo else space_left_in_mag;
                    entity.reserve_ammo -= ammo_added_to_mag;
                    entity.magazine_ammo += ammo_added_to_mag;
                    entiity_unset_flag(entity, flag_is_reloading);
                }
            }
        }
    }

    { // delete entities marked for deletion
        var index: usize = 0;
        while(index < state.entities.len) {
            if(entiity_has_flag(&state.entities.slice()[index], flag_to_be_deleted))  {
                const entity = state.entities.swapRemove(index);
                if(entiity_has_flag(&entity, flag_ai)) {
                    state.level.total_kills += 1;
                    state.level.kills_this_round += 1;
                }
            } else {
                index += 1;
            }
        }
    }
}

fn update_level(state: *State) void {
    // keep ticking down end of round timer
    if(state.level.end_of_round) {
        if(state.level.time_to_next_round > 0) {
            state.level.time_to_next_round -= TICK_RATE; 
        }
    }

    // start new round
    if(state.level.time_to_next_round < 0) {
        state.level.time_to_next_round = 0;
        state.level.end_of_round = false;
        state.level.round += 1;
        state.level.enemies_left_to_spawn = get_enemy_count_for_round(state.level.round);
        state.level.kills_this_round = 0;
    }

    // end the round
    std.debug.assert(get_enemy_count_for_round(state.level.round) >= state.level.kills_this_round);
    if(get_enemy_count_for_round(state.level.round) - state.level.kills_this_round == 0 and !state.level.end_of_round) {
        state.level.end_of_round = true;
        state.level.time_to_next_round = 5;
    }
}


/////////////////////////////////////////////////////////////////////////
///                         @draw
/////////////////////////////////////////////////////////////////////////
fn draw(state: *const State, delta_time: f32) void {
    raylib.BeginDrawing();
    raylib.ClearBackground(raylib.ColorBrightness(raylib.WHITE, -0.2));
    raylib.BeginMode2D(state.camera);

    { // rendering in world space
        // drawing of the entities, first layer drawing 
        for(0..state.entities.len) |i| {
            const entity: *const Entity = &state.entities.slice()[i];

            if(entity.texture != .none) {
                var color = switch (entity.texture) {
                    .none => unreachable,
                    .blue => raylib.ColorBrightness(raylib.BLUE, -0.3),
                    .red => raylib.ColorBrightness(raylib.RED, -0.3),
                    .black => raylib.BLACK,
                    .yellow => raylib.ColorBrightness(raylib.RED, -0.3),
                    .green => raylib.ColorBrightness(raylib.RED, -0.3),
                    .pink => raylib.ColorBrightness(raylib.RED, -0.3),
                    .brown => raylib.BROWN,
                };

                if(entiity_has_flag(entity, flag_is_reloading)) {
                    color = raylib.ORANGE;
                }
                
                render.rectangle(entity.position, entity.size, color);
            }
    
            if(DEBUG_DRAW_CENTRE_POINTS) {
                render.circle(entity.position, @reduce(.Add, entity.size) * 0.03, raylib.YELLOW);
            }

            if(DEBUG_DRAW_MOUSE_DIRECTION_ARROWS) {
                const arrow_length = vscaler(35);
                var direction_vector =  state.mouse_world_position - entity.position;
                direction_vector = vnormalise(direction_vector);
                direction_vector *= arrow_length;

                render.line(entity.position, entity.position + direction_vector, 2, raylib.PURPLE);
            }

            if(DEBUG_DRAW_ENEMY_ATTACK_BOXES and entiity_has_flag(entity, flag_ai)) {
                if (entity.attacking_cooldown > 0) {
                    const box_size = get_enemy_attack_box_size(entity);
                    const fade = if (entity.attacking_cooldown > 2) 0.2 else entity.attacking_cooldown * 0.1;

                    render.rectangle(entity.position, box_size, raylib.Fade(raylib.GREEN, fade));
                }
            }
        }

        // second layer for things like icons you want to sit on top
        // of the entities in the scene
        for(state.entities.slice()) |*entity| {
            if(entiity_has_flag(entity, flag_has_health) and entity.health < entity.max_health) {
                const bar_size = V2f{entity.size[0] + (entity.size[0] * 0.3), 8};
                const padding = 15;

                render.rectangle(
                    entity.position - V2f{0, (entity.size[1] * 0.5) + padding}, 
                    bar_size, 
                    raylib.ColorBrightness(raylib.RED, -0.5)
                );
                
                render.progress_bar_horizontal(
                    entity.position - V2f{0, (entity.size[1] * 0.5) + padding}, 
                    bar_size, 
                    raylib.RED, 
                    entity.health, 
                    entity.max_health
                );
            }
        }
    }

    raylib.EndMode2D();

    { // rendering in screen space (ui :( )
        var string_format_buffer = [_]u8{0} ** 256;

        // performance text
        {
            const ut_milliseconds = @as(f64, @floatFromInt(state.update_time_nanoseconds)) / 1_000_000;
            const pt_milliseconds = @as(f64, @floatFromInt(state.physics_time_nanoseconds)) / 1_000_000;
            const dt_milliseconds = @as(f64, @floatFromInt(state.draw_time_nanoseconds)) / 1_000_00;

            const string = std.fmt.bufPrintZ(
                &string_format_buffer, 
                "fps: {d:<8.4}, u: {d:<8.4}, p: {d:<8.4}, d-1: {d:<8.4}, e: {d}", 
                .{1 / delta_time, ut_milliseconds, pt_milliseconds, dt_milliseconds, state.entities.slice().len}
            ) catch unreachable;

            render.text(string, V2f{WINDOW_WIDTH - 240, WINDOW_HEIGHT - 20}, 16, raylib.WHITE);
        }  

        // game info text
        {
            const string = std.fmt.bufPrintZ(
                &string_format_buffer, 
                "round: {:<3}   kills: {:<4} remaining: {:<4}", 
                .{state.level.round, state.level.total_kills, get_enemy_count_for_round(state.level.round) - state.level.kills_this_round}
            ) catch unreachable;


            const color = if(state.level.end_of_round) raylib.WHITE else raylib.RED;
            render.text(string, V2f{WINDOW_WIDTH * 0.5, 20}, 30, color);
        }

        weapon_info_text: {
            const player = get_entity_with_flag(state, flag_player) orelse break :weapon_info_text;
            std.debug.assert(entiity_has_flag(player, flag_has_weapon));

            const string = std.fmt.bufPrintZ(
                &string_format_buffer, 
                "{s}: {}/{}    money: {}", 
                .{player.weapon.display_name(), player.magazine_ammo, player.reserve_ammo, state.level.money}
            ) catch unreachable;

            const color = if(entiity_has_flag(player, flag_is_reloading)) raylib.ORANGE else raylib.BLUE;
            render.text(string, V2f{200, WINDOW_HEIGHT - 20}, 30, color);
        }
    } 

    raylib.EndDrawing();
}

/////////////////////////////////////////////////////////////////////////
///                         @physics
/////////////////////////////////////////////////////////////////////////
fn physics(state: *State, delta_time: f32) void {
    if(delta_time == 0) return;

    const drag = 500;

    for(state.entities.slice()) |*entity| {
        // saving this before anything so we know *when* a collision
        // occurs not just if it is occuring
        const entity_start_position = entity.position;

        { // move camera
            if(entiity_has_flag(entity, flag_player)) {
                state.camera.target = .{
                    .x = entity.position[0],
                    .y = entity.position[1],
                };
            }
        }

        { // movement physics
            entity.position += entity.velocity * vscaler(delta_time);

            if(!entiity_has_flag(entity, flag_projectile)) {
                entity.velocity -= vnormalise(entity.velocity) * vscaler(drag) * vscaler(delta_time);
            }
        }

        { // collisions physics
            if(!entiity_has_flag(entity, flag_has_solid_hitbox | flag_has_trigger_hitbox)) {
                continue;
            }

            // this makes sure when we are checking for static 
            // it will always be the *other* entity below
            if(entiity_has_flag(entity, flag_is_static)) {
                continue;
            }
    
            for(state.entities.slice()) |*other| {
                if(@intFromPtr(entity) == @intFromPtr(other)) {
                    continue;
                }
    
                if(!entiity_has_flag(other, flag_has_solid_hitbox)) {
                    continue;
                }
    
                const distance_vector = other.position - entity.position;
                const absolute_distance_vector = @abs(distance_vector); // we use absolute for collision detection because which side does matter
                const distance_for_collision = (entity.size + other.size) * vscaler(0.5);
    
                if (
                    !(distance_for_collision[0] >= absolute_distance_vector[0] and
                        distance_for_collision[1] >= absolute_distance_vector[1])
                ) {
                    continue;
                }
    
                // enforce collision on bounding boxes when both entities 
                // have solid hitboxes
                if(entiity_has_flag(entity, flag_has_solid_hitbox)) {
                    const overlap_amount = distance_for_collision - @abs(distance_vector);
                    const other_static = entiity_has_flag(other, flag_is_static);
     
                    // if there is an overlap then measure on which axis has less 
                    // overlap and equally move both entities by that amount away
                    // from each other
                    //
                    // other could be static so if it is only move entity
                    // by the full overlap instead of sharing it
                    if(overlap_amount[0] < overlap_amount[1]) {
                        const x_push_amount = if(other_static) overlap_amount[0] else overlap_amount[0] * 0.5;

                        entity.position[0] -= std.math.sign(distance_vector[0]) * x_push_amount;

                        if(!other_static) {
                            other.position[0] += std.math.sign(distance_vector[0]) * x_push_amount;
                        }
                    } else {
                        const y_push_amount = if(other_static) overlap_amount[1] else overlap_amount[1] * 0.5;

                        entity.position[1] -= std.math.sign(distance_vector[1]) * y_push_amount;
                        
                        if(!other_static) {
                            other.position[1] += std.math.sign(distance_vector[1]) * y_push_amount;
                        }
                    }
                }
    
                // enforce trigger detection on bounding boxes when current
                // entity is a trigger and the other is a solid
                if(entiity_has_flag(entity, flag_has_trigger_hitbox)) {
                    // we want to trigger the entity when a collision happens
                    // but only if it is just happened this frame
                    //
                    // to check if this is a new collision you do another 
                    // collision detection based of off the beginging
                    // position of the entity this frame, which allows to 
                    // check if any positional changes that happened this frame
                    // has caused a new collision to occur
                    const starting_distance_vector = other.position - entity_start_position;
                    const starting_absolute_distance_vector = @abs(starting_distance_vector);
     
                    if (
                        !(distance_for_collision[0] >= starting_absolute_distance_vector[0] and
                            distance_for_collision[1] >= starting_absolute_distance_vector[1])
                    ) {
                        on_trigger_collided_start(state, entity, other);
                    }
                }
            }
        }
    }
}

const BoxCollisionIterator = struct {
    const Self = @This();

    index: usize,
    position: V2f,
    size: V2f,

    fn next(self: *Self, state: *State) ?*Entity {
        const index = self.index;
        for (state.entities.slice()[index..]) |*entity| {
            self.index += 1;

            const distance_vector = entity.position - self.position;
            const absolute_distance_vector = @abs(distance_vector); // we use absolute for collision detection because which side does matter
            const distance_for_collision = (self.size + entity.size) * vscaler(0.5);
 
            if (
                distance_for_collision[0] >= absolute_distance_vector[0] and
                distance_for_collision[1] >= absolute_distance_vector[1]
            ) {
                return entity;
            }
        }

        return null;
    }
};

fn new_box_collision_iterator(position: V2f, size: V2f) BoxCollisionIterator {
    return BoxCollisionIterator{
        .index = 0,
        .position = position,
        .size = size
    };
}

/////////////////////////////////////////////////////////////////////////
///                         @event
/////////////////////////////////////////////////////////////////////////
fn on_trigger_collided_start(state: *State, trigger_entity: *Entity, collided_entity: *Entity) void {
    if(entiity_has_flag(trigger_entity, flag_projectile)) {
        if(entiity_has_flag(collided_entity, flag_player) or !entiity_has_flag(collided_entity, flag_has_health)) {
            return;
        }

        entity_take_damage(collided_entity, 20);
        trigger_entity.penetration_count += 1;

        if(collided_entity.health == 0) {
            state.level.money += 100; 
        } else {
            state.level.money += 10; 
        }
    }
}

/////////////////////////////////////////////////////////////////////////
///                         @texture
/////////////////////////////////////////////////////////////////////////
const TextureHandle = enum(u8) {
    none,
    blue,
    red,
    black,
    yellow,
    green,
    pink,
    brown
};

/////////////////////////////////////////////////////////////////////////
///                         @entity
/////////////////////////////////////////////////////////////////////////
const Entity = struct {
    // meta
    id: EntityID                = 0,
    time_created: f64           = 0,
    flags: u64                  = 0,

    // physics
    position: V2f              = .{0, 0},
    velocity: V2f              = .{0, 0},
    size: V2f                  = .{0, 0},

    // rendering
    texture: TextureHandle      = .none,

    // gameplay
    attacking_cooldown: f32     = 0,

    // gameplay: projectile
    penetration_count: u32      = 0,

    // gameplay: health
    health: u64                 = 0,
    max_health: u64             = 0,
    health_regen_rate: u32      = 0,
    health_regen_cooldown: f32  = 0,

    // gameplay: ai
    target: ?EntityID           = 0,

    // gameplay: spawner
    spawner_cooldown: f32       = 0,

    // gameplay: weapon
    weapon: Weapon              = .none,
    magazine_ammo: u16          = 0,
    reserve_ammo: u16           = 0,
    reload_cooldown: f32        = 0,

    // gameplay: door
    door_cost: u32              = 0 
};

const EntityID = u32;
const EntityFlag = u64;

const flag_none                     :EntityFlag = 0;
const flag_player                   :EntityFlag = 1;
const flag_ai                       :EntityFlag = 1 << 1;
const flag_spawner                  :EntityFlag = 1 << 2;
const flag_projectile               :EntityFlag = 1 << 3;
const flag_to_be_deleted            :EntityFlag = 1 << 4;
const flag_has_health               :EntityFlag = 1 << 5;
const flag_has_solid_hitbox         :EntityFlag = 1 << 6;
const flag_has_trigger_hitbox       :EntityFlag = 1 << 7;
const flag_is_static                :EntityFlag = 1 << 8;
const flag_has_weapon               :EntityFlag = 1 << 9;
const flag_is_reloading             :EntityFlag = 1 << 10;
const flag_is_ammo_crate            :EntityFlag = 1 << 11;
const flag_is_door                  :EntityFlag = 1 << 12;

fn create_entity(state: *State, entity: Entity) *Entity {
    const Static = struct {
        var id: u32 = 1;
    }; 

    const entity_ptr =  state.entities.addOne() catch unreachable;

    entity_ptr.* = entity;
    entity_ptr.id = Static.id;
    entity_ptr.time_created = state.time_since_start;

    Static.id += 1;

    { // basic logic checks when creating entities go here
        std.debug.assert(entity_ptr.health <= entity_ptr.max_health);

        if(entiity_has_flag(entity_ptr, flag_is_static)) {
            std.debug.assert(entiity_has_flag(entity_ptr, flag_has_solid_hitbox));
        }

        if(entiity_has_flag(entity_ptr, flag_is_door)) {
            std.debug.assert(entity_ptr.door_cost > 0);
        }
    }

    return entity_ptr;
}

fn entity_take_damage(entity: *Entity, damage: u64) void {
    entity.health -= if(damage > entity.health) entity.health else damage;

    if(entity.health == 0) {
        entiity_set_flag(entity, flag_to_be_deleted);
        return;
    }

    if(entiity_has_flag(entity, flag_player)) {
        entity.health_regen_cooldown = 2;
    }
}

fn create_player(state: *State, position: V2f) *Entity {
    return create_entity(state, .{
        .flags = flag_player | flag_has_solid_hitbox | flag_has_health | flag_has_weapon,
        .position = position,
        .size = .{50, 50},
        .texture = .blue,
        .health = 100,
        .max_health = 100,
        .health_regen_rate = 40,
        .weapon = .m4,
        .magazine_ammo = Weapon.m4.magazine_size(),
        .reserve_ammo =  Weapon.m4.magazine_size() * Weapon.m4.magazine_count()
    });
}

fn create_ammo_crate(state: *State, position: V2f) *Entity {
    return create_entity(state, .{
        .flags = flag_has_solid_hitbox | flag_is_static | flag_is_ammo_crate,
        .position = position,
        .size = .{25, 25},
        .texture = .brown,
    });
}

fn create_door(state: *State, position: V2f, size: V2f, cost: u32) *Entity {
    return create_entity(state, .{
        .flags = flag_has_solid_hitbox | flag_is_static | flag_is_door,
        .position = position,
        .size = size,
        .texture = .pink,
        .door_cost = cost
    });
}

fn create_basic_enemy(state: *State, position: V2f) *Entity {
    return create_entity(state, .{
        .flags = flag_has_health | flag_ai | flag_has_solid_hitbox,
        .position = position,
        .size = .{40, 40},
        .texture = .red,
        .health = 100,
        .max_health = 100,
    });
}

fn create_spawner(state: *State, position: V2f) *Entity {
    return create_entity(state, .{
        .flags = flag_spawner,
        .position = position,
        .size = vscaler(10),
        .texture = .pink,
    });
}

fn create_wall(state: *State, position: V2f, size: V2f) *Entity {
    return create_entity(state, .{
        .flags = flag_has_solid_hitbox | flag_is_static,
        .position = position,
        .size = size,
        .texture = .black,
    });
}

fn get_entity_with_flag(state: *const State, flag: EntityFlag) ?*const Entity {
    for(state.entities.slice()) |*entity| {
        if(entiity_has_flag(entity, flag)) {
            return entity;
        }
    }

    return null;
}

inline fn entiity_set_flag(entity: *Entity, flag: EntityFlag) void {
    entity.flags |= flag;
}

inline fn entiity_unset_flag(entity: *Entity, flag: EntityFlag) void {
    entity.flags ^= flag;
}

inline fn entiity_has_flag(entity: *const Entity, flag: EntityFlag) bool {
    return !(entity.flags & flag == 0);
}

fn get_enemy_attack_box_size(entity: *const Entity) V2f {
    return entity.size + vscaler(20);
}

/////////////////////////////////////////////////////////////////////////
///                         @weapon
/////////////////////////////////////////////////////////////////////////
const Weapon = enum {
    const Self = @This();

    none,
    pistol,
    m4,

    fn damage(self: Self) u64 {
        return switch (self) {
            .none => 0,
            .pistol => 15,
            .m4 => 30,
        };
    }

    fn magazine_size(self: Self) u16 {
        return switch (self) {
            .none => 0,
            .pistol => 12,
            .m4 => 30,
        };
    }

    fn magazine_count(self: Self) u16 {
        return switch (self) {
            .none => 0,
            .pistol => 4,
            .m4 => 5,
        };
    }

    fn reload_cooldown(self: Self) f32 {
        return switch (self) {
            .none => 0,
            .pistol => 1.5,
            .m4 => 3,
        };
    }

    fn firing_cooldown(self: Self) f32 {
        return switch (self) {
            .none => 0,
            .pistol => 0.5,
            .m4 => 0.1,
        };
    }

    fn display_name(self: Self) []const u8 {
        return switch (self) {
            .none => "<empty>",
            .pistol => "pistol",
            .m4 => "m4",
        };
    }
};

/////////////////////////////////////////////////////////////////////////
///                         @vector
/////////////////////////////////////////////////////////////////////////
pub const V2f = @Vector(2, f32);
pub const V2i = @Vector(2, i32);

inline fn vscaler(scaler: f32) V2f {
    return @splat(scaler);
}

inline fn vlength(vector: V2f) f32 {
    return @sqrt(@reduce(.Add, vector * vector));
}

fn vnormalise(vector: V2f) V2f {
    const length = vlength(vector);
    if(length == 0) {
        return V2f{0, 0};
    }

    return vector / V2f{length, length};
}

/////////////////////////////////////////////////////////////////////////
///                         @random
/////////////////////////////////////////////////////////////////////////
fn init_raylib() void {
    raylib.InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "shooting game");
    raylib.SetTargetFPS(120);
}

pub fn key(state: *const State, k: c_int) State.InputState {
    return state.keyboard[@intCast(k)];
}

pub fn mouse(state: *const State, m: c_int) State.InputState {
    return state.mouse[@intCast(m)];
}

fn get_enemy_count_for_round(round: u64) u64 {
    if(true) {
        const amount = 7 + std.math.pow(f64, @floatFromInt(round), 1.4);
        return @as(u64, @intFromFloat(amount));
    } else {
        return 1;
    }
}

fn get_enemy_speed_multiplier_for_round(round: u64) f32 {
    return switch (round) {
        1 => 0.5,
        2, 3 => 0.65,
        4...7 => 0.8,
        8...10 => 0.9,
        else => 1.0
    };
}

fn get_spawner_cooldown_for_round(round: u64) f32 {
    return 1.0 + switch (round) {
        1, 2 => 3,
        3, 4 => 2,
        5, 6 => 1,
        else => @as(f32, 0)
    };
}

fn load_texture(relative_texture_path: []const u8) !raylib.Texture {
    var base_path_buffer = std.mem.zeroes([std.fs.MAX_PATH_BYTES]u8);
    
    const cwd_dir = std.fs.cwd();
    const base_path = try cwd_dir.realpath(".", base_path_buffer[0..]);

    var texture_path_buffer = std.mem.zeroes([std.fs.MAX_PATH_BYTES]u8);
    const texture_path = try std.fmt.bufPrint(texture_path_buffer[0..], "{s}/resources/textures/{s}", .{base_path, relative_texture_path});

    const texture = raylib.LoadTexture(&texture_path[0]);
    if(texture.id <= 0) {
        std.debug.panic("unable to load texture with path {s}\n", .{texture_path});
    }

    return texture;
}

const DualTileGrid = struct {
    const Self = @This();

    const neighbour_to_tile_index: [16]struct {
        neighbours: [4]bool,
        index: usize
    } = .{
        .{.neighbours = .{true, true, true, true}, .index = 6},
        .{.neighbours = .{false, false, false, true}, .index = 13},
        .{.neighbours = .{false, false, true, false}, .index = 0},
        .{.neighbours = .{false, true, false, false}, .index = 8},
        .{.neighbours = .{true, false, false, false}, .index = 15},
        .{.neighbours = .{false, true, false, true}, .index = 1},
        .{.neighbours = .{true, false, true, false}, .index = 11},
        .{.neighbours = .{false, false, true, true}, .index = 3},
        .{.neighbours = .{true, true, false, false}, .index = 9},
        .{.neighbours = .{false, true, true, true}, .index = 5},
        .{.neighbours = .{true, false, true, true}, .index = 2},
        .{.neighbours = .{true, true, false, true}, .index = 10},
        .{.neighbours = .{true, true, true, false}, .index = 7},
        .{.neighbours = .{false, true, true, false}, .index = 14},
        .{.neighbours = .{true, false, false, true}, .index = 4},
        .{.neighbours = .{false, false, false, false}, .index = 12},
    };

    const neighbour_coords: [4]V2i = .{
        V2i{0, 0},
        V2i{1, 0},
        V2i{0, 1},
        V2i{1, 1},
    };

    texture: raylib.Texture,
    grid : [8][8]bool,
    tile_draw_size: f32,

    fn get_display_tile_index(self: *const Self, position: V2i) usize {

        if(position[0] == 2 and position[1] == 1) {
            const x = 0; 
            _ = x; // autofix
        }

        const top_left_location = position - Self.neighbour_coords[3]; 
        const top_right_location = position - Self.neighbour_coords[2]; 
        const bottom_left_location = position - Self.neighbour_coords[1]; 
        const bottom_right_location = position - Self.neighbour_coords[0]; 

        const neighbour_values = [4]bool{
            self.get_grid_value(top_left_location),
            self.get_grid_value(top_right_location),
            self.get_grid_value(bottom_left_location),
            self.get_grid_value(bottom_right_location)
        };

        var tile_index: usize = 0;

        outer: for(&Self.neighbour_to_tile_index) |*neighbour_info| {
            for(neighbour_info.neighbours, 0..) |value, i| {
                if(value != neighbour_values[i]) {
                    continue :outer;
                }
            }

            tile_index = neighbour_info.index;
            break;
        }

        return tile_index;
    }

    fn get_grid_value(self: *const Self, position: V2i) bool {
        if(
            position[0] < 0 or 
            position[0] >= self.grid.len or
            position[1] < 0 or 
            position[1] >= self.grid[0].len

        ) {
            return true;
        }

        const x = @as(usize, @intCast(position[0]));
        const y = @as(usize, @intCast(position[1]));

        return self.grid[y][x];
    }

    fn index_to_texture_coords(self: *const Self, index: usize) V2f {
        _ = self; // autofix
        return .{
            @as(f32, @floatFromInt(@mod(index, 4))),
            @as(f32, @floatFromInt(@divFloor(index, 4)))
        };
    }

    fn toggle_tile(self: *Self, position: V2i) void {
        const x: usize = @intCast(position[0]);
        const y: usize = @intCast(position[1]);

        self.grid[y][x] = !self.grid[y][x];
    }

    fn draw(self: *const Self) void {
        const tile_texture_size = 16;
        const y_len = self.grid.len;
        const x_len = self.grid[0].len;

        for(0..y_len + 1) |y| {
            for(0..x_len + 1) |x| {
                const grid_position = V2i{@intCast(x), @intCast(y)};
                const index = self.get_display_tile_index(grid_position);
                const texture_coords = self.index_to_texture_coords(index);
                const draw_position = V2f{@as(f32, @floatFromInt(x)) * self.tile_draw_size, @as(f32, @floatFromInt(y)) * self.tile_draw_size};

                render.texture(self.texture, .{
                    .position = draw_position,
                    .size = vscaler(self.tile_draw_size),
                    .source_position = V2f{texture_coords[0] * tile_texture_size, texture_coords[1] * tile_texture_size},
                    .source_size = V2f{tile_texture_size, tile_texture_size},
                });
            }   
        }

        // draw tile preview dots
        for(0..y_len) |y| {
            for(0..x_len) |x| {
                const draw_position = V2f{@as(f32, @floatFromInt(x)) * self.tile_draw_size, @as(f32, @floatFromInt(y)) * self.tile_draw_size};
                render.circle(draw_position + vscaler(self.tile_draw_size), 10, raylib.RED);
            }   
        }
    }
};

/////////////////////////////////////////////////////////////////////////
///                         @main
/////////////////////////////////////////////////////////////////////////
pub fn main() !void {
    init_raylib();
    var state = new_state();

    {
        _ = create_player(&state, vscaler(0));
        _ = create_ammo_crate(&state, V2f{385, 0});
    }

    {
        const spawner_inset_amount = 80;
        _ = create_spawner(&state, V2f{-(WINDOW_WIDTH * 0.5) + spawner_inset_amount, -(WINDOW_HEIGHT * 0.5) + spawner_inset_amount});
        _ = create_spawner(&state, V2f{WINDOW_WIDTH * 0.5 - spawner_inset_amount, -(WINDOW_HEIGHT * 0.5) + spawner_inset_amount});
        _ = create_spawner(&state, V2f{-(WINDOW_WIDTH * 0.5) + spawner_inset_amount, (WINDOW_HEIGHT * 0.5) - spawner_inset_amount});
        _ = create_spawner(&state, V2f{WINDOW_WIDTH * 0.5 - spawner_inset_amount, (WINDOW_HEIGHT * 0.5) - spawner_inset_amount});
    }

    {
        const wall_thickness = 40;
        _ = create_wall(&state, V2f{0, (-WINDOW_HEIGHT * 0.5) + wall_thickness * 0.5}, V2f{WINDOW_WIDTH, wall_thickness});
        _ = create_wall(&state, V2f{0, (WINDOW_HEIGHT * 0.5) - wall_thickness * 0.5}, V2f{WINDOW_WIDTH, wall_thickness});
        _ = create_wall(&state, V2f{(-WINDOW_WIDTH * 0.5) + wall_thickness * 0.5, 0}, V2f{wall_thickness, WINDOW_HEIGHT});
        _ = create_wall(&state, V2f{(WINDOW_WIDTH * 0.5) - wall_thickness * 0.5, 0}, V2f{wall_thickness, WINDOW_HEIGHT});

        // centre walls middle
        _ = create_wall(&state, V2f{0, -190}, V2f{400, 180});
        _ = create_wall(&state, V2f{0, 190}, V2f{400, 180});

        // centre walls side
        const side_wall_width = 200;
        const side_wall_height = 30;

        _ = create_wall(&state, V2f{(-WINDOW_WIDTH * 0.5) + (side_wall_width * 0.5), 0}, V2f{side_wall_width, side_wall_height});
        _ = create_wall(&state, V2f{(WINDOW_WIDTH * 0.5) - (side_wall_width * 0.5), 0}, V2f{side_wall_width, side_wall_height});
    }

    run(&state);
}
