const std = @import("std");

const raylib = @cImport(@cInclude("raylib.h"));

const render = @import("render.zig");

const MAX_ENTITIES = 100;

const PLAYER_SPEED  = 200;
const ENEMY_SPEED   = 150;

const WINDOW_WIDTH  = 1200;
const WINDOW_HEIGHT = 900;

const TICKS_PER_SECONDS: f64    = 20;
const TICK_RATE: f64            = 1.0 / TICKS_PER_SECONDS;

const DEBUG_DRAW_CENTRE_POINTS          = false;
const DEBUG_DRAW_MOUSE_DIRECTION_ARROWS = false;

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
    camera: raylib.Camera2D,
    entities: std.BoundedArray(Entity, MAX_ENTITIES),
    update_time_nanoseconds: u64,
    physics_time_nanoseconds: u64,
    draw_time_nanoseconds: u64,
};

fn new_state() State {
    var state = State{
        .keyboard = [_]State.InputState{.up} ** 348,
        .mouse = [_]State.InputState{.up} ** 7,
        .mouse_screen_position = v2_scaler(0),
        .mouse_world_position = v2_scaler(0),
        .time_since_start = 0,
        .tick_timer = 0,
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
        .update_time_nanoseconds = 0,
        .physics_time_nanoseconds = 0,
        .draw_time_nanoseconds = 0,
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

            var update_timer = std.time.Timer.start() catch unreachable;
            update(state);
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
fn update(state: *State) void {
    for(state.entities.slice()) |*entity| {
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

            if(key(state, raylib.KEY_SPACE) == .down) {
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

            if(entity.target != null) {
                const target_entity = get_entity_with_flag(state, flag_player) orelse break :ai;
                const delta_vector = target_entity.position - entity.position;
                const normalised = v2_normalise(delta_vector);

                if(v2_length(delta_vector) <= 50) {
                    entity.velocity = v2_scaler(0);
                    break :ai;
                }

                entity.velocity = normalised * v2_scaler(ENEMY_SPEED);
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
        } 

        spawner: {
            if(!entiity_has_flag(entity, flag_spawner)) {
                break :spawner;
            }

            const Static = struct {
                var spawn_timer: usize = 0;
            };

            // spawn every 3 seconds
            const spawn_delay_seconds = 1;
            Static.spawn_timer += 1;

            if(TICKS_PER_SECONDS * spawn_delay_seconds == Static.spawn_timer) {
                Static.spawn_timer = 0;

                _ = create_entity(state, .{
                    .flags = flag_has_health | flag_ai | flag_has_solid_hitbox,
                    .position = entity.position,
                    .size = .{25, 40},
                    .texture = .red,
                    .health = 100,
                    .max_health = 100,
                });       
            }
        }
    }

    { // delete entities marked for deletion
        var index: usize = 0;
        while(index < state.entities.len) {
            if(entiity_has_flag(&state.entities.slice()[index], flag_to_be_deleted))  {
                _ = state.entities.swapRemove(index);
            } else {
                index += 1;
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////
///                         @draw
/////////////////////////////////////////////////////////////////////////
fn draw(state: *const State, delta_time: f32) void {
    raylib.BeginDrawing();
    raylib.ClearBackground(raylib.GRAY);
    raylib.BeginMode2D(state.camera);

    { // rendering in world space
        // drawing of the entities
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

            // only draw health bar if it has health and there is actual damage
            if(entiity_has_flag(entity, flag_has_health) and entity.health < entity.max_health) {
                const bar_size = Vec2{5, entity.size[1]};
                const padding = 5;
                
                render.progress_bar_vertical(
                    entity.position + (entity.size * Vec2{0.5, -0.5}) + Vec2{padding, 0}, 
                    bar_size, 
                    raylib.RED, 
                    raylib.PINK, 
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

            render.text(fps_string, v2_scaler(20), 20, raylib.RED);
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

    // gameplay: health
    health: u64                 = 0,
    max_health: u64             = 0,

    // gameplay: ai target
    target: ?EntityID           = 0,
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
    raylib.SetTargetFPS(0);
}

pub fn key(state: *const State, k: c_int) State.InputState {
    return state.keyboard[@intCast(k)];
}

/////////////////////////////////////////////////////////////////////////
///                         @main
/////////////////////////////////////////////////////////////////////////
pub fn main() !void {
    init_raylib();
    var state = new_state();

    // player
    _ = create_entity(&state, .{
        .flags = flag_player | flag_has_solid_hitbox,
        .position = .{0, 0},
        .size = .{30, 50},
        .texture = .blue
    });

    if (true) {
        // spawner
        _ = create_entity(&state, .{
            .flags = flag_spawner,
            .position = .{200, 0},
            .size = .{15, 15},
            .texture = .yellow,
        }); 
    }

    { // spawn walls
        const wall_thickness = 40;
   
        // top
        _ = create_entity(&state, .{
            .flags = flag_has_solid_hitbox | flag_is_static,
            .position = .{0, (-WINDOW_HEIGHT * 0.5) + wall_thickness * 0.5},
            .size = .{WINDOW_WIDTH, wall_thickness},
            .texture = .blue,
        });

        // bottom
        _ = create_entity(&state, .{
            .flags = flag_has_solid_hitbox | flag_is_static,
            .position = .{0, (WINDOW_HEIGHT * 0.5) - wall_thickness * 0.5},
            .size = .{WINDOW_WIDTH, wall_thickness},
            .texture = .red,
        });

        // left
        _ = create_entity(&state, .{
            .flags = flag_has_solid_hitbox | flag_is_static,
            .position = .{(-WINDOW_WIDTH * 0.5) + wall_thickness * 0.5, 0},
            .size = .{wall_thickness, WINDOW_HEIGHT},
            .texture = .yellow,
        });

        // right
        _ = create_entity(&state, .{
            .flags = flag_has_solid_hitbox | flag_is_static,
            .position = .{(WINDOW_WIDTH * 0.5) - wall_thickness * 0.5, 0},
            .size = .{wall_thickness, WINDOW_HEIGHT},
            .texture = .green,
        });
    }

    run(&state);
}
