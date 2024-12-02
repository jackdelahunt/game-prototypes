const std = @import("std");

const raylib = @cImport(@cInclude("raylib.h"));

const render = @import("render.zig");

const MAX_ENTITIES = 100;

const PLAYER_SPEED  = 200;
const ENEMY_SPEED   = 100;

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
    mouse_screen_position: Vec2,
    mouse_world_position: Vec2,
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
        time_to_next_round: f32
    },
    rng: std.rand.DefaultPrng,
};

fn new_state() State {
    var state = State{
        .keyboard = [_]State.InputState{.up} ** 348,
        .mouse = [_]State.InputState{.up} ** 7,
        .mouse_screen_position = v2_scaler(0),
        .mouse_world_position = v2_scaler(0),
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

    state.mouse_screen_position = Vec2{mouse_screen_position.x, mouse_screen_position.y};
    state.mouse_world_position = Vec2{mouse_world_position.x, mouse_world_position.y};
}

/////////////////////////////////////////////////////////////////////////
///                         @update
/////////////////////////////////////////////////////////////////////////
fn update_entites(state: *State) void {
    for(0..state.entities.len) |i| {
        var entity = &state.entities.slice()[i];

        // tick any delays or timers that are going on in this entity
        if(entity.attacking_cooldown > 0) {
            entity.attacking_cooldown -= TICK_RATE;

            if(entity.attacking_cooldown < 0) {
                entity.attacking_cooldown = 0;
            }
        }

        if(entity.health_regen_cooldown > 0) {
            entity.health_regen_cooldown -= TICK_RATE;

            if(entity.health_regen_cooldown < 0) {
                entity.health_regen_cooldown = 0;
            }
        }

        if(entity.spawner_cooldown > 0) {
            entity.spawner_cooldown -= TICK_RATE;

            if(entity.spawner_cooldown < 0) {
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

            if(key(state, raylib.KEY_SPACE) == .pressing and entity.attacking_cooldown == 0) {
                entity.attacking_cooldown = 0.2;

                const projectile_speed = v2_scaler(800);
                var direction_vector =  state.mouse_world_position - entity.position;
                direction_vector = v2_normalise(direction_vector) * projectile_speed;

                _ = create_entity(state, Entity{
                    .flags = flag_projectile | flag_has_trigger_hitbox,
                    .position = entity.position,
                    .velocity = direction_vector,
                    .size = .{20, 20},
                    .texture= .black,
                });
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

            if(v2_length(delta_vector) > 55) {
                entity.velocity = 
                    v2_normalise(delta_vector) * 
                    v2_scaler(ENEMY_SPEED) * 
                    v2_scaler(get_enemy_speed_multiplier_for_round(state.level.round));

                break :ai;
            }

            // attack target
            if(entity.attacking_cooldown > 0) {
                break :ai; 
            }

            // ai needs to slow down to attack
            if(v2_length(entity.velocity) > 35) {
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

                if(f < 0.01) {
                    _ = create_basic_enemy(state, entity.position);
                    entity.spawner_cooldown = get_spawner_cooldown_for_round(state.level.round);
                    state.level.enemies_left_to_spawn -= 1;
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
        for(state.entities.slice()) |*entity| {
            switch (entity.texture) {
                .none => {},
                .blue => {
                    render.rectangle(entity.position, entity.size, raylib.ColorBrightness(raylib.BLUE, -0.3));
                },
                .red => {
                    render.rectangle(entity.position, entity.size, raylib.ColorBrightness(raylib.RED, -0.5));
                },
                .black => {
                    render.rectangle(entity.position, entity.size, raylib.BLACK);
                },
                .yellow => {
                    render.rectangle(entity.position, entity.size, raylib.ColorBrightness(raylib.YELLOW, -0.5));
                },
                .green => {
                    render.rectangle(entity.position, entity.size, raylib.ColorBrightness(raylib.GREEN, -0.5));
                },
                .pink => {
                    render.rectangle(entity.position, entity.size, raylib.ColorBrightness(raylib.PINK, -0.5));
                },
            }
    
            if(DEBUG_DRAW_CENTRE_POINTS) {
                render.circle(entity.position, @reduce(.Add, entity.size) * 0.03, raylib.YELLOW);
            }

            if(DEBUG_DRAW_MOUSE_DIRECTION_ARROWS) {
                const arrow_length = v2_scaler(35);
                var direction_vector =  state.mouse_world_position - entity.position;
                direction_vector = v2_normalise(direction_vector);
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
                const bar_size = Vec2{entity.size[0] + (entity.size[0] * 0.3), 8};
                const padding = 15;

                render.rectangle(
                    entity.position - Vec2{0, (entity.size[1] * 0.5) + padding}, 
                    bar_size, 
                    raylib.ColorBrightness(raylib.RED, -0.5)
                );
                
                render.progress_bar_horizontal(
                    entity.position - Vec2{0, (entity.size[1] * 0.5) + padding}, 
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
        // performance text
        {
            const ut_milliseconds = @as(f64, @floatFromInt(state.update_time_nanoseconds)) / 1_000_000;
            const pt_milliseconds = @as(f64, @floatFromInt(state.physics_time_nanoseconds)) / 1_000_000;
            const dt_milliseconds = @as(f64, @floatFromInt(state.draw_time_nanoseconds)) / 1_000_00;

            var buffer = [_]u8{0} ** 256;
            const fps_string = std.fmt.bufPrintZ(
                &buffer, 
                "fps: {d:<8.4}, u: {d:<8.4}, p: {d:<8.4}, d-1: {d:<8.4}, e: {d}", 
                .{1 / delta_time, ut_milliseconds, pt_milliseconds, dt_milliseconds, state.entities.slice().len}
            ) catch unreachable;

            render.text(fps_string, Vec2{WINDOW_WIDTH * 0.5, WINDOW_HEIGHT - 20}, 16, raylib.WHITE);
        }  

        // game info text
        {
            var buffer = [_]u8{0} ** 256;
            const string = std.fmt.bufPrintZ(
                &buffer, 
                "round: {:<3}   kills: {:<4} remaining: {:<4}", 
                .{state.level.round, state.level.total_kills, get_enemy_count_for_round(state.level.round) - state.level.kills_this_round}
            ) catch unreachable;

            render.text(string, Vec2{WINDOW_WIDTH * 0.5, 20}, 30, raylib.RED);
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

        { // movement physics
            entity.position += entity.velocity * v2_scaler(delta_time);

            if(!entiity_has_flag(entity, flag_projectile)) {
                entity.velocity -= v2_normalise(entity.velocity) * v2_scaler(drag) * v2_scaler(delta_time);
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
                const distance_for_collision = (entity.size + other.size) * v2_scaler(0.5);
    
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
                        on_trigger_collided_start(entity, other);
                    }
                }
            }
        }
    }
}

const BoxCollisionIterator = struct {
    const Self = @This();

    index: usize,
    position: Vec2,
    size: Vec2,

    fn next(self: *Self, state: *State) ?*Entity {
        const index = self.index;
        for (state.entities.slice()[index..]) |*entity| {
            self.index += 1;

            const distance_vector = entity.position - self.position;
            const absolute_distance_vector = @abs(distance_vector); // we use absolute for collision detection because which side does matter
            const distance_for_collision = (self.size + entity.size) * v2_scaler(0.5);
 
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

fn new_box_collision_iterator(position: Vec2, size: Vec2) BoxCollisionIterator {
    return BoxCollisionIterator{
        .index = 0,
        .position = position,
        .size = size
    };
}

/////////////////////////////////////////////////////////////////////////
///                         @event
/////////////////////////////////////////////////////////////////////////
fn on_trigger_collided_start(trigger_entity: *Entity, collided_entity: *Entity) void {
    std.debug.assert(entiity_has_flag(trigger_entity, flag_projectile));

    if(entiity_has_flag(collided_entity, flag_player) or !entiity_has_flag(collided_entity, flag_has_health)) {
        return;
    }

    const damage = 20;
    collided_entity.health = if(damage >= collided_entity.health) 0 else collided_entity.health - damage; // health is unsigned so need to be careful
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
    pink
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
    position: Vec2              = .{0, 0},
    velocity: Vec2              = .{0, 0},
    size: Vec2                  = .{0, 0},

    // rendering
    texture: TextureHandle      = .none,

    // gameplay
    attacking_cooldown: f32     = 0,

    // gameplay: health
    health: u64                 = 0,
    max_health: u64             = 0,
    health_regen_rate: u32      = 0,
    health_regen_cooldown: f32  = 0,

    // gameplay: ai
    target: ?EntityID           = 0,

    // gameplay: spawner
    spawner_cooldown: f32       = 0,
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

fn create_player(state: *State, position: Vec2) *Entity {
    return create_entity(state, .{
        .flags = flag_player | flag_has_solid_hitbox | flag_has_health,
        .position = position,
        .size = .{50, 50},
        .texture = .blue,
        .health = 100,
        .max_health = 100,
        .health_regen_rate = 40,
    });
}

fn create_basic_enemy(state: *State, position: Vec2) *Entity {
    return create_entity(state, .{
        .flags = flag_has_health | flag_ai | flag_has_solid_hitbox,
        .position = position,
        .size = .{40, 40},
        .texture = .red,
        .health = 100,
        .max_health = 100,
    });
}

fn create_spawner(state: *State, position: Vec2) *Entity {
    return create_entity(state, .{
        .flags = flag_spawner,
        .position = position,
        .size = v2_scaler(10),
        .texture = .pink,
    });
}

fn create_wall(state: *State, position: Vec2, size: Vec2) *Entity {
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

inline fn entiity_has_flag(entity: *const Entity, flag: EntityFlag) bool {
    return !(entity.flags & flag == 0);
}

fn get_enemy_attack_box_size(entity: *const Entity) Vec2 {
    return entity.size + v2_scaler(20);
}

/////////////////////////////////////////////////////////////////////////
///                         @vector
/////////////////////////////////////////////////////////////////////////
pub const Vec2 = @Vector(2, f32);

inline fn v2_scaler(scaler: f32) Vec2 {
    return @splat(scaler);
}

fn v2_length(vector: Vec2) f32 {
    return @sqrt(@reduce(.Add, vector * vector));
}

fn v2_normalise(vector: Vec2) Vec2 {
    const length = v2_length(vector);
    if(length == 0) {
        return Vec2{0, 0};
    }

    return vector / Vec2{length, length};
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

/////////////////////////////////////////////////////////////////////////
///                         @main
/////////////////////////////////////////////////////////////////////////
pub fn main() !void {
    init_raylib();
    var state = new_state();

    _ = create_player(&state, v2_scaler(0));

    {
        const spawner_inset_amount = 80;
        _ = create_spawner(&state, Vec2{-(WINDOW_WIDTH * 0.5) + spawner_inset_amount, -(WINDOW_HEIGHT * 0.5) + spawner_inset_amount});
        _ = create_spawner(&state, Vec2{WINDOW_WIDTH * 0.5 - spawner_inset_amount, -(WINDOW_HEIGHT * 0.5) + spawner_inset_amount});
        _ = create_spawner(&state, Vec2{-(WINDOW_WIDTH * 0.5) + spawner_inset_amount, (WINDOW_HEIGHT * 0.5) - spawner_inset_amount});
        _ = create_spawner(&state, Vec2{WINDOW_WIDTH * 0.5 - spawner_inset_amount, (WINDOW_HEIGHT * 0.5) - spawner_inset_amount});
    }

    {
        const wall_thickness = 40;
        _ = create_wall(&state, Vec2{0, (-WINDOW_HEIGHT * 0.5) + wall_thickness * 0.5}, Vec2{WINDOW_WIDTH, wall_thickness});
        _ = create_wall(&state, Vec2{0, (WINDOW_HEIGHT * 0.5) - wall_thickness * 0.5}, Vec2{WINDOW_WIDTH, wall_thickness});
        _ = create_wall(&state, Vec2{(-WINDOW_WIDTH * 0.5) + wall_thickness * 0.5, 0}, Vec2{wall_thickness, WINDOW_HEIGHT});
        _ = create_wall(&state, Vec2{(WINDOW_WIDTH * 0.5) - wall_thickness * 0.5, 0}, Vec2{wall_thickness, WINDOW_HEIGHT});

        // centre walls middle
        _ = create_wall(&state, Vec2{0, -190}, Vec2{400, 180});
        _ = create_wall(&state, Vec2{0, 190}, Vec2{400, 180});

        // centre walls side
        const side_wall_width = 200;
        const side_wall_height = 30;

        _ = create_wall(&state, Vec2{(-WINDOW_WIDTH * 0.5) + (side_wall_width * 0.5), 0}, Vec2{side_wall_width, side_wall_height});
        _ = create_wall(&state, Vec2{(WINDOW_WIDTH * 0.5) - (side_wall_width * 0.5), 0}, Vec2{side_wall_width, side_wall_height});
    }

    run(&state);
}
