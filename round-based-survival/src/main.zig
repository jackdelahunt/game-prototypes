const std = @import("std");
const log = std.log.scoped(.game);

const raylib = @cImport(@cInclude("raylib.h"));
const microui = @cImport(@cInclude("microui.h"));

const render = @import("base_layer/render.zig");
const encode = @import("base_layer/encode.zig");

const MAX_ENTITIES = 1024;

const PLAYER_SPEED  = 200;
const PLAYER_DASH_MULTIPLIER = 4;
const ENEMY_SPEED   = 100;
const PROJECTILE_SPEED = 550;

const PLAYER_REACH_SIZE = 110;
const PLAYER_HEALTH  = 100;

// const WINDOW_WIDTH  = 1000;
// const WINDOW_HEIGHT = 750;

const WINDOW_WIDTH  = 1500;
const WINDOW_HEIGHT = 1000;

const ROOM_WIDTH = 900;
const ROOM_HEIGHT = 800;
const WALLTHICKNESS = 20;
const DOOR_WIDTH = 350;

const AMMO_CRATE_COST = 750;
const RANDOM_BOX_COST = 1250;

const MAX_SPAWN_DISTANCE = ROOM_WIDTH * 0.8;
const MIN_SPAWN_DISTANCE = 100;

const MAX_BUFF_LEVEL = 3;
const MAX_ITEM_LEVEL = 3;

const TICKS_PER_SECONDS: f32    = 20;
const TICK_RATE: f32            = 1.0 / TICKS_PER_SECONDS;

// base cooldown list (real value might get changed due to other things in the game)
const COOLDOWN_RANDOM_BOX = 5;
const COOLDOWN_PLAYER_DASH = 2;
const COOLDOWN_ROUND_RESET = 8;

// debug flags
const DEBUG_DISABLE_ALL                 = true;
const DEBUG_DRAW_CENTRE_POINTS          = if(DEBUG_DISABLE_ALL) false else false;
const DEBUG_DRAW_MOUSE_DIRECTION_ARROWS = if(DEBUG_DISABLE_ALL) false else false;
const DEBUG_DRAW_ENEMY_ATTACK_BOXES     = if(DEBUG_DISABLE_ALL) false else true;
const DEBUG_DRAW_SPAWN_DISTANCE         = if(DEBUG_DISABLE_ALL) false else false;
const DEBUG_DRAW_PLAYER_REACH_SIZE      = if(DEBUG_DISABLE_ALL) false else false;
const DEBUG_DISABLE_SPAWNS              = if(DEBUG_DISABLE_ALL) false else false;
const DEBUG_ONE_ENEMY_PER_ROUND         = if(DEBUG_DISABLE_ALL) false else false;
const DEBUG_GOD_MODE                    = if(DEBUG_DISABLE_ALL) false else true;
const DEBUG_GIVE_MONEY                  = if(DEBUG_DISABLE_ALL) false else true;
const DEBUG_START_ROUND                 = if(DEBUG_DISABLE_ALL) 0     else 0;
const DEBUG_GIVE_BUFFS                  = if(DEBUG_DISABLE_ALL) false else true;

const MICROUI_FONT_SIZE = 10;

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

    paused: bool,
    keyboard: [348]InputState,
    mouse: [7]InputState,
    mouse_screen_position: V2f,
    mouse_world_position: V2f,
    mouse_delta_movement: V2f,
    mouse_wheel: f32,
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
    microui_context: *microui.mu_Context,
    allocator: std.mem.Allocator,
    frame_allocator: std.heap.FixedBufferAllocator
};

fn new_state(allocator: std.mem.Allocator) State {
    var state = State{
        .paused = false,
        .keyboard = [_]State.InputState{.up} ** 348,
        .mouse = [_]State.InputState{.up} ** 7,
        .mouse_screen_position = vscaler(0),
        .mouse_world_position = vscaler(0),
        .mouse_delta_movement = vscaler(0),
        .mouse_wheel = 0,
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
            .round = 0 + DEBUG_START_ROUND,
            .enemies_left_to_spawn = 0,
            .total_kills = 0,
            .kills_this_round = 0,
            .end_of_round = true,
            .time_to_next_round = 1,
            .money = 0,
        },
        .rng = std.rand.DefaultPrng.init(123456),
        .microui_context = microui.init_context(),
        .allocator = allocator,
        .frame_allocator = undefined,
    };

    const frmae_allocator_buffer = state.allocator.alloc(u8, 1024 * 100) catch unreachable; 
    state.frame_allocator = std.heap.FixedBufferAllocator.init(frmae_allocator_buffer);

    state.microui_context.text_width = microui_text_width_callback;
    state.microui_context.text_height = microui_text_height_callback;

    // zero init all entities
    for(state.entities.slice()) |*entity| {
        entity.* = std.mem.zeroes(Entity);
    }

    if(DEBUG_GIVE_MONEY) {
        state.level.money = 20000;
    }

    return state;
}

fn save_state(state: *State) !void {
    const cwd = std.fs.cwd();

    const file = try cwd.createFile("level.scene", .{});
    defer file.close();

    if(true) {
        for(state.entities.slice(), 0..) |*entity, i| {
            _ = i; // autofix
            // if(i < 1) {
                const bytes = try encode.serialize(state.frame_allocator.allocator(), entity.*);
                try file.writeAll(bytes);
            // }
        }
    }
}

fn load_state(state: *State) !void {
    const cwd = std.fs.cwd();

    const bytes = try cwd.readFileAlloc(state.frame_allocator.allocator(), "level.scene", state.frame_allocator.buffer.len - state.frame_allocator.end_index);
    if(true){
        // remove all entities
        state.entities.len = 0;

        var read_index: usize = 0;

        while (read_index < bytes.len) {
            const entity = try state.entities.addOne();
            entity.* = Entity{};
            const bytes_read = try encode.deserialize(state.frame_allocator.allocator(), Entity, bytes[read_index..], entity);

            read_index += bytes_read + 1;
        }
    }
}

fn run(state: *State) void {
    while (!raylib.WindowShouldClose()) {
        const delta_time = raylib.GetFrameTime();
        state.tick_timer += delta_time; 
        state.time_since_start += delta_time;

        tick: {
            if(state.tick_timer >= TICK_RATE) {
                input(state);

                if(key(state, raylib.KEY_P) == .down) {
                    state.paused = !state.paused;
                }

                if(state.paused) break :tick;
    
                const after_one_zoom = 1;
                const before_one_zoom = 0.2;
    
                if(key(state, raylib.KEY_UP) == .down) {
                    if(state.camera.zoom > 1) {
                        state.camera.zoom += after_one_zoom;
                    } else {
                        state.camera.zoom += before_one_zoom;
                    }
                }
    
                if(key(state, raylib.KEY_DOWN) == .down) {
                    if(state.camera.zoom > 1) {
                        state.camera.zoom -= after_one_zoom;
                    } else {
                        state.camera.zoom -= before_one_zoom;
                    }
                }
    
                state.camera.zoom = std.math.clamp(state.camera.zoom, 0.1, 10);
    
                var update_timer = std.time.Timer.start() catch unreachable;
                update_level(state);
                update_entites(state);
                state.update_time_nanoseconds = update_timer.read();
    
                state.tick_timer = 0;
            }
        }

        var physics_timer = std.time.Timer.start() catch unreachable;
        if(!state.paused) {
            physics(state, delta_time);
        }
        state.physics_time_nanoseconds = physics_timer.read();

        var draw_timer = std.time.Timer.start() catch unreachable;
        draw(state, delta_time);
        state.draw_time_nanoseconds = draw_timer.read();

        state.frame_allocator.reset();
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
    const delta = raylib.GetMouseDelta();

    state.mouse_screen_position = V2f{mouse_screen_position.x, mouse_screen_position.y};
    state.mouse_world_position = V2f{mouse_world_position.x, mouse_world_position.y};
    state.mouse_delta_movement = V2f{delta.x, delta.y};
    state.mouse_wheel = raylib.GetMouseWheelMoveV().y;

    { // micro ui input events
        switch (mouse(state, raylib.MOUSE_BUTTON_LEFT)) {
            .down => microui.mu_input_mousedown(
                state.microui_context, 
                @intFromFloat(state.mouse_screen_position[0]), 
                @intFromFloat(state.mouse_screen_position[1]), 
                microui.MU_MOUSE_LEFT
            ),
            .released => microui.mu_input_mouseup(
                state.microui_context, 
                @intFromFloat(state.mouse_screen_position[0]), 
                @intFromFloat(state.mouse_screen_position[1]), 
                microui.MU_MOUSE_LEFT
            ),
            else => {}
        }

        switch (mouse(state, raylib.MOUSE_BUTTON_RIGHT)) {
            .down => microui.mu_input_mousedown(
                state.microui_context, 
                @intFromFloat(state.mouse_screen_position[0]), 
                @intFromFloat(state.mouse_screen_position[1]), 
                microui.MU_MOUSE_RIGHT
            ),
            .released => microui.mu_input_mouseup(
                state.microui_context, 
                @intFromFloat(state.mouse_screen_position[0]), 
                @intFromFloat(state.mouse_screen_position[1]), 
                microui.MU_MOUSE_RIGHT
            ),
            else => {}
        }

        if(state.mouse_delta_movement[0] != 0 or state.mouse_delta_movement[1] != 0) {
            microui.mu_input_mousemove(
                state.microui_context, 
                @intFromFloat(state.mouse_delta_movement[0]), 
                @intFromFloat(state.mouse_delta_movement[1]), 
            );
        }

        if(state.mouse_wheel != 0) {
            microui.mu_input_scroll(
                state.microui_context, 
                0,
                @intFromFloat(state.mouse_wheel), 
            );
        }
    }
}

/////////////////////////////////////////////////////////////////////////
///                         @update
/////////////////////////////////////////////////////////////////////////
fn update_entites(state: *State) void {
    for(0..state.entities.len) |_index| {
        var entity: *Entity = &state.entities.slice()[_index];

        { // tick update cooldowns
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

            if(entity.random_box_cooldown > 0) {
                entity.random_box_cooldown -= TICK_RATE;

                if(entity.random_box_cooldown <= 0) {
                    entity.random_box_cooldown = 0;
                }
            }

            if(entity.dash_cooldown > 0) {
                entity.dash_cooldown -= TICK_RATE;

                if(entity.dash_cooldown <= 0) {
                    entity.dash_cooldown = 0;
                }
            }

            if(entity.item_cooldown > 0) {
                entity.item_cooldown -= TICK_RATE;

                if(entity.item_cooldown <= 0) {
                    entity.item_cooldown = 0;
                }
            }
        }

        player: {
            if(!entiity_has_flag(entity, flag_player)) {
                break :player;
            } 

            // TODO: this is kind of a hack, we are checking every update to change the max health
            // I was just changing the max health when the player picks up the buff
            // but then it would change the max health if you just set it from debug options 
            // but ohhh well...
            const new_max_health = @as(f32, @floatFromInt(PLAYER_HEALTH)) * Buff.health_multiplier(entity.health_buff_level);
            entity.max_health = @intFromFloat(new_max_health);

            { // movement
                var input_vector = vscaler(0);

                if(key(state, raylib.KEY_W) == .pressing) {
                    input_vector[1] -= 1;
                }
     
                if(key(state, raylib.KEY_A) == .pressing) {
                    input_vector[0] -= 1;
                }
         
                if(key(state, raylib.KEY_S) == .pressing) {
                    input_vector[1] += 1;
                }
         
                if(key(state, raylib.KEY_D) == .pressing) {
                    input_vector[0] += 1;
                }

                // normalise vector because if the input is affecting both axis (1, -1)
                // it means the length of the vector is greater the one giving us a 
                // greater speed then if you are moving in one direction
                input_vector = vnormalise(input_vector);

                var dash_vector = vscaler(1);
                if(key(state, raylib.KEY_LEFT_SHIFT) == .down and entity.dash_cooldown == 0) {
                    // dont do the dash if the player is not moving
                    if(vlength(input_vector) != 0) {
                        entity.dash_cooldown = COOLDOWN_PLAYER_DASH * Buff.reload_multiplier(entity.reload_buff_level);
                        dash_vector = vscaler(PLAYER_DASH_MULTIPLIER);
                    }
                }

                const speed_vector = vscaler(PLAYER_SPEED * Buff.speed_multipler(entity.speed_buff_level));

                entity.velocity = input_vector * speed_vector * dash_vector;
            }

            if(key(state, raylib.KEY_U) == .down) {
                if(entity.item != .none and entity.item_level < MAX_ITEM_LEVEL) {
                    entity.item_level += 1; 
                }
            }

            if(key(state, raylib.KEY_R) == .down) {
                if(entity.magazine_ammo != entity.weapon.magazine_size()) {
                    entiity_set_flag(entity, flag_is_reloading);
                    entity.reload_cooldown = entity.weapon.reload_cooldown() * Buff.reload_multiplier(entity.reload_buff_level);
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

                const aim_direction =  vnormalise(state.mouse_world_position - entity.position);
                create_projectiles_for_weapon(state, entity.weapon, entity.position, aim_direction, .player);  

                player_jr_shooting: {
                    if(entity.item == .player_jr) {
                        // if level 1 the the player jr only shoots 50% of the time,
                        // for level 2 and 3 they always shoot
                        if(entity.item_level == 1) {
                            if(state.rng.random().float(f32) < 0.5) {
                                break :player_jr_shooting;
                            }
                        }

                        const player_jr_positions = [_]V2f{
                            entity.position + vscaler(20),
                            entity.position - vscaler(20),
                        };
    
                        const positions_to_use: usize = if(entity.item_level < MAX_ITEM_LEVEL) 1 else 2;
    
                        for(0..positions_to_use) |i| {
                            const position = player_jr_positions[i];
    
                            const player_jr_aim_direction =  vnormalise(state.mouse_world_position - position);
                            create_projectiles_for_weapon(state, entity.weapon, position, player_jr_aim_direction, .item); 
                        }
                    }
                }
            }
            
            if(key(state, raylib.KEY_E) == .down) {
                var iter = new_box_collision_iterator(entity.position, vscaler(PLAYER_REACH_SIZE));

                iteract_check: 
                while(iter.next(state)) |other| {
                    if(entity.id == other.id) {
                        continue;
                    }

                    if(!entiity_has_flag(other, flag_is_interactable)) {
                        continue;
                    }

                    if(entiity_has_flag(other, flag_is_ammo_crate) and state.level.money >= AMMO_CRATE_COST) {
                        entity.magazine_ammo = entity.weapon.magazine_size();
                        entity.reserve_ammo = entity.magazine_ammo * entity.weapon.magazine_count();
                        state.level.money -= AMMO_CRATE_COST;
                    }

                    if(entiity_has_flag(other, flag_is_door) and state.level.money >= other.door_cost) {
                        entiity_set_flag(other, flag_to_be_deleted);
                        state.level.money -= other.door_cost;
                    }

                    if(entiity_has_flag(other, flag_is_random_box)) {
                        // pick random weapon
                        if(other.random_box_cooldown == 0) {
                            if(state.level.money >= RANDOM_BOX_COST) {
                                other.random_box_cooldown = COOLDOWN_RANDOM_BOX;
                                other.random_box_weapon = pick_random_random(state);
                                state.level.money -= RANDOM_BOX_COST;
                            }
                        }
                        else { // pickup weapon in the box
                            std.debug.assert(other.random_box_weapon != .none); // cooldown is not 0 so there should be a weapon
                            
                            entity.weapon = other.random_box_weapon;
                            entity.magazine_ammo = entity.weapon.magazine_size();
                            entity.reserve_ammo = entity.weapon.magazine_size() * entity.weapon.magazine_count();

                            other.random_box_weapon = .none;
                            other.random_box_cooldown = 0;
                        }
                    }

                    if(entiity_has_flag(other, flag_is_weapon_buy) and state.level.money >= other.weapon_buy_cost) {
                        entity.weapon = other.weapon_buy_type;
                        entity.magazine_ammo = entity.weapon.magazine_size();
                        entity.reserve_ammo = entity.magazine_ammo * entity.weapon.magazine_count();
                        state.level.money -= AMMO_CRATE_COST;
                        entiity_set_flag(other, flag_to_be_deleted);
                    }

                    if(entiity_has_flag(other, flag_is_buff_buy) and state.level.money >= other.buff_buy_type.base_cost()) {
                        const level_to_upgrade: *u8 = switch (other.buff_buy_type) {
                            .none => unreachable,
                            .health => &entity.health_buff_level,
                            .speed => &entity.speed_buff_level,
                            .reload => &entity.reload_buff_level
                        };

                        if(level_to_upgrade.* < 4) {
                            state.level.money -= other.buff_buy_type.base_cost();
                            level_to_upgrade.* += 1; 
                        }
                    }

                    if(entiity_has_flag(other, flag_is_item)) {
                        // drop current item
                        if(entity.item != .none) {
                            _ = create_item(state, entity.position, entity.item, entity.item_level, entity.item_kills);
                        }

                        // pickup item
                        entity.item = other.item;
                        entity.item_level = other.item_level;
                        entity.item_kills = other.item_kills;
                        entiity_set_flag(other, flag_to_be_deleted);

                        // need to break because we are creating the new item on the 
                        // player so the collider iterator will keep going and create 
                        // more and more entities
                        break :iteract_check;
                    }
                }
            }
        }

        ai: {
            if(!entiity_has_flag(entity, flag_ai)) {
                break :ai;
            }

            std.debug.assert(entity.ai_type != .none);

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
            const centre_to_centre_distance = vlength(delta_vector);

            // if too far away then just keep moving toward the target and stop 
            // anything else from proceeding
            if(centre_to_centre_distance > entity.ai_type.distance_before_attacking()) {
                entity.velocity = 
                    vnormalise(delta_vector) * 
                    vscaler(ENEMY_SPEED) * 
                    vscaler(get_enemy_speed_multiplier_for_round(state.level.round));

                break :ai;
            } else {
                entity.velocity = vscaler(0);
            }

            if(entity.attacking_cooldown > 0) {
                break :ai; 
            }

            entity.attacking_cooldown = 2;

            const attack_shape = entity.size + vscaler(entity.ai_type.attack_size());
            var iter = new_box_collision_iterator(entity.position, attack_shape);

            while(iter.next(state)) |other| {
                if(entity.id == other.id or other.id != target_entity.id) {
                    continue;
                }

                // maybe this will change but right now assume it is true
                std.debug.assert(entiity_has_flag(other, flag_has_health));
                
                entity_take_damage(other, entity.ai_type.damage());
            }
        }

        items: {
            // this is for entities that have items not items 
            // themselves
            if(entiity_has_flag(entity, flag_is_item) or entity.item == .none) {
                break :items;
            }

            if(entity.item_level < MAX_ITEM_LEVEL and entity.item_kills >= entity.item.kills_to_complete_level(entity.item_level)) {
                entity.item_level += 1;
            }

            damage_ring: {
                if(entity.item != .damage_ring) {
                    break :damage_ring;
                }
                
                if(entity.item_cooldown > 0) {
                    break :damage_ring;
                }

                var did_damage = false;

                var iter = new_circle_collision_iterator(entity.position, Item.damage_ring_radius(entity.item_level));
                while(iter.next(state)) |other| {
                    if(other.id == entity.id) continue;

                    if(!entiity_has_flag(other, flag_has_health)) {
                        continue;
                    }

                    entity_take_damage(other, Item.damage_ring_damage(entity.item_level));
                    did_damage = true;

                    if(other.health == 0) {
                        entity.item_kills += 1;
                    }
                }

                if(did_damage) {
                    entity.item_cooldown = entity.item.cooldown(entity.item_level);
                }
            }

            gaurdian: {
                if(entity.item != .gaurdian) {
                    break :gaurdian;
                }
                
                if(entity.item_cooldown > 0) {
                    break :gaurdian;
                }


                entity.item_cooldown = entity.item.cooldown(entity.item_level);

                for(0..Item.gaurdian_projectile_count(entity.item_level)) |_| {
                    const angle = state.rng.random().float(f32) * 360;
                    const fire_angle = rotate_normalised_vector(V2f{1, 0}, angle);
    
                    _ = create_projectile(
                        state, 
                        entity.position, 
                        fire_angle, 
                        Item.gaurdian_projectile_size(entity.item_level), 
                        Item.gaurdian_damage(entity.item_level), 
                        .item
                    );
                }
            }
        }

        projectile: {
            if(!entiity_has_flag(entity, flag_projectile)) {
                break :projectile;
            }

            if(state.time_since_start - entity.time_created > 2) {
                entiity_set_flag(entity, flag_to_be_deleted);
            } 

            if(entity.penetration_count > 8) {
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
            if(entity.health_regen_rate > 0 and entity.health < entity.max_health and entity.health_regen_cooldown == 0) {
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

            if(DEBUG_DISABLE_SPAWNS) {
                break :spawner;
            }

            if(entity.spawner_cooldown == 0 and state.level.enemies_left_to_spawn > 0) {
                const player = get_entity_with_flag(state, flag_player) orelse break :spawner;

                // even if we fail to spawn still reset the spawner, this means spawers
                // wont sawn enemies instally when the player is in the correct spot
                entity.spawner_cooldown = get_varied_spawner_cooldown_for_round(state, state.level.round);

                // player distance
                const distance = vlength(vdistance_abs(entity.position, player.position));
                if(distance > MAX_SPAWN_DISTANCE or distance < MIN_SPAWN_DISTANCE) {
                    break :spawner;
                }

                // line of sight
                var iter = new_raycast_iterator(entity.position, player.position);
                while(iter.next(state)) |other| {
                    if(entity.id == other.id or player.id == other.id) continue;

                    if(entiity_has_flag(other, flag_is_static)) {
                        break :spawner;
                    }
                }

                // past round 12 have a 5% chance of spawing a big enemy 
                if(state.level.round > 12 and state.rng.random().float(f32) < 0.05) {
                    _ = create_big_enemy(state, entity.position);
                } else {
                    _ = create_basic_enemy(state, entity.position);
                }

                state.level.enemies_left_to_spawn -= 1;    
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

        random_box: {
            if(!entiity_has_flag(entity, flag_is_random_box)) {
                break :random_box;
            }

            if(entity.random_box_cooldown == 0) {
                entity.random_box_weapon = .none;
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

        for(state.entities.slice()) |*entity| {
            on_round_start(state, entity);
        }
    }

    // end the round
    std.debug.assert(get_enemy_count_for_round(state.level.round) >= state.level.kills_this_round);
    if(get_enemy_count_for_round(state.level.round) - state.level.kills_this_round == 0 and !state.level.end_of_round) {
        state.level.end_of_round = true;
        state.level.time_to_next_round = COOLDOWN_ROUND_RESET;
    }
}


/////////////////////////////////////////////////////////////////////////
///                         @draw
/////////////////////////////////////////////////////////////////////////
fn draw(state: *State, delta_time: f32) void {
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
                    .yellow => raylib.ColorBrightness(raylib.YELLOW, -0.3),
                    .green => raylib.ColorBrightness(raylib.GREEN, -0.3),
                    .pink => raylib.ColorBrightness(raylib.PINK, -0.3),
                    .brown => raylib.BROWN,
                    .light_blue => raylib.SKYBLUE,
                };

                if(entiity_has_flag(entity, flag_is_reloading)) {
                    color = raylib.ORANGE;
                }

                if(entity.spawner_cooldown > 0) {
                    color = raylib.ORANGE;
                }
                
                render.rectangle(entity.position, entity.size, color);

                if(entity.display_name.len != 0) {
                    render.text(entity.display_name, entity.position, 10, raylib.BLACK);
                }
            }
    
            if(DEBUG_DRAW_CENTRE_POINTS) {
                render.circle(entity.position, 8, raylib.YELLOW);
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
                    const box_size = entity.size + vscaler(30);
                    const fade = if (entity.attacking_cooldown > 2) 0.2 else entity.attacking_cooldown * 0.1;

                    render.rectangle(entity.position, box_size, raylib.Fade(raylib.GREEN, fade));
                }
            }

            spawner_lines: {
                if(DEBUG_DRAW_SPAWN_DISTANCE and entiity_has_flag(entity, flag_spawner)) {
                    const player = get_entity_with_flag(state, flag_player) orelse break :spawner_lines;

                    render.circle(entity.position, MIN_SPAWN_DISTANCE, raylib.Fade(raylib.RED, 0.15));

                    var line_color = raylib.GREEN;

                    // player too far away
                    if(vlength(vdistance_abs(entity.position, player.position)) > MAX_SPAWN_DISTANCE) {
                        line_color = raylib.ORANGE;
                    }
                     
                    var iter = new_raycast_iterator(entity.position, player.position);
                    while(iter.next(state)) |other| {
                        if(entity.id == other.id or player.id == other.id) continue;
    
                        if(entiity_has_flag(other, flag_is_static)) {
                            line_color = raylib.RED;
                        }
                    }

                    render.line(entity.position, player.position, 2, line_color);
                }
            }

            if(DEBUG_DRAW_PLAYER_REACH_SIZE and entiity_has_flag(entity, flag_player)) {
                render.rectangle(entity.position, vscaler(PLAYER_REACH_SIZE), raylib.Fade(raylib.YELLOW, 0.15));
            }

        }

        // second layer for things like icons you want to sit on top
        // of the entities in the scene
        for(0..state.entities.slice().len) |i| {
            const entity: *Entity = &state.entities.slice()[i];

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

            if(entiity_has_flag(entity, flag_player)) {
                { // player interaction
                    var iter = new_box_collision_iterator(entity.position, vscaler(PLAYER_REACH_SIZE));
    
                    while(iter.next(state)) |other| {
                        if(entity.id == other.id) {
                            continue;
                        }
    
                        if(!entiity_has_flag(other, flag_is_interactable)) {
                            continue;
                        }
    
                        var icon_text: [:0]const u8 = "";
                        var icon_text_color = raylib.WHITE;
    
                        if(entiity_has_flag(other, flag_is_ammo_crate)) {
                            icon_text = std.fmt.comptimePrint("Buy Ammo {}", .{AMMO_CRATE_COST});
                        }
    
                        if(entiity_has_flag(other, flag_is_door)) {
                            icon_text = std.fmt.allocPrintZ(state.frame_allocator.allocator(), "Open door {}", .{other.door_cost}) catch unreachable;
                        }
    
                        if(entiity_has_flag(other, flag_is_random_box)) {
                            if(other.random_box_cooldown > 0) {
                                std.debug.assert(other.random_box_weapon != .none);

                                icon_text = std.fmt.allocPrintZ(
                                    state.frame_allocator.allocator(), 
                                    "Take {s} ({d})", 
                                    .{other.random_box_weapon.display_name(), @round(other.random_box_cooldown)}
                                ) catch unreachable;

                                icon_text_color = raylib.YELLOW;
                            }
                            else {
                                icon_text = std.fmt.comptimePrint("Random weapon {}", .{RANDOM_BOX_COST});
                            }
                        }

                        if(entiity_has_flag(other, flag_is_weapon_buy)) {
                            icon_text = std.fmt.allocPrintZ(
                                state.frame_allocator.allocator(), 
                                "Buy {s} {d}", 
                                .{other.weapon_buy_type.display_name(), other.weapon_buy_cost}
                            ) catch unreachable;

                            icon_text_color = raylib.WHITE;
                        }

                        if(entiity_has_flag(other, flag_is_buff_buy)) {
                            icon_text = std.fmt.allocPrintZ(
                                state.frame_allocator.allocator(), 
                                "Buy {s} buff {d}", 
                                .{other.buff_buy_type.display_name(), other.buff_buy_type.base_cost()}
                            ) catch unreachable;

                            icon_text_color = raylib.WHITE;
                        }

                        if(entiity_has_flag(other, flag_is_item)) {
                            icon_text = std.fmt.allocPrintZ(
                                state.frame_allocator.allocator(), 
                                "Pickup {s} lvl {}", 
                                .{other.item.display_name(), other.item_level}
                            ) catch unreachable;

                            icon_text_color = raylib.WHITE;
                        }
  
                        const icon_text_size = 15;
                        const icon_padding = 5;
                        const icon_offset = V2f{0, -30};
                        const text_width = render.text_draw_width(icon_text, icon_text_size);

                        render.rectangle(other.position + icon_offset, V2f{text_width + (icon_padding * 2), 20}, raylib.Fade(raylib.BLACK, 0.5));
                        render.text(icon_text, other.position + icon_offset, icon_text_size, icon_text_color);
                    }
                }

                dash_indicator: {
                    if(entity.dash_cooldown == 0) break :dash_indicator;

                    const bar_size = V2f{entity.size[0] + (entity.size[0] * 0.3), 8};
    
                    render.rectangle(
                        entity.position - V2f{0, (entity.size[1])}, 
                        bar_size, 
                        raylib.ColorBrightness(raylib.BLUE, -0.5)
                    );
    
                    const indicator_multiplier = 20; // just so the bar is smooth
                 
                    render.progress_bar_horizontal(
                        entity.position - V2f{0, (entity.size[1])}, 
                        bar_size, 
                        raylib.BLUE, 
                    @intFromFloat((COOLDOWN_PLAYER_DASH - entity.dash_cooldown) * indicator_multiplier), 
                        COOLDOWN_PLAYER_DASH * indicator_multiplier
                    );
                }

                if(false) { // raycast lines from player
                    // L1
                    const l1_start = V2f{entity.position[0], entity.position[1]};
                    const l1_end = state.mouse_world_position;
                    render.line(l1_start, l1_end, 2, raylib.BLUE);
     
                    // L2
                    for(0..state.entities.slice().len) |j| {
                        const other: *Entity = &state.entities.slice()[j];

                        const points = [4]V2f{
                            other.position - (other.size * vscaler(0.5)), // top left
                            other.position + (other.size * vscaler(0.5)), // bottom right
                            other.position + (V2f{other.size[0], -other.size[1]} * vscaler(0.5)), // top right
                            other.position + (V2f{-other.size[0], other.size[1]} * vscaler(0.5)), // bottom left
                        };

                        const lines = [4]struct {start: V2f, end: V2f} {
                            .{.start = points[0], .end = points[2]}, // top line
                            .{.start = points[2], .end = points[1]}, // right line
                            .{.start = points[1], .end = points[3]}, // bottom line
                            .{.start = points[3], .end = points[0]} // left line
                        };

                        // for(0..points.len) |p| {
                            // render.circle(points[p], 20, raylib.YELLOW);
                        // }

                        for(0..lines.len) |l| {
                            const line = lines[l];
                            const color = if(lines_intersect(l1_start, l1_end, line.start, line.end)) raylib.GREEN else raylib.RED;
                            render.line(line.start, line.end, 3, color);
                        }
                    }
                }
            }

            items: {
                if(entiity_has_flag(entity, flag_is_item) or entity.item == .none) {
                    break :items;
                }

                damage_ring: {
                    if(entity.item != .damage_ring) {
                        break :damage_ring;
                    }
                   
                    // hack to get it to diplay green for one frame before being used
                    const color = if(entity.item_cooldown > TICK_RATE) raylib.RED else raylib.GREEN;
                    render.circle(entity.position, Item.damage_ring_radius(entity.item_level), raylib.Fade(color, 0.1));
                }

                player_jr: {
                    if(entity.item != .player_jr) {
                        break :player_jr;
                    }
                   
                    render.rectangle(entity.position + vscaler(20), vscaler(30), raylib.SKYBLUE);

                    if(entity.item_level == MAX_ITEM_LEVEL) {
                        render.rectangle(entity.position - vscaler(20), vscaler(30), raylib.BLUE);
                    }
                }
            }
        }
    }

    raylib.EndMode2D();

    { // rendering in screen space 
        player_buff_info: {
            const player = get_entity_with_flag(state, flag_player) orelse break :player_buff_info;

            const buff_icons_size = 25;
            const buff_names_font_size = 12;
            const icons_y = WINDOW_HEIGHT - buff_icons_size - 8; // padding
            const names_y = icons_y - buff_icons_size - 15;

            render.text("Buffs", V2f{WINDOW_WIDTH - 120, icons_y - 70}, 20, raylib.BLACK);
            
            { // health buff
                const icon_color = if(player.health_buff_level > 0) raylib.ColorBrightness(raylib.RED, -0.3) else raylib.Fade(raylib.ColorBrightness(raylib.RED, -0.7), 0.5);
                const icon_x = WINDOW_WIDTH - 190;
                const text_width = render.text_draw_width(Buff.health.display_name(), buff_names_font_size);

                render.circle(V2f{icon_x, icons_y}, buff_icons_size, icon_color);
                render.rectangle(V2f{icon_x, names_y}, V2f{text_width + 15, 20}, raylib.Fade(raylib.BLACK, 0.6));
                render.text(Buff.health.display_name(), V2f{icon_x, names_y}, buff_names_font_size, raylib.WHITE);
               
                if(player.health_buff_level > 0) {
                    const level_string = std.fmt.allocPrintZ(state.frame_allocator.allocator(), "lvl {}", .{player.health_buff_level}) catch unreachable;
                    render.text(level_string, V2f{icon_x, icons_y}, buff_names_font_size, raylib.WHITE);
                }
            }

            { // speed buff
                const icon_color = if(player.speed_buff_level > 0) raylib.ColorBrightness(raylib.SKYBLUE, -0.3) else raylib.Fade(raylib.ColorBrightness(raylib.SKYBLUE, -0.7), 0.5);
                const icon_x = WINDOW_WIDTH - 120;
                const text_width = render.text_draw_width(Buff.speed.display_name(), buff_names_font_size);

                render.circle(V2f{icon_x, icons_y}, buff_icons_size, icon_color);
                render.rectangle(V2f{icon_x, names_y}, V2f{text_width + 15, 20}, raylib.Fade(raylib.BLACK, 0.6));
                render.text(Buff.speed.display_name(), V2f{icon_x, names_y}, buff_names_font_size, raylib.WHITE);
               
                if(player.speed_buff_level > 0) {
                    const level_string = std.fmt.allocPrintZ(state.frame_allocator.allocator(), "lvl {}", .{player.speed_buff_level}) catch unreachable;
                    render.text(level_string, V2f{icon_x, icons_y}, buff_names_font_size, raylib.WHITE);
                }
            }

            { // reload buff
                const icon_color = if(player.reload_buff_level > 0) raylib.ColorBrightness(raylib.GREEN, -0.3) else raylib.Fade(raylib.ColorBrightness(raylib.GREEN, -0.7), 0.5);
                const icon_x = WINDOW_WIDTH - 50;
                const text_width = render.text_draw_width(Buff.reload.display_name(), buff_names_font_size);

                render.circle(V2f{icon_x, icons_y}, buff_icons_size, icon_color);
                render.rectangle(V2f{icon_x, names_y}, V2f{text_width + 15, 20}, raylib.Fade(raylib.BLACK, 0.6));
                render.text(Buff.reload.display_name(), V2f{icon_x, names_y}, buff_names_font_size, raylib.WHITE);
               
                if(player.reload_buff_level > 0) {
                    const level_string = std.fmt.allocPrintZ(state.frame_allocator.allocator(), "lvl {}", .{player.reload_buff_level}) catch unreachable;
                    render.text(level_string, V2f{icon_x, icons_y}, buff_names_font_size, raylib.WHITE);
                }
            }
        }

        player_item_info: {
            const player = get_entity_with_flag(state, flag_player) orelse break :player_item_info;

            const name_y = WINDOW_HEIGHT - 75;
            const name_x = WINDOW_WIDTH - 400;
            const name_font_size = 20;
            const item_name = if(player.item == .none) "No item equipped" else player.item.display_name();
            const rectangle_height = 22;
            var rectangle_width = render.text_draw_width(item_name, name_font_size);
            if(rectangle_width < 300) {
                rectangle_width = 300;
            }
           
            // top text
            render.text("Item", V2f{name_x, name_y - 25}, 20, raylib.BLACK);

            // item name text
            render.rectangle(V2f{name_x, name_y}, V2f{rectangle_width, rectangle_height}, raylib.Fade(raylib.BLACK, 0.6));
            render.text(item_name, V2f{name_x, name_y}, name_font_size, raylib.WHITE);

            // level display
            render.rectangle(V2f{name_x, name_y + 25}, V2f{rectangle_width, rectangle_height}, raylib.ColorBrightness(raylib.BLUE, -0.6));

            if(player.item != .none) {
                if(player.item_level != MAX_ITEM_LEVEL) {
                    render.progress_bar_horizontal(V2f{name_x, name_y + 25}, V2f{rectangle_width, rectangle_height}, raylib.BLUE, player.item_kills, player.item.kills_to_complete_level(player.item_level));

                    const level_text = std.fmt.allocPrintZ(
                        state.frame_allocator.allocator(), 
                        "lvl {}      {}/{}", 
                        .{player.item_level, player.item_kills, player.item.kills_to_complete_level(player.item_level)}
                    ) catch unreachable;
     
                    render.text(level_text, V2f{name_x, name_y + 25}, name_font_size, raylib.Fade(raylib.WHITE, 0.5));
                } else {
                    render.rectangle(V2f{name_x, name_y + 25}, V2f{rectangle_width, rectangle_height}, raylib.ColorBrightness(raylib.YELLOW, -0.3));
                    render.text("MAX LEVEL", V2f{name_x, name_y + 25}, name_font_size, raylib.BLACK);
                }
            }
        }

        weapon_info_text: {
            const player = get_entity_with_flag(state, flag_player) orelse break :weapon_info_text;
            std.debug.assert(entiity_has_flag(player, flag_has_weapon));

            const string = std.fmt.allocPrintZ(
                state.frame_allocator.allocator(), 
                "{s}: {}/{}    money: {}", 
                .{player.weapon.display_name(), player.magazine_ammo, player.reserve_ammo, state.level.money}
            ) catch unreachable;

            const color = if(entiity_has_flag(player, flag_is_reloading)) raylib.ORANGE else raylib.BLUE;
            const text_width = render.text_draw_width(string, 30);
            render.text(string, V2f{(text_width * 0.5) + 10, WINDOW_HEIGHT - 20}, 30, color);
        }

        { // pause text
            if(state.paused) {
                render.text("PAUSED", V2f{WINDOW_WIDTH * 0.5, WINDOW_HEIGHT * 0.5}, 80, raylib.BLACK);
                render.text("esc to quit", V2f{WINDOW_WIDTH * 0.5, (WINDOW_HEIGHT * 0.5) + 60}, 30, raylib.BLACK);
            }
        }

        { // game info text
            const string = std.fmt.allocPrintZ(
                state.frame_allocator.allocator(), 
                "round: {:<3}   kills: {:<4} remaining: {:<4}", 
                .{state.level.round, state.level.total_kills, get_enemy_count_for_round(state.level.round) - state.level.kills_this_round}
            ) catch unreachable;


            const text_color = if(state.level.end_of_round) raylib.WHITE else raylib.RED;
            render.text(string, V2f{WINDOW_WIDTH * 0.5, 20}, 30, text_color);
        }

        { // performance text
            const ut_milliseconds = @as(f64, @floatFromInt(state.update_time_nanoseconds)) / 1_000_000;
            const pt_milliseconds = @as(f64, @floatFromInt(state.physics_time_nanoseconds)) / 1_000_000;
            const dt_milliseconds = @as(f64, @floatFromInt(state.draw_time_nanoseconds)) / 1_000_00;

            const string = std.fmt.allocPrintZ(
                state.frame_allocator.allocator(), 
                "fps: {d:<8.4}, u: {d:<8.4}, p: {d:<8.4}, d-1: {d:<8.4}, e: {d}", 
                .{1 / delta_time, ut_milliseconds, pt_milliseconds, dt_milliseconds, state.entities.slice().len}
            ) catch unreachable;

            render.text(string, V2f{WINDOW_WIDTH * 0.5, 50}, 16, raylib.WHITE);
        } 
    } 

    if(false) { // micro ui rendering
        microui.mu_begin(state.microui_context);

        { // entity window
            const Static = struct {
                    var selected_entity: ?usize = null;
            };

            if (microui.mu_begin_window_ex(state.microui_context, "Entities", microui.mu_rect(30, 30, 250, 400), microui.MU_OPT_AUTOSIZE) != 0) {
                for(0..state.entities.len) |i| {
                    const entity: *Entity = &state.entities.slice()[i];
                    const string = std.fmt.allocPrintZ(
                        state.frame_allocator.allocator(), 
                        "entity: {d}", 
                        .{entity.id}
                    ) catch unreachable;
     
                    microui.mu_push_id(state.microui_context, &i, @sizeOf(usize));
                    if (microui.mu_button(state.microui_context, string) != 0) {
                        Static.selected_entity = i;
                    }
                    microui.mu_pop_id(state.microui_context);
                } 

                microui.mu_end_window(state.microui_context);
            }

            if(Static.selected_entity) |selected_entity| {
                if (microui.mu_begin_window_ex(state.microui_context, "entity", microui.mu_rect(300, 300, 100, 100), microui.MU_OPT_AUTOSIZE) != 0) {
                    if (microui.mu_button(state.microui_context, "close") != 0) {
                        Static.selected_entity = null;
                    } else {
                        draw_value_microui(state, state.entities.slice()[selected_entity]);
                    }

                    microui.mu_end_window(state.microui_context);
                }
            }
        }

        microui.mu_end(state.microui_context);
    } 

    draw_microui_command_list(state);
    raylib.EndDrawing();
}

/////////////////////////////////////////////////////////////////////////
///                         @physics
/////////////////////////////////////////////////////////////////////////
fn physics(state: *State, delta_time: f32) void {
    if(delta_time == 0) return;

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

            const distance = vdistance_abs(entity.position, self.position);
            const distance_for_collision = (self.size + entity.size) * vscaler(0.5);
 
            if (
                distance_for_collision[0] >= distance[0] and
                distance_for_collision[1] >= distance[1]
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

const CirlceCollisionIterator = struct {
    const Self = @This();

    index: usize,
    position: V2f,
    radius: f32,

    fn next(self: *Self, state: *State) ?*Entity {
        const index = self.index;
        for (state.entities.slice()[index..]) |*entity| {
            self.index += 1;

            const entity_collision_positions = [_]V2f{
                entity.position - (entity.size * vscaler(0.5)), // top left
                entity.position + (V2f{entity.size[0], -entity.size[1]} * vscaler(0.5)), // top right
                entity.position - (V2f{entity.size[0], -entity.size[1]} * vscaler(0.5)), // bottom left
                entity.position + (entity.size * vscaler(0.5)), // bottom right
            };

            for(entity_collision_positions) |position| {
                const distance_vector = vdistance_abs(self.position, position);
                const distance = vlength(distance_vector);

                if(distance <= self.radius) {
                    return entity;
                }
            }
        }

        return null;
    }
};

fn new_circle_collision_iterator(position: V2f, radius: f32) CirlceCollisionIterator {
    return CirlceCollisionIterator{
        .index = 0,
        .position = position,
        .radius = radius
    };
}

const RayCastIterator = struct {
    const Self = @This();

    index: usize,
    start_ray: V2f,
    end_ray: V2f,

    fn next(self: *Self, state: *State) ?*Entity {
        const index = self.index;
        for (index..state.entities.slice().len) |i| {
            self.index += 1;
            
            const other: *Entity = &state.entities.slice()[i];

            const points = [4]V2f{
                other.position - (other.size * vscaler(0.5)), // top left
                other.position + (other.size * vscaler(0.5)), // bottom right
                other.position + (V2f{other.size[0], -other.size[1]} * vscaler(0.5)), // top right
                other.position + (V2f{-other.size[0], other.size[1]} * vscaler(0.5)), // bottom left
            };
                
            const lines = [4]struct {start: V2f, end: V2f} {
                .{.start = points[0], .end = points[2]}, // top line
                .{.start = points[2], .end = points[1]}, // right line
                .{.start = points[1], .end = points[3]}, // bottom line
                .{.start = points[3], .end = points[0]} // left line
            };
                
            for(0..lines.len) |l| {
                const line = lines[l];
                if(lines_intersect(self.start_ray, self.end_ray, line.start, line.end)) {
                    return other;
                }
            }
        }

        return null;
    }
};

fn new_raycast_iterator(start_ray: V2f, end_ray: V2f) RayCastIterator {
    return RayCastIterator {
        .index = 0,
        .start_ray = start_ray,
        .end_ray = end_ray
    };
}

/////////////////////////////////////////////////////////////////////////
///                         @gameplay
/////////////////////////////////////////////////////////////////////////
fn create_projectiles_for_weapon(state: *State, weapon: Weapon, position: V2f, normalised_direction: V2f, source: ProjectileSource) void {
    switch (weapon) {
        .pistol, .m4, .smg => {
            _ = create_projectile(state, position, normalised_direction, vscaler(10), weapon.damage(), source);
        },
        .splitter => {
            const directions = [_]V2f {
                rotate_normalised_vector(normalised_direction, 10),
                rotate_normalised_vector(normalised_direction, -10),
            };

            for(&directions) |direction| {
                _ = create_projectile(state, position, direction, vscaler(10), weapon.damage(), source);
            }
        },
        .shotgun => {
            const directions = [_]V2f {
                normalised_direction,

                rotate_normalised_vector(normalised_direction, 10),
                rotate_normalised_vector(normalised_direction, 20),

                rotate_normalised_vector(normalised_direction, -10),
                rotate_normalised_vector(normalised_direction, -20),
            };

            for(&directions) |direction| {
                _ = create_projectile(state, position, direction, vscaler(15), weapon.damage(), source);
            }
        },
        .none => unreachable,
    }
}

fn pick_random_random(state: *State) Weapon {
    const type_info = @typeInfo(Weapon).Enum;
    const max = type_info.fields[type_info.fields.len - 1].value;
    const i = state.rng.random().intRangeAtMost(type_info.tag_type, 1, max);
    return @enumFromInt(i);
}

/////////////////////////////////////////////////////////////////////////
///                         @event
/////////////////////////////////////////////////////////////////////////
fn on_trigger_collided_start(state: *State, trigger_entity: *Entity, collided_entity: *Entity) void {
    if(entiity_has_flag(trigger_entity, flag_projectile)) {
        if(entiity_has_flag(collided_entity, flag_player) or !entiity_has_flag(collided_entity, flag_has_health)) {
            return;
        }

        std.debug.assert(trigger_entity.projectile_damage != 0);

        // 10% each penetration amount
        const damage_nerf_percentage: f32 = @as(f32, @floatFromInt(trigger_entity.penetration_count)) * 0.1;
        const damage_reduction: u64 = @intFromFloat(@as(f32, @floatFromInt(trigger_entity.projectile_damage)) * damage_nerf_percentage);

        entity_take_damage(collided_entity, trigger_entity.projectile_damage - damage_reduction);

        trigger_entity.penetration_count += 1;

        if(collided_entity.health == 0) {
            state.level.money += 100; 

            if(trigger_entity.entity_source == .item) {
                if(get_entity_with_flag(state, flag_player)) |player| {
                    player.item_kills += 1;
                }
            }
        } else {
            state.level.money += 10; 
        }
    }
}

fn on_round_start(state: *State, entity: *Entity) void {
    spawner: {
        if(!entiity_has_flag(entity, flag_spawner)) {
            break :spawner;
        }

        entity.spawner_cooldown = get_varied_spawner_cooldown_for_round(state, state.level.round);
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
    brown,
    light_blue
};

/////////////////////////////////////////////////////////////////////////
///                         @entity
/////////////////////////////////////////////////////////////////////////
const Entity = struct {
    // meta
    id: EntityID                = 0,
    time_created: f64           = 0,
    flags: u64                  = 0,
    display_name: []const u8    = &[_]u8{},

    // physics
    position: V2f              = .{0, 0},
    velocity: V2f              = .{0, 0},
    size: V2f                  = .{0, 0},

    // rendering
    texture: TextureHandle      = .none,

    // gameplay
    attacking_cooldown: f32     = 0,

    // gameplay: player
    dash_cooldown: f32          = 0,
    health_buff_level: u8       = 0,
    speed_buff_level: u8        = 0,
    reload_buff_level: u8       = 0,
    item: Item                  = .none,
    item_level: u8              = 0,
    item_cooldown: f32          = 0,
    item_kills: u64             = 0,

    // gameplay: projectile
    penetration_count: u32           = 0,
    projectile_damage: u64           = 0,
    entity_source: ProjectileSource  = .none,

    // gameplay: health
    health: u64                 = 0,
    max_health: u64             = 0,
    health_regen_rate: u32      = 0,
    health_regen_cooldown: f32  = 0,

    // gameplay: ai
    ai_type: AIType             = .none,
    target: ?EntityID           = 0,

    // gameplay: spawner
    spawner_cooldown: f32       = 0,

    // gameplay: weapon
    weapon: Weapon              = .none,
    magazine_ammo: u16          = 0,
    reserve_ammo: u16           = 0,
    reload_cooldown: f32        = 0,

    // gameplay: door
    door_cost: u16              = 0,

    // gameplay: random box
    random_box_cooldown: f32    = 0,
    random_box_weapon: Weapon   = .none,

    // gameplay: weapon buy
    weapon_buy_cost: u16        = 0,
    weapon_buy_type: Weapon     = .none,

    // gameplay: buff buy
    buff_buy_type: Buff         = .none,
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
const flag_is_interactable          :EntityFlag = 1 << 13;
const flag_is_random_box            :EntityFlag = 1 << 14;
const flag_is_weapon_buy            :EntityFlag = 1 << 15;
const flag_is_buff_buy              :EntityFlag = 1 << 16;
const flag_is_item                  :EntityFlag = 1 << 17;

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

        if(entiity_has_flag(entity_ptr, flag_is_weapon_buy)) {
            std.debug.assert(entity_ptr.weapon_buy_cost > 0);
            std.debug.assert(entity_ptr.weapon_buy_type != .none);
        }

        if(entiity_has_flag(entity_ptr, flag_is_item)) {
            std.debug.assert(entity_ptr.item != .none);
        }

        if(entiity_has_flag(entity_ptr, flag_ai)) {
            std.debug.assert(entity_ptr.ai_type != .none);
        }
    }

    return entity_ptr;
}

fn entity_take_damage(entity: *Entity, damage: u64) void {
    if(DEBUG_GOD_MODE and entiity_has_flag(entity, flag_player)) {
        log.info("damage not taken -- god mod enabled", .{});
        return;
    }

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
    const player = create_entity(state, .{
        .flags = flag_player | flag_has_solid_hitbox | flag_has_health | flag_has_weapon,
        .position = position,
        .size = .{50, 50},
        .texture = .blue,
        .health = PLAYER_HEALTH,
        .max_health = PLAYER_HEALTH,
        .health_regen_rate = 40,
        .weapon = .pistol,
        .magazine_ammo = Weapon.pistol.magazine_size(),
        .reserve_ammo =  Weapon.pistol.magazine_size() * Weapon.pistol.magazine_count(),
    });

    if(DEBUG_GIVE_BUFFS) {
        player.health_buff_level = MAX_BUFF_LEVEL;
        player.reload_buff_level = MAX_BUFF_LEVEL;
        player.speed_buff_level = MAX_BUFF_LEVEL;
    }

    return player;
}

fn create_projectile(state: *State, position: V2f, normalised_direction: V2f, size: V2f, damage: u64, projectile_source: ProjectileSource) *Entity {
    return create_entity(state, Entity{
        .flags = flag_projectile | flag_has_trigger_hitbox,
        .position = position,
        .velocity = normalised_direction * vscaler(PROJECTILE_SPEED),
        .size = size,
        .texture = .black,
        .projectile_damage = damage,
        .entity_source = projectile_source
    });
}

fn create_ammo_crate(state: *State, position: V2f) *Entity {
    return create_entity(state, .{
        .flags = flag_has_solid_hitbox | flag_is_static | flag_is_ammo_crate | flag_is_interactable,
        .position = position,
        .size = .{50, 30},
        .display_name = "A",
        .texture = .brown,
    });
}

fn create_weapon_buy(state: *State, position: V2f, weapon: Weapon, cost: u16) *Entity {
    return create_entity(state, .{
        .flags = flag_is_weapon_buy | flag_is_interactable,
        .position = position,
        .size = .{50, 30},
        .display_name = "G",
        .texture = .green,
        .weapon_buy_cost = cost,
        .weapon_buy_type = weapon
    });
}

fn create_buff_buy(state: *State, position: V2f, buff: Buff) *Entity {
    return create_entity(state, .{
        .flags = flag_is_static | flag_has_solid_hitbox | flag_is_buff_buy | flag_is_interactable,
        .position = position,
        .size = .{30, 30},
        .display_name = "U",
        .texture = buff.entity_texture(),
        .buff_buy_type = buff,
    });
}

fn create_item(state: *State, position: V2f, item: Item, level: u8, kills: u64) *Entity {
    return create_entity(state, .{
        .flags = flag_is_item | flag_is_interactable,
        .position = position,
        .size = .{30, 30},
        .display_name = "I",
        .texture = .yellow,
        .item = item,
        .item_level = level,
        .item_kills = kills
    });
}

fn create_random_box(state: *State, position: V2f) *Entity {
    return create_entity(state, .{
        .flags = flag_has_solid_hitbox | flag_is_static | flag_is_random_box | flag_is_interactable,
        .position = position,
        .size = .{60, 80},
        .display_name = "B",
        .texture = .light_blue,
    });
}

fn create_door(state: *State, position: V2f, size: V2f, cost: u16) *Entity {
    return create_entity(state, .{
        .flags = flag_has_solid_hitbox | flag_is_static | flag_is_door | flag_is_interactable,
        .position = position,
        .size = size,
        .texture = .pink,
        .display_name = "D",
        .door_cost = cost
    });
}

fn create_basic_enemy(state: *State, position: V2f) *Entity {
    const base_health: f32 = 50;
    const health_from_round = base_health * get_enemy_health_multiplier_for_round(state.level.round);

    return create_entity(state, .{
        .flags = flag_has_health | flag_ai | flag_has_solid_hitbox,
        .position = position,
        .size = .{30, 30},
        .texture = .red,
        .health = @intFromFloat(health_from_round),
        .max_health = @intFromFloat(health_from_round),
        .ai_type = .basic,
    });
}

fn create_big_enemy(state: *State, position: V2f) *Entity {
    const base_health: f32 = 200;
    const health_from_round = base_health * get_enemy_health_multiplier_for_round(state.level.round);

    return create_entity(state, .{
        .flags = flag_has_health | flag_ai | flag_has_solid_hitbox,
        .position = position,
        .size = .{60, 60},
        .texture = .green,
        .health = @intFromFloat(health_from_round),
        .max_health = @intFromFloat(health_from_round),
        .ai_type = .big,
    });
}

fn create_spawner(state: *State, position: V2f) *Entity {
    return create_entity(state, .{
        .flags = flag_spawner,
        .position = position,
        .size = vscaler(15),
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

fn get_entity_with_flag(state: *State, flag: EntityFlag) ?*Entity {
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

/////////////////////////////////////////////////////////////////////////
//                         @ai
/////////////////////////////////////////////////////////////////////////
const AIType = enum {
    const Self = @This();

    none,
    basic,
    big,

    // centre to centre distance between target entity
    // and the ai entity before it stops and attacks
    fn distance_before_attacking(self: Self) f32 {
        return @as(f32, switch (self) {
            .none => 0,
            .basic => 50,
            .big => 70,
        });
    }

    // how much does the attack box extended from the entity
    fn attack_size(self: Self) f32 {
        return @as(f32, switch (self) {
            .none => 0,
            else => 30,
        });
    }

    fn damage(self: Self) u64 {
        return @as(u64, switch (self) {
            .none => 0,
            .basic => 34,
            .big => 50
        });
    }
};

/////////////////////////////////////////////////////////////////////////
//                         @items
/////////////////////////////////////////////////////////////////////////
const Item = enum {
    const Self = @This();

    none,
    damage_ring,
    player_jr,
    gaurdian,

    fn display_name(self: Self) []const u8 {
        return switch (self) {
            .none => "<empty>",
            .damage_ring => "Damage Ring",
            .player_jr => "Player Jr.",
            .gaurdian => "Gaurdian",
        };
    }

    fn kills_to_complete_level(self: Self, level: u8) u64 {
        return switch (self) {
            .none => 0,
            .damage_ring => switch (level) {
                1 => 50,
                2 => 250,
                else => unreachable
            },
            .player_jr => switch (level) {
                1 => 50,
                2 => 300,
                else => unreachable
            },
            .gaurdian => switch (level) {
                1 => 40,
                2 => 200,
                else => unreachable
            },
        };
    }

    fn cooldown(self: Self, level: u8) f32 {
        return switch (self) {
            .none => 0,
            .damage_ring => switch (level) {
                1 => 1.2,
                2 => 0.95,
                3 => 0.4,
                else => unreachable
            },
            .player_jr => 0, // cooldown is based on player's weapon cooldown
            .gaurdian => switch (level) {
                1 => 1.1,
                2 => 0.9,
                3 => 0.8,
                else => unreachable
            }
        };
    }

    fn damage_ring_damage(level: u8) u64 {
        return switch (level) {
            1 => 40,
            2 => 90,
            3 => 120,
            else => unreachable
        };
    }

    fn damage_ring_radius(level: u8) f32 {
        return switch (level) {
            1 => 85,
            2 => 100,
            3 => 130,
            else => unreachable
        };
    }

    fn gaurdian_damage(level: u8) u64 {
        return switch (level) {
            1 => 100,
            2 => 150,
            3 => 250,
            else => unreachable
        };
    }

    fn gaurdian_projectile_size(level: u8) V2f {
        return switch (level) {
            1 => vscaler(10),
            2 => vscaler(20),
            3 => vscaler(35),
            else => unreachable
        };
    }

    fn gaurdian_projectile_count(level: u8) usize {
        return switch (level) {
            1 => 1,
            2 => 2,
            3 => 5,
            else => unreachable
        };
    }
};

/////////////////////////////////////////////////////////////////////////
///                         @buff
/////////////////////////////////////////////////////////////////////////
const Buff = enum {
    const Self = @This();

    none,
    health,
    speed,
    reload,

    fn base_cost(self: Self) u16 {
        return @as(u16, switch (self) {
            .none => 0,
            .health => 3000,
            .speed => 2500,
            .reload => 2000
        });
    }

    fn entity_texture(self: Self) TextureHandle {
        return switch (self) {
            .none => .none,
            .health => .red,
            .speed => .light_blue,
            .reload => .green
        };
    }

    fn display_name(self: Self) []const u8 {
        return switch (self) {
            .none => "<empty>",
            .health => "Health",
            .speed => "Speed",
            .reload => "Reload"
        };
    }

    fn speed_multipler(level: u8) f32 {
        return switch (level) {
            0 => 1,
            1 => 1.4,
            2 => 1.8,
            3 => 2.2,
            else => unreachable
        };
    }

    fn reload_multiplier(level: u8) f32 {
        return switch (level) {
            0 => 1,
            1 => 0.7,
            2 => 0.45,
            3 => 0.2,
            else => unreachable
        };
    }

    fn health_multiplier(level: u8) f32 {
        return switch (level) {
            0 => 1,
            1 => 1.5,
            2 => 1.75,
            3 => 2,
            else => unreachable
        };
    }
};

/////////////////////////////////////////////////////////////////////////
///                         @weapon
/////////////////////////////////////////////////////////////////////////
const Weapon = enum {
    const Self = @This();

    none,
    pistol,
    splitter,
    shotgun,
    m4,
    smg,

    fn damage(self: Self) u64 {
        return switch (self) {
            .none => 0,
            .pistol => 40,
            .splitter => 70,
            .shotgun => 120,
            .m4 => 100,
            .smg => 65,
        };
    }

    fn magazine_size(self: Self) u16 {
        return switch (self) {
            .none => 0,
            .pistol => 12,
            .splitter => 8,
            .shotgun => 9,
            .m4 => 40,
            .smg => 50,
        };
    }

    fn magazine_count(self: Self) u16 {
        return switch (self) {
            .none => 0,
            .pistol => 4,
            .splitter => 5,
            .shotgun => 6,
            .m4 => 7,
            .smg => 7
        };
    }

    fn reload_cooldown(self: Self) f32 {
        return switch (self) {
            .none => 0,
            .pistol => 1,
            .splitter => 1.5,
            .shotgun => 2,
            .m4 => 1.4,
            .smg => 1,
        };
    }

    fn firing_cooldown(self: Self) f32 {
        return switch (self) {
            .none => 0,
            .pistol => 0.4,
            .splitter => 0.5,
            .shotgun => 0.4,
            .m4 => 0.08,
            .smg => 0.04
        };
    }

    fn display_name(self: Self) []const u8 {
        return switch (self) {
            .none => "<empty>",
            .pistol => "pistol",
            .splitter => "splitter",
            .shotgun => "shotgun",
            .m4 => "m4",
            .smg => "smg",
        };
    }
};

const ProjectileSource = enum {
    none,
    player,
    item
};

/////////////////////////////////////////////////////////////////////////
///                         @vector
/////////////////////////////////////////////////////////////////////////
pub const V2f = @Vector(2, f32);
pub const V2i = @Vector(2, i32);

pub fn vscaler(scaler: f32) V2f {
    return @splat(scaler);
}

pub fn vlength(vector: V2f) f32 {
    return @sqrt(@reduce(.Add, vector * vector));
}

pub fn vdistance(source: V2f, destination: V2f) V2f {
    return source - destination;
}

pub fn vdistance_abs(source: V2f, destination: V2f) V2f {
    return @abs(source - destination);
}

pub fn vnormalise(vector: V2f) V2f {
    const length = vlength(vector);
    if(length == 0) {
        return V2f{0, 0};
    }

    return vector / vscaler(length);
}

pub fn rotate_normalised_vector(vector: V2f, degrees: f32) V2f {
    const radians = std.math.degreesToRadians(degrees);

    const s = @sin(radians);
    const c = @cos(radians);

    const x = (vector[0] * c) - (vector[1] * s);
    const y = (vector[0] * s) + (vector[1] * c);

    return V2f{x, y};
}

fn orientation(p: V2f, q: V2f, r: V2f) enum {collinear, clockwise, counter_clockwise} 
{
    const val = (q[1] - p[1]) * (r[0] - q[0]) - (q[0] - p[0]) * (r[1] - q[1]);
    if (val == 0) {
        return .collinear;
    } else if (val > 0) {
        return .clockwise;
    } else {
        return .counter_clockwise;
    }
}

fn point_on_line(p: V2f, q: V2f, r: V2f) bool {
    return q[0] >= @min(p[0], r[0]) and q[0] <= @max(p[0], r[0]) and
           q[1] >= @min(p[1], r[1]) and q[1] <= @max(p[1], r[1]);
}

fn lines_intersect(
    v1_start: V2f,
    v1_end: V2f,
    v2_start: V2f,
    v2_end: V2f
) bool {
    const o1 = orientation(v1_start, v1_end, v2_start);
    const o2 = orientation(v1_start, v1_end, v2_end);
    const o3 = orientation(v2_start, v2_end, v1_start);
    const o4 = orientation(v2_start, v2_end, v1_end);

    // General case
    if (o1 != o2 and o3 != o4) {
        return true;
    }

    // Special cases
    if (o1 == .collinear and point_on_line(v1_start, v2_start, v1_end)) return true;
    if (o2 == .collinear and point_on_line(v1_start, v2_end, v1_end)) return true;
    if (o3 == .collinear and point_on_line(v2_start, v1_start, v2_end)) return true;
    if (o4 == .collinear and point_on_line(v2_start, v1_end, v2_end)) return true;

    return false; // Otherwise, no intersection
}

/////////////////////////////////////////////////////////////////////////
///                         @random
/////////////////////////////////////////////////////////////////////////
fn init_raylib() void {
    raylib.InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "shoot shoot game");
    raylib.SetTargetFPS(240);
    raylib.SetTextLineSpacing(render.FONT_LINE_SPACING);
}

pub fn key(state: *const State, k: c_int) State.InputState {
    return state.keyboard[@intCast(k)];
}

pub fn mouse(state: *const State, m: c_int) State.InputState {
    return state.mouse[@intCast(m)];
}

fn get_enemy_count_for_round(round: u64) u64 {
    if(DEBUG_ONE_ENEMY_PER_ROUND) {
        return 1;
    }

    // growing but capped
    if(false) {
        return 6 + @as(u64, switch (round) {
            1...5 => round * 2,
            6 => 24,
            7 => 30,
            8 => 38,
            9 => 48,
            10 => 60,
            11 => 75,
            12 => 95,
            13 => 120,
            15 => 150,
            else => 175,
        });
    }

    // exponetial 
    if(true) {
        return 6 + @as(u64, switch (round) {
            1...6 => round * 2,
            else => @intFromFloat(std.math.pow(f32, @floatFromInt(round), 1.8)),
        });
    }
}

fn get_enemy_speed_multiplier_for_round(round: u64) f32 {
    return switch (round) {
        1, 2 => 0.45,
        3...5 => 0.6,
        6...9 => 0.75,
        10...15 => 0.9,
        else => 1.0
    };
}

fn get_enemy_health_multiplier_for_round(round: u64) f32 {
    std.debug.assert(round > 0);

    // exponential health
    if(false) {
        const r = @as(f32, @floatFromInt(round - 1)) * 0.25;
        return 1 + (std.math.pow(f32, r, 1.6));
    } 

    // growing but capped health
    if(true) {
        return 1.0 + @as(f32, switch (round) {
            1...3 => 0,
            4...5 => 0.5,
            6...8 => 1,
            9...13 => 1.5,
            14...20 => 2,
            21...29 => 2.5,
            else => 3
        });
    }

    // same health
    if(false) {
        return 1;
    }
}

fn get_spawner_cooldown_for_round(round: u64) f32 {
    return switch (round) {
        1...3 => 4,
        4...6 => 3,
        7...10 => 2,
        11...14 => 1,
        15...19 => 0.75,
        else => @as(f32, 0.5)
    };
}

fn get_varied_spawner_cooldown_for_round(state: *State, round: u64) f32 {
    const base_delay = get_spawner_cooldown_for_round(round);
    return base_delay + (base_delay * state.rng.random().float(f32));
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

fn draw_value_microui(state: *State, args: anytype) void {
    const ArgsType = @TypeOf(args);
    const args_type_info = @typeInfo(ArgsType);
    if (args_type_info != .Struct) {
        @compileError("expected tuple or struct argument, found " ++ @typeName(ArgsType));
    }

    const fields_info = args_type_info.Struct.fields;
    inline for(fields_info) |field_info| {
        const string = std.fmt.allocPrintZ(state.frame_allocator.allocator(), "{s}:{any}", .{field_info.name, @field(args, field_info.name)}) catch unreachable;
        const raylib_font = raylib.GetFontDefault();
        const dimensions = raylib.MeasureTextEx(raylib_font, string, MICROUI_FONT_SIZE, render.FONT_LINE_SPACING);

        const width: c_int = @intFromFloat(dimensions.x);
        const height: c_int = @intFromFloat(dimensions.y);

        microui.mu_layout_row(state.microui_context, 1, &width, height);
        microui.mu_text(state.microui_context, string);
    }
}

fn draw_microui_command_list(state: *State) void {
    var command: [*c]microui.mu_Command = 0;
    while(microui.mu_next_command(state.microui_context, &command) != 0) { // **mu_command here
        switch (command.*.type) {
            microui.MU_COMMAND_RECT => {
                const rect = &command.*.rect.rect;
                raylib.DrawRectangle(
                    rect.x, rect.y, 
                    rect.w, rect.h, 
                    microui_color_to_raylib(command.*.rect.color)
                );
            },
            microui.MU_COMMAND_TEXT => {
                const text_as_ptr: [*]u8 = @ptrCast(&command.*.text.str);
                raylib.DrawText(text_as_ptr, command.*.text.pos.x, command.*.text.pos.y, MICROUI_FONT_SIZE, microui_color_to_raylib(command.*.text.color));
            },
            microui.MU_COMMAND_CLIP => {
                const rect = &command.*.clip.rect;
                raylib.BeginScissorMode(rect.x, rect.y, rect.w, rect.h);
            },
            microui.MU_COMMAND_ICON => {
                // not supported :[
            },
            else => {},
        }
    }
    
    raylib.EndScissorMode();
}

fn microui_text_width_callback(font: microui.mu_Font, text: [*c]const u8, length: c_int) callconv(.C) c_int {
    _ = font; // autofix
    _ = length; // autofix

    const raylib_font = raylib.GetFontDefault();
    const dimensions = raylib.MeasureTextEx(raylib_font, text, MICROUI_FONT_SIZE, render.FONT_LINE_SPACING);
    return @intFromFloat(dimensions.x);
}

fn microui_text_height_callback(font: microui.mu_Font) callconv(.C) c_int {
    _ = font; // autofix
    const raylib_font = raylib.GetFontDefault();
    const dimensions = raylib.MeasureTextEx(raylib_font, "random text", MICROUI_FONT_SIZE, render.FONT_LINE_SPACING);
    return @intFromFloat(dimensions.y);
}

fn microui_color_to_raylib(color: microui.mu_Color) raylib.Color {
    return raylib.Color{
        .r = color.r,
        .g = color.g,
        .b = color.b,
        .a = color.a,
    };
}

/////////////////////////////////////////////////////////////////////////
//                         @level creation
/////////////////////////////////////////////////////////////////////////
const DoorOptions = struct {
    left: bool = false,
    top: bool = false,
    right: bool = false,
    bottom: bool = false,

    top_cost: u16 = 0,
    right_cost: u16 = 0
};


fn create_room(state: *State, x_index: i64, y_index: i64, door_options: DoorOptions) V2f {
    const centre = V2f{ROOM_WIDTH * @as(f32, @floatFromInt(x_index)), ROOM_HEIGHT * @as(f32, @floatFromInt(y_index))};

    // top
    var top_position = centre + V2f{0, (-ROOM_HEIGHT * 0.5) + WALLTHICKNESS * 0.5};
    var top_size = V2f{ROOM_WIDTH, WALLTHICKNESS};

    // bottom
    var bottom_position = centre + V2f{0, (ROOM_HEIGHT * 0.5) - WALLTHICKNESS * 0.5};
    var bottom_size = V2f{ROOM_WIDTH, WALLTHICKNESS};

    // left
    var left_position = centre + V2f{(-ROOM_WIDTH * 0.5) + WALLTHICKNESS * 0.5, 0};
    var left_size = V2f{WALLTHICKNESS, ROOM_HEIGHT};

    // right
    var right_position = centre + V2f{(ROOM_WIDTH * 0.5) - WALLTHICKNESS * 0.5, 0};
    var right_size = V2f{WALLTHICKNESS, ROOM_HEIGHT};

    { // create doors, done before wall size changes to use those as referance
        // only need to create the top and right door, other sides are handled by other rooms 
        // that will have their respected side open and have a door
        if(door_options.top) {
            std.debug.assert(door_options.top_cost > 0); // forgot to set the cost of the door...

            const door_position = V2f{top_position[0] + (ROOM_WIDTH * 0.5) - (DOOR_WIDTH * 0.5), top_position[1]};
            const door_size = V2f{DOOR_WIDTH, WALLTHICKNESS};
            _ = create_door(state, door_position, door_size, door_options.top_cost);
        }

        if(door_options.right) {
            std.debug.assert(door_options.right_cost > 0); // forgot to set the cost of the door...

            const door_position = V2f{right_position[0], right_position[1] + (ROOM_HEIGHT * 0.5) - (DOOR_WIDTH * 0.5)};
            const door_size = V2f{WALLTHICKNESS, DOOR_WIDTH};
            _ = create_door(state, door_position, door_size, door_options.right_cost);
        }
    }

    { // adjust wall size based on doors
        if(door_options.top) {
            top_size -= V2f{DOOR_WIDTH, 0}; 
            top_position -= V2f{DOOR_WIDTH, 0} * vscaler(0.5);
        }

        if(door_options.bottom) {
            bottom_size -= V2f{DOOR_WIDTH, 0}; 
            bottom_position -= V2f{DOOR_WIDTH, 0} * vscaler(0.5);
        }

        if(door_options.left) {
            left_size -= V2f{0, DOOR_WIDTH}; 
            left_position -= V2f{0, DOOR_WIDTH} * vscaler(0.5);
        }

        if(door_options.right) {
            right_size -= V2f{0, DOOR_WIDTH}; 
            right_position -= V2f{0, DOOR_WIDTH} * vscaler(0.5);
        }
    }

    { // create 4 surrounding walls
        _ = create_wall(state, top_position, top_size);
        _ = create_wall(state, bottom_position, bottom_size);
        _ = create_wall(state, left_position, left_size);
        _ = create_wall(state, right_position, right_size);
    } 

    return centre;
}

fn create_scene(state: *State) void {
    const room_object_offset = V2f{ROOM_WIDTH * 0.45, ROOM_HEIGHT * 0.45};
    {
        const centre = vscaler(0);

        _ = create_player(state, centre + room_object_offset);
    }

    { // spawn room
        const centre = create_room(state, 0, 0, .{.top = true, .top_cost = 400});

        _ = create_spawner(state, centre + V2f{-room_object_offset[0], 0});     // left middle
        _ = create_spawner(state, centre + V2f{room_object_offset[0], 0});      // right middle
        _ = create_spawner(state, centre + V2f{0, -room_object_offset[1]});     // top middle
        _ = create_spawner(state, centre + V2f{0, room_object_offset[1]});      // bottom middle

        _ = create_wall(state, centre, V2f{350, 25});
    }

    { // main room
        const centre = create_room(state, 0, -1, .{.bottom = true, .right = true, .left = true, .top = true, .top_cost = 1250, .right_cost = 800});

        _ = create_wall(state, centre, vscaler(300));
        
        _ = create_spawner(state, centre + V2f{room_object_offset[0], 0});      // right middle
        _ = create_spawner(state, centre + V2f{0, -room_object_offset[1]});     // top middle
        _ = create_spawner(state, centre + room_object_offset * vscaler(-1));   // top left
        _ = create_spawner(state, centre + V2f{room_object_offset[0], -room_object_offset[1]});   // top right
                                                                                                  
        _ = create_buff_buy(state, centre + V2f{0, room_object_offset[1]}, .reload);      // bottom middle
        _ = create_weapon_buy(state, centre + V2f{0, -190}, .splitter, 900);
    }

    { // ammo room
        const centre = create_room(state, 1, -1, .{.left = true, .right = true, .right_cost = 1250});

        _ = create_spawner(state, centre + V2f{0, -room_object_offset[1]});     // top middle
        _ = create_spawner(state, centre + room_object_offset * vscaler(-1));   // top left
        _ = create_spawner(state, centre + V2f{room_object_offset[0], -room_object_offset[1]});   // top right
        _ = create_spawner(state, centre + V2f{room_object_offset[0], room_object_offset[1]}); // bottom right
        _ = create_spawner(state, centre + V2f{0, room_object_offset[1]});      // bottom middle
        _ = create_spawner(state, centre + V2f{-room_object_offset[0], 0});     // left middle
        
        _ = create_ammo_crate(state, centre + V2f{room_object_offset[0], 0});
    }

    { // box room
        const centre = create_room(state, 2, -1, .{.left = true, .bottom = true});

        _ = create_spawner(state, centre + V2f{0, -room_object_offset[1]});     // top middle
        _ = create_spawner(state, centre + room_object_offset * vscaler(-1));   // top left
        _ = create_spawner(state, centre + V2f{room_object_offset[0], -room_object_offset[1]});   // top right
        _ = create_spawner(state, centre + V2f{room_object_offset[0], room_object_offset[1]}); // bottom right
        _ = create_spawner(state, centre + V2f{0, room_object_offset[1]});      // bottom middle
        
        _ = create_random_box(state, centre);
    }

    { // gaurdian room
        const centre = create_room(state, 2, 0, .{.top = true, .top_cost = 1500});

        _ = create_spawner(state, centre + V2f{0, -room_object_offset[1]});     // top middle
        _ = create_spawner(state, centre + room_object_offset * vscaler(-1));   // top left
        _ = create_spawner(state, centre + V2f{-room_object_offset[0], room_object_offset[1]});      // bottom left
        _ = create_spawner(state, centre + V2f{room_object_offset[0], room_object_offset[1]}); // bottom right
        _ = create_spawner(state, centre + V2f{0, room_object_offset[1]});      // bottom middle
        
        _ = create_item(state, centre, .gaurdian, 1, 0);
    }

    { // health buff room
        const centre = create_room(state, -1, -1, .{.left = true, .right = true, .right_cost = 1500});

        _ = create_spawner(state, centre + V2f{0, -room_object_offset[1]});     // top middle
        _ = create_spawner(state, centre + room_object_offset * vscaler(-1));   // top left
        _ = create_spawner(state, centre + V2f{room_object_offset[0], -room_object_offset[1]});   // top right
        _ = create_spawner(state, centre + V2f{room_object_offset[0], room_object_offset[1]}); // bottom right
        _ = create_spawner(state, centre + V2f{0, room_object_offset[1]});      // bottom middle
        _ = create_spawner(state, centre + V2f{-room_object_offset[0], room_object_offset[1]});      // bottom left

        
        _ = create_buff_buy(state, centre + V2f{-room_object_offset[0], 0}, .health);     // left middle
    }

    { // player jr room
        const centre = create_room(state, -2, -1, .{.right = true, .right_cost = 2000});

        _ = create_spawner(state, centre + V2f{0, -room_object_offset[1]});     // top middle
        _ = create_spawner(state, centre + room_object_offset * vscaler(-1));   // top left
        _ = create_spawner(state, centre + V2f{room_object_offset[0], -room_object_offset[1]});   // top right
        _ = create_spawner(state, centre + V2f{0, room_object_offset[1]});      // bottom middle
        _ = create_spawner(state, centre + V2f{-room_object_offset[0], room_object_offset[1]});      // bottom left
        _ = create_spawner(state, centre + V2f{-room_object_offset[0], 0});     // left middle


        
        _ = create_item(state, centre, .player_jr, 1, 0);
    }

    { // speed buff room
        const centre = create_room(state, 0, -2, .{.bottom = true, .top = true, .top_cost =  2000});

        _ = create_spawner(state, centre + room_object_offset * vscaler(-1));   // top left
        _ = create_spawner(state, centre + V2f{0, room_object_offset[1]});      // bottom middle
        _ = create_spawner(state, centre + V2f{-room_object_offset[0], room_object_offset[1]});      // bottom left
        _ = create_spawner(state, centre + V2f{-room_object_offset[0], 0});     // left middle

        
        _ = create_buff_buy(state, centre + V2f{0, -room_object_offset[1]}, .speed);     // top middle
    }

    { // damage room
        const centre = create_room(state, 0, -3, .{.bottom = true});

        _ = create_spawner(state, centre + room_object_offset * vscaler(-1));   // top left
        _ = create_spawner(state, centre + V2f{room_object_offset[0], -room_object_offset[1]});   // top right
        _ = create_spawner(state, centre + V2f{0, room_object_offset[1]});      // bottom middle
        _ = create_spawner(state, centre + V2f{-room_object_offset[0], room_object_offset[1]});      // bottom left

        
        _ = create_item(state, centre, .damage_ring, 1, 0);
    }
}

/////////////////////////////////////////////////////////////////////////
//                         @main
/////////////////////////////////////////////////////////////////////////
pub fn main() !void {
    init_raylib();

    log.info("{} {}", .{@sizeOf(Entity), @sizeOf(Entity) * MAX_ENTITIES});

    for(1..30) |round| {
        log.info(
            "round: {:<3}, healthX: {d:<4}, spawnDelay: {d:<4}, enemy#: {:<5}, speedX: {d:<5}", .{
            round,
            get_enemy_health_multiplier_for_round(round), 
            get_spawner_cooldown_for_round(round),
            get_enemy_count_for_round(round),
            get_enemy_speed_multiplier_for_round(round)
        });
    }
    
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();

    const allocator = arena.allocator();
    var state = new_state(allocator);

    create_scene(&state); 

    run(&state);
}

/////////////////////////////////////////////////////////////////////////
//                          @todo 
/////////////////////////////////////////////////////////////////////////

// PITCH
// This is a top down horde shooter about survivaing continuosly
// more difficult waves of enemies
//
// You kill enemies and open rooms in each level to aquire and upgrade
// new weapons and abilities
//
// Goal
// beginer rounds (1 -> ~5):
// - let player save some money to start opening the map
// - weapons go from super effective to in need of an upgrade, to encourage exploration
// - going from killing enemies one at a time to training
// - starting room becomes to difficult
//
// open rounds (~6 -> 15)
// - level is now needed to be opened up
// - upgrades on guns are in progress
// - enemy numbers start to grow in large hordes
